/*
 * Copyright 2009 Sun Microsystems, Inc.  
 * Copyright 2015 Marcelo Araujo <araujo@FreeBSD.org>.
 * All rights reserved.
 *
 * Use is subject to license terms.
 */

/*
 * BSD 3 Clause License
 *
 * Copyright (c) 2007, The Storage Networking Industry Association.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 	- Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 * 	- Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in
 *	  the documentation and/or other materials provided with the
 *	  distribution.
 *
 *	- Neither the name of The Storage Networking Industry Association (SNIA)
 *	  nor the names of its contributors may be used to endorse or promote
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>

/* getopt */
#include <unistd.h>

#include <ndmpd.h>
#include <handler.h>
#include <ndmpd_session.h>
#include <ndmpd_util.h>
#include <ndmpd_func.h>
#include <ndmpd_table.h>
#include <ndmpd_prop.h>

#include <signal.h>
#include <assert.h>

/* for print log function */
#include <stdarg.h>

extern void ndmpd_mover_cleanup(ndmpd_session_t *session);

extern ndmp_connection_t *ndmp_create_xdr_connection(void);
extern void ndmp_destroy_xdr_connection(ndmp_connection_t *);
extern void *ndmp_malloc(size_t size);

/* in ndmpd_func */
extern int tcp_accept(int listen_sock, unsigned int *inaddr_p);
extern int ndmp_get_fd(ndmp_connection_t *connection_handle);
extern int ndmpd_remove_file_handler(ndmpd_session_t *session, int fd);
extern int ndmpd_select(ndmpd_session_t *session, bool_t block, u_long class_mask);
extern int ndmp_ver;

/* defined in hist */
extern void ndmpd_file_history_init(ndmpd_session_t *session);
extern void ndmpd_file_history_cleanup(ndmpd_session_t *session, bool_t send_flag);

extern void ndmpd_mover_shut_down(ndmpd_session_t *session);

int PRINT_DEBUG_LOG;

void
ndmpd_log(int level, const char *fmt,...)
{
	va_list arg;
	va_start(arg, fmt);
	if (PRINT_DEBUG_LOG){
		vfprintf(stderr, fmt, arg);
		fprintf(stderr, "\n");
	}else{
		if(level!=LOG_DEBUG){
			vfprintf(stderr, fmt, arg);
			fprintf(stderr, "\n");
		}
	}
	va_end(arg);
}

/*
 * ndmpd_worker thread
 *
 * Parameters:
 *   argp (input) - structure containing socket and handler function
 *
 * Returns:
 *   0 - successful connection.
 *  -1 - error.
 */
void *
ndmpd_worker(void *ptarg)
{
	int sock;
	ndmp_connection_t *connection;
	ndmpd_worker_arg_t *argp = (ndmpd_worker_arg_t *)ptarg;

	if (!argp)
		return ((void *)-1);

	sock = argp->nw_sock;

	if ((connection = ndmp_create_xdr_connection()) == NULL) {
		(void) close(sock);
		free(argp);
		return (NULL);
	}


	((ndmp_connection_t *)connection)->conn_sock = sock;
	(*argp->nw_con_handler_func)(connection);


	ndmp_destroy_xdr_connection(connection);

	(void) close(sock);
	free(argp);
	pthread_exit(NULL);
	return (NULL);
}


/*
 * Creates a socket for listening and accepting connections
 * from NDMP clients.
 *
 * Parameters:
 *   port (input)   -  NDMP server port.
 *   handler (input) - connection handler function.
 *
 * Returns:
 *   This function normally never returns unless there's error.
 *   -1 : error
 */
int
ndmp_run(u_long port, ndmp_con_handler_func_t con_handler_func)
{
	int ns;
	int on, tmp;
	int server_socket;
	unsigned int ipaddr;
	struct sockaddr_in sin;
	int flag = 1;
	ndmpd_worker_arg_t *argp;
	char *listenIP;

	(void) memset((void *) &sin, 0, sizeof (sin));
	sin.sin_family = AF_INET;

	listenIP = getIPfromNIC(ndmpd_get_prop(NDMP_LISTEN_NIC));
	printf("listen on IP: %s\n",listenIP);
	if(strcmp(ndmpd_get_prop(NDMP_SERVE_NIC),"")==0)
		printf(" Serve on IP: %s\n",getIPfromNIC(ndmpd_get_prop(NDMP_LISTEN_NIC)));
	else
		printf(" Serve on IP: %s\n",getIPfromNIC(ndmpd_get_prop(NDMP_SERVE_NIC)));

	sin.sin_addr.s_addr = inet_addr(listenIP);
	sin.sin_port = htons(port);
	
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ndmpd_log(LOG_ERR, "Socket error: %m");
		return (-1);
	}

	on = 1;
	(void) setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
	    (char *)&on, sizeof (on));

	if (bind(server_socket, (struct sockaddr *)&sin, sizeof (sin)) < 0) {
		ndmpd_log(LOG_ERR, "bind error: %m");
		(void) close(server_socket);
		return (-1);
	}
	if (listen(server_socket, 50) < 0) {
		ndmpd_log(LOG_ERR, "listen error: %m");
		(void) close(server_socket);
		return (-1);
	}
	
	signal(SIGCHLD, SIG_IGN); // <-- ignore child fate, don't let it become zombie
	pid_t childPID;
	for (;;) {
		if ((ns = tcp_accept(server_socket, &ipaddr)) < 0) {
			ndmpd_log(LOG_ERR, "tcp_accept error: %m");
			exit(1);
		}

		/*
		 *  fork process here.
		 *  use process can save a lot of trouble.
		 *
		 * */
		childPID = fork();
		if(childPID >= 0) // fork was successful
		{
			if(childPID == 0) {
				close(server_socket);

				ndmpd_log(LOG_DEBUG, "connection fd: %d, got socket, start to process", ns);
				ndmp_set_socket_nodelay(ns);
				(void) setsockopt(ns, SOL_SOCKET, SO_KEEPALIVE, &flag,
					sizeof (flag));

				if ((argp = ndmp_malloc(sizeof (ndmpd_worker_arg_t))) != NULL) {
					argp->nw_sock = ns;
					argp->nw_ipaddr = ipaddr;
					/*	assign handler function 	*/
					argp->nw_con_handler_func = con_handler_func;
					ndmpd_worker(argp);
				}
				else
					close(ns);
			}
		}
	}/* end of listening */
	return 0;
}

/*
 * connection_handler
 * 
 *
 * NDMP connection handler. main entry for ndmpd
 * Waits for, reads, and processes NDMP requests on a connection.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *
 * Return:
 *   void
 */
void
connection_handler(ndmp_connection_t *connection)
{
	ndmpd_log(LOG_DEBUG, " - connection_handler: handle the connection START - %d",
						connection->conn_sock);
	static int conn_id = 1;
	ndmpd_session_t session;
	ndmp_notify_connected_request req;
	int connection_fd;
	
	(void) memset(&session, 0, sizeof (session));
	session.ns_connection = connection;
	session.ns_eof = FALSE;
	/*
	 * The 'protocol_version' must be 1 at first, since the client talks
	 * to the server in version 1 then they can move to a higher
	 * protocol version.
	 */
	session.ns_protocol_version = ndmp_ver;
	session.ns_file_handler_list = 0;

	// no malloc inside
	(void) ndmpd_data_init(&session);

	ndmpd_file_history_init(&session);

	 if (ndmpd_mover_init(&session) < 0)
		 return;

	if (ndmp_lbr_init(&session) < 0) {
		ndmpd_mover_cleanup(&session);
		return;
	}

	/*
	 * Setup defaults here. The init functions can not set defaults
	 * since the init functions are called by the stop request handlers
	 * and client set variables need to persist across data operations.
	 */
	session.ns_mover.md_record_size = MAX_RECORD_SIZE;

	ndmp_set_client_data(connection, (void *)&session);

	req.reason = NDMP_CONNECTED;
	req.protocol_version = ndmp_ver;
	req.text_reason = "";

	/* Send request to tell the client that we're ready for connection */
	if (ndmp_send_request_lock(connection, 
			NDMP_NOTIFY_CONNECTION_STATUS,
			NDMP_NO_ERR, (void *)&req, 0) == NDMP_PROC_ERR) {
			
		ndmpd_log(LOG_DEBUG, "Connection terminated");

		ndmp_lbr_cleanup(&session);
		ndmpd_mover_cleanup(&session);

		return;
	}
	connection_fd = ndmp_get_fd(connection);

	/*
	 * Add the handler function for the connection. This is the main 
	 * function to retain the session and keep it running on the
	 * background.
	 */
	if (ndmpd_add_file_handler(&session, (void *)&session, connection_fd,
		NDMPD_SELECT_MODE_READ, HC_CLIENT, connection_file_handler) != 0) {
		ndmpd_log(LOG_DEBUG, "Could not register session handler.");
		ndmp_lbr_cleanup(&session);
		ndmpd_mover_cleanup(&session);
		return;
	}

	/*
	 * Register the connection in the list of active connections.
	 * every connection will increment conn_id by 1.
	 */
	if (ndmp_connect_list_add(connection, &conn_id) != 0) {
		ndmpd_log(LOG_ERR,
		    "Could not register the session to the server.");
		(void) ndmpd_remove_file_handler(&session, connection_fd);
		ndmp_lbr_cleanup(&session);
		ndmpd_mover_cleanup(&session);
		return;
	}

	session.hardlink_q = hardlink_q_init();

	/*
	 * wait for connection_file_hanlder to be finished.
	 *
	 */
	while (session.ns_eof == FALSE) {
		if(ndmpd_select(&session, TRUE, HC_ALL) < 0)
			break; // connection broken. terminate this process.
	}

	hardlink_q_cleanup(session.hardlink_q);

	ndmpd_log(LOG_DEBUG, "Connection terminated");

	(void) ndmpd_remove_file_handler(&session, connection_fd);

	 ndmpd_mover_shut_down(&session);
	 ndmp_lbr_cleanup(&session);
	 ndmpd_data_cleanup(&session);
	 ndmpd_file_history_cleanup(&session, FALSE);
	 ndmpd_mover_cleanup(&session);

	 (void) ndmp_connect_list_del(connection);

	 ndmpd_log(LOG_DEBUG, "- connection_handler: handle the connection END - %d",connection_fd);
}

int
startNDMPD()
{	
	return ndmp_run(NDMPPORT, connection_handler);
}


bool_t
file_exists(const char * filename)
{
	FILE  *file;

    	if ((file = fopen(filename, "r"))) {
        	fclose(file);
        	return TRUE;
    	}

    	return FALSE;
}

int 
main(int argc, char *argv[])
{
	char c;
	struct stat st = {0};
	char *configFile="/usr/local/etc/ndmpd.conf";

	if (stat("/var/log/ndmp", &st) == -1) {
		mkdir("/var/log/ndmp", 0755);
		fprintf(stderr, "/var/log/ndmp created.\n");
	}

	PRINT_DEBUG_LOG=0;
	while ((c = getopt(argc, argv, "df:")) != -1) {
		switch (c) {
		case 'f':
			printf("using configuration file: %s\n",argv[2]);
			configFile=argv[2];
			break;
		case 'd':
			printf("running in debug mode\n");
			PRINT_DEBUG_LOG=1;
			break;
		default:
			printf("%s: Invalid option -%c.\n",argv[0], optopt);
			return(1);
		}
	}

	/* check if /usr/local/etc/ndmpd.conf exists */
	if (stat(configFile, &st) == -1) {
		fprintf(stderr, "configuration file \"%s\" does not exist.\n",configFile);
		exit(1);
	}

	/* load ENVs */
	if (ndmpd_load_prop(configFile)) {
		fprintf(stderr, "NDMP properties initialization failed.\n");
		exit(1);
	}
	ndmp_load_params();

	startNDMPD();
	
	return 0;
}


