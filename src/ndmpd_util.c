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
 *      - Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in
 *        the documentation and/or other materials provided with the
 *        distribution.
 *
 *      - Neither the name of The Storage Networking Industry Association (SNIA)
 *        nor the names of its contributors may be used to endorse or promote
 *        products derived from this software without specific prior written
 *        permission.
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

#include <ndmpd_util.h>
#include <ndmpd.h>
#include <ndmpd_func.h>
#include <ndmpd_session.h>
#include <ndmpd_prop.h>
#include <ndmpd_table.h>

#include <handler.h>

#include <sys/types.h>
#include <sys/socket.h>

/*	snprintf	*/
#include <stdio.h>

/* used in getIPFromNIC*/
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>

#include <tlm_buffers.h>
#include <tlm_util.h>
#include <tlm_lib.h>

#include <ndmpd_tar_v3.h>

/*
 * Mutex to protect Nlp
 */
mutex_t nlp_mtx;


/*
 * Force to backup all the intermediate directories leading to an object
 * to be backed up in 'dump' format backup.
 */
static bool_t ndmp_dump_path_node = FALSE;


/*
 * Force to backup all the intermediate directories leading to an object
 * to be backed up in 'tar' format backup.
 */
static bool_t ndmp_tar_path_node = FALSE;


/*
 * Should the 'st_ctime' be ignored during incremental level backup?
 */
bool_t ndmp_ignore_ctime = FALSE;

/*
 * Should the 'st_lmtime' be included during incremental level backup?
 */
bool_t ndmp_include_lmtime = FALSE;

/*
 * Force to send the file history node entries along with the file history
 * dir entries for all directories containing the changed files to the client
 * for incremental backup.
 *
 * Note: This variable is added to support Bakbone Software's Netvault DMA
 * which expects to get the FH ADD NODES for all upper directories which
 * contain the changed files in incremental backup along with the FH ADD DIRS.
 */
static bool_t ndmp_fhinode = FALSE;

/*
 * Force backup directories in incremental backups.  If the
 * directory is not modified itself, it's not backed up by
 * default.
 */
extern int ndmp_force_bk_dirs;
int ndmp_force_bk_dirs  = 1;

/*
 * List of things to be exluded from backup.
 */
static char *exls[] = {
	(char *)EXCL_PROC,
	(char *)EXCL_TMP,
	NULL, /* reserved for a copy of the "backup.directory" */
	NULL
};

/*
 * ndmpd_add_file_handler
 *
 * Adds a file handler to the file handler list.
 * The file handler list is used by ndmpd_api_dispatch.
 *
 * Parameters:
 *   session (input) - session pointer.
 *   cookie  (input) - opaque data to be passed to file hander when called.
 *   fd      (input) - file descriptor.
 *   mode    (input) - bitmask of the following:
 *		     1 = watch file for ready for reading
 *		     2 = watch file for ready for writing
 *		     4 = watch file for exception
 *   class   (input) - handler class. (HC_CLIENT, HC_MOVER, HC_MODULE)
 *   func    (input) - function to call when the file meets one of the
 *		     conditions specified by mode.
 *
 * Returns:
 *   0 - success.
 *  -1 - error.
 */
int
ndmpd_add_file_handler(ndmpd_session_t *session, void *cookie, int fd,
    u_long mode, u_long class, ndmpd_file_handler_func_t *func)
{
	ndmpd_file_handler_t *new;

	new = ndmp_malloc(sizeof (ndmpd_file_handler_t));
	if (new == 0)
		return (-1);

	ndmpd_log(LOG_DEBUG, "ndmpd_add_file_handler, fd=%d",fd);

	new->fh_cookie = cookie;
	new->fh_fd = fd;
	new->fh_mode = mode;
	new->fh_class = class;
	new->fh_func = func;
	new->fh_next = session->ns_file_handler_list;
	session->ns_file_handler_list = new;

	return (0);
}

/*
 * ndmpd_remove_file_handler
 *
 * Removes a file handler from the file handler list.
 *
 * Parameters:
 *   session (input) - session pointer.
 *   fd      (input) - file descriptor.
 *
 * Returns:
 *   0 - success.
 *  -1 - error.
 */
int
ndmpd_remove_file_handler(ndmpd_session_t *session, int fd)
{
	ndmpd_file_handler_t **last;
	ndmpd_file_handler_t *handler;
	ndmpd_log(LOG_DEBUG, "ndmpd_remove_file_handler, fd=%d",fd);
	last = &session->ns_file_handler_list;
	while (*last != 0) {
		handler = *last;
		if (handler->fh_fd == fd) {
			*last = handler->fh_next;
			(void) free(handler);
			return (1);
		}
		last = &handler->fh_next;
	}

	return (0);
}

/*
 * ndmp_connection_closed
 *
 * If the connection closed or not.
 *
 * Parameters:
 *   fd (input) : file descriptor
 *
 * Returns:
 *   0  - connection is still valid
 *   1  - connection is not valid anymore
 *   -1 - Internal kernel error
 */
int
ndmp_connection_closed(int fd)
{
	fd_set fds;
	int closed, ret;
	struct timeval timeout;

	if (fd < 0) /* We are not using the mover */
		return (-1);

	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	ret = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
	closed = (ret == -1 && errno == EBADF);

	return (closed);
}

/*
 * ndmp_check_mover_state
 *
 * Checks the mover connection status and sends an appropriate
 * NDMP message to client based on that.
 *
 * Parameters:
 *   ndmpd_session_t *session (input) : session pointer
 *
 * Returns:
 *   void.
 */
void
ndmp_check_mover_state(ndmpd_session_t *session)
{
	int moverfd;
	/*
	 * NDMPV3 Spec (Three-way restore):
	 * Once all of the files have been recovered, NDMP DATA Server closes
	 * the connection to the mover on the NDMP TAPE Server. THEN
	 * The NDMP client should receive an NDMP_NOTIFY_MOVER_HALTED message
	 * with an NDMP_MOVER_CONNECT_CLOSED reason from the NDMP TAPE Server
	 */
	moverfd = session->ns_mover.md_sock;
	/* If connection is closed by the peer */
	if (moverfd >= 0 &&
	    session->ns_mover.md_mode == NDMP_MOVER_MODE_WRITE) {
		int closed, reason;
		closed = ndmp_connection_closed(moverfd);
		if (closed) {
			/* Connection closed or internal error */
			if (closed > 0) {
				ndmpd_log(LOG_DEBUG,
				    "ndmp mover: connection closed by peer");
				reason = NDMP_MOVER_HALT_CONNECT_CLOSED;
			} else {
				ndmpd_log(LOG_DEBUG,
				    "ndmp mover: Internal error");
				reason = NDMP_MOVER_HALT_INTERNAL_ERROR;
			}
			ndmpd_mover_error(session, reason);

		}
	}
}

/*
 * ndmpd_select
 *
 * Calls select on the the set of file descriptors from the
 * file handler list masked by the fd_class argument.
 * Calls the file handler function for each
 * file descriptor that is ready for I/O.
 *
 * Parameters:
 *   session (input) - session pointer.
 *   block   (input) - if TRUE, ndmpd_select waits until at least one
 *		     file descriptor is ready for I/O. Otherwise,
 *		     it returns immediately if no file descriptors are
 *		     ready for I/O.
 *   class_mask (input) - bit mask of handler classes to be examined.
 *		     Provides for excluding some of the handlers from
 *		     being called.
 *
 * Returns:
 *  -1 - error.
 *   0 - no handlers were called.
 *   1 - at least one handler was called.
 */
int
ndmpd_select(ndmpd_session_t *session, bool_t block, u_long class_mask)
{
	fd_set rfds;
	fd_set wfds;
	fd_set efds;
	int n;
	ndmpd_file_handler_t *handler;
	struct timeval timeout;
	struct timeval time_base,time_exit;

	nlp_event_rv_set(session, 0);

	if (session->ns_file_handler_list == 0)
		return (0);

	/*
	 * If select should be blocked, then we poll every ten seconds.
	 * The reason is in case of three-way restore we should be able
	 * to detect if the other end closed the connection or not.
	 * NDMP client(DMA) does not send any information about the connection
	 * that was closed in the other end.
	 */

	if (block == TRUE)
		timeout.tv_sec = 10;
	else
		timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	/*
	 *	when process lost its session. The process will not exit()
	 *	In block mode, we will set a total 90 seconds timeout
	 *	to exist before session lost.
	 */
	if(block==TRUE)
		gettimeofday(&time_base,NULL);

	do {
		/* Create the fd_sets for select. */
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&efds);

		for (handler = session->ns_file_handler_list; handler != 0;
		    handler = handler->fh_next) {
			if ((handler->fh_class & class_mask) == 0)
				continue;
			if (handler->fh_mode & NDMPD_SELECT_MODE_READ)
				FD_SET(handler->fh_fd, &rfds);
			if (handler->fh_mode & NDMPD_SELECT_MODE_WRITE)
				FD_SET(handler->fh_fd, &wfds);
			if (handler->fh_mode & NDMPD_SELECT_MODE_EXCEPTION)
				FD_SET(handler->fh_fd, &efds);
		}
		ndmp_check_mover_state(session);

		// if n == 0, means no data with in the timeout.
		n = select(FD_SETSIZE, &rfds, &wfds, &efds, &timeout);
		if(block){
			gettimeofday(&time_exit,NULL);
			if(time_exit.tv_sec - time_base.tv_sec>=90)
				return (-1);
		}

	} while (n == 0 && block == TRUE);

	if (n < 0) {
		int connection_fd = ndmp_get_fd(session->ns_connection);
		ndmpd_log(LOG_DEBUG, "Select error: %m");
		if (errno == EINTR)
			return (0);

		ndmpd_log(LOG_DEBUG, "Select error: %m");

		for (handler = session->ns_file_handler_list; handler != 0;
		    handler = handler->fh_next) {
			ndmpd_log(LOG_DEBUG, "handler = next handler");

			if ((handler->fh_class & class_mask) == 0)
				continue;
			if (handler->fh_mode & NDMPD_SELECT_MODE_READ) {
				if (FD_ISSET(handler->fh_fd, &rfds) &&
				    connection_fd == handler->fh_fd)
					session->ns_eof = TRUE;
			}
			if (handler->fh_mode & NDMPD_SELECT_MODE_WRITE) {
				if (FD_ISSET(handler->fh_fd, &wfds) &&
				    connection_fd == handler->fh_fd)
					session->ns_eof = TRUE;
			}
			if (handler->fh_mode & NDMPD_SELECT_MODE_EXCEPTION) {
				if (FD_ISSET(handler->fh_fd, &efds) &&
				    connection_fd == handler->fh_fd)
					session->ns_eof = TRUE;
			}
		}

		nlp_event_rv_set(session, -1);
		return (-1);
	}
	if (n == 0)
		return (0);

	/*	got some data here.	Iterate handler to handle the data.	*/
	handler = session->ns_file_handler_list;
	u_long mode = 0;
	while (handler != 0) {
		mode = 0;

		if ((handler->fh_class & class_mask) == 0) {
			handler = handler->fh_next;
			continue;
		}
		if (handler->fh_mode & NDMPD_SELECT_MODE_READ) {
			if (FD_ISSET(handler->fh_fd, &rfds)) {
				mode |= NDMPD_SELECT_MODE_READ;
				FD_CLR(handler->fh_fd, &rfds);
			}
		}
		if (handler->fh_mode & NDMPD_SELECT_MODE_WRITE) {
			if (FD_ISSET(handler->fh_fd, &wfds)) {
				mode |= NDMPD_SELECT_MODE_WRITE;
				FD_CLR(handler->fh_fd, &wfds);
			}
		}
		if (handler->fh_mode & NDMPD_SELECT_MODE_EXCEPTION) {
			if (FD_ISSET(handler->fh_fd, &efds)) {
				mode |= NDMPD_SELECT_MODE_EXCEPTION;
				FD_CLR(handler->fh_fd, &efds);
			}
		}

		if (mode) {
			ndmpd_log(LOG_DEBUG, "pass to handler - start");

			(*handler->fh_func) (handler->fh_cookie, handler->fh_fd, mode);

			ndmpd_log(LOG_DEBUG, "pass to handler - done");


			/*
			 * K.L. The list can be modified during the execution
			 * of handler->fh_func. Therefore, handler will start
			 * from the beginning of the handler list after
			 * each execution.
			 */
			handler = session->ns_file_handler_list;

			/*
			 * Release the thread which is waiting for a request
			 * to be proccessed.
			 */
			nlp_event_nw(session);
		} else
			handler = handler->fh_next;
	}

	nlp_event_rv_set(session, 1);

	return (1);
}

/*
 * ndmpd_save_env
 *
 * Saves a copy of the environment variable list from the data_start_backup
 * request or data_start_recover request.
 *
 * Parameters:
 *   session (input) - session pointer.
 *   env     (input) - environment variable list to be saved.
 *   envlen  (input) - length of variable array.
 *
 * Returns:
 *   error code.
 */
ndmp_error
ndmpd_save_env(ndmpd_session_t *session, ndmp_pval *env, u_long envlen)
{
	u_long i;
	char *namebuf;
	char *valbuf;
	ndmpd_log(LOG_DEBUG, "ndmpd_save_env");
	session->ns_data.dd_env_len = 0;

	if (envlen == 0)
		return (NDMP_NO_ERR);

	session->ns_data.dd_env = ndmp_malloc(sizeof (ndmp_pval) * envlen);
	if (session->ns_data.dd_env == 0)
		return (NDMP_NO_MEM_ERR);

	for (i = 0; i < envlen; i++) {
		namebuf = strdup(env[i].name);
		if (namebuf == 0)
			return (NDMP_NO_MEM_ERR);

		valbuf = strdup(env[i].value);
		if (valbuf == 0) {
			free(namebuf);
			return (NDMP_NO_MEM_ERR);
		}

		ndmpd_log(LOG_DEBUG, "env(%s): \"%s\"",
		    namebuf, valbuf);

		(void) mutex_lock(&session->ns_lock);
		session->ns_data.dd_env[i].name = namebuf;
		session->ns_data.dd_env[i].value = valbuf;
		session->ns_data.dd_env_len++;
		(void) mutex_unlock(&session->ns_lock);
	}

	return (NDMP_NO_ERR);
}

/*
 * ndmpd_free_tcp
 *
 * Free the TCP structure allocate for ndmp v4.
 *
 * Parameters:
 *   session - NDMP session pointer.
 *
 * Returns:
 *   void.
 */
void
ndmpd_free_tcp(ndmpd_session_t *session)
{
	if(session->ns_data.dd_data_addr_v4.tcp_len_v4){
		(void) mutex_lock(&session->ns_lock);
		session->ns_data.dd_data_addr_v4.tcp_len_v4=0;
		free(session->ns_data.dd_data_addr_v4.tcp_addr_v4);
		(void) mutex_unlock(&session->ns_lock);
	}
}

/*
 * ndmpd_free_env
 *
 * Free the previously saved environment variable array.
 *
 * Parameters:
 *   session - NDMP session pointer.
 *
 * Returns:
 *   void.
 */
void
ndmpd_free_env(ndmpd_session_t *session)
{
	int i;
	int count = session->ns_data.dd_env_len;

	(void) mutex_lock(&session->ns_lock);
	session->ns_data.dd_env_len = 0;
	for (i = 0; i < count; i++) {
		free(session->ns_data.dd_env[i].name);
		free(session->ns_data.dd_env[i].value);
	}

	free((char *)session->ns_data.dd_env);
	session->ns_data.dd_env = 0;
	(void) mutex_unlock(&session->ns_lock);
}

/*
 * ndmpd_free_nlist_v3
 *
 * Free a list created by ndmpd_save_nlist_v3.
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   void
 */
void
ndmpd_free_nlist_v3(ndmpd_session_t *session)
{
	u_long i;
	mem_ndmp_name_v3_t *tp; /* destination entry */

	tp = session->ns_data.dd_nlist_v3;
	for (i = 0; i < session->ns_data.dd_nlist_len; tp++, i++) {
		NDMP_FREE(tp->nm3_opath);
		NDMP_FREE(tp->nm3_dpath);
		NDMP_FREE(tp->nm3_newnm);
	}

	NDMP_FREE(session->ns_data.dd_nlist_v3);
	session->ns_data.dd_nlist_len = 0;
}

/*
 * ndmpd_save_nlist_v3
 *
 * Save a copy of list of file names to be restored.
 *
 * Parameters:
 *   nlist    (input) - name list from data_start_recover request.
 *   nlistlen (input) - length of name list.
 *
 * Returns:
 *   array of file name pointers.
 *
 * Notes:
 *   free_nlist should be called to free the returned list.
 *   A null pointer indicates the end of the list.
 */
ndmp_error
ndmpd_save_nlist_v3(ndmpd_session_t *session, ndmp_name_v3 *nlist,
    u_long nlistlen)
{
	u_long i;
	ndmp_error rv;
	ndmp_name_v3 *sp; /* source entry */
	mem_ndmp_name_v3_t *tp; /* destination entry */

	if (nlistlen == 0)
		return (NDMP_ILLEGAL_ARGS_ERR);

	session->ns_data.dd_nlist_len = 0;
	tp = session->ns_data.dd_nlist_v3 =
	    ndmp_malloc(sizeof (mem_ndmp_name_v3_t) * nlistlen);
	if (session->ns_data.dd_nlist_v3 == 0)
		return (NDMP_NO_MEM_ERR);

	rv = NDMP_NO_ERR;
	sp = nlist;
	for (i = 0; i < nlistlen; tp++, sp++, i++) {
		tp->nm3_opath = strdup(sp->original_path);
		if (!tp->nm3_opath) {
			rv = NDMP_NO_MEM_ERR;
			break;
		}
		if (!*sp->destination_dir) {
			tp->nm3_dpath = NULL;
			/* In V4 destination dir cannot be NULL */
			if (session->ns_protocol_version == NDMPV4) {
				rv = NDMP_ILLEGAL_ARGS_ERR;
				break;
			}
		} else if (!(tp->nm3_dpath = strdup(sp->destination_dir))) {
			rv = NDMP_NO_MEM_ERR;
			break;
		}
		if (!*sp->new_name)
			tp->nm3_newnm = NULL;
		else if (!(tp->nm3_newnm = strdup(sp->new_name))) {
			rv = NDMP_NO_MEM_ERR;
			break;
		}

		tp->nm3_node = quad_to_long_long(sp->node);
		tp->nm3_fh_info = quad_to_long_long(sp->fh_info);
		tp->nm3_err = NDMP_NO_ERR;
		session->ns_data.dd_nlist_len++;

		ndmpd_log(LOG_DEBUG, "orig \"%s\"", tp->nm3_opath);
		ndmpd_log(LOG_DEBUG, "dest \"%s\"", NDMP_SVAL(tp->nm3_dpath));
		ndmpd_log(LOG_DEBUG, "name \"%s\"", NDMP_SVAL(tp->nm3_newnm));
		ndmpd_log(LOG_DEBUG, "node %lld", tp->nm3_node);
		ndmpd_log(LOG_DEBUG, "fh_info %lld", tp->nm3_fh_info);
	}

	if (rv != NDMP_NO_ERR)
		ndmpd_free_nlist_v3(session);

	return (rv);
}

/*
 * ndmpd_free_nlist
 *
 * Free the recovery list based on the version
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   void
 */
void
ndmpd_free_nlist(ndmpd_session_t *session)
{
	switch (session->ns_protocol_version) {
	case 1:
	case 2:
	case 3:
	case 4:
		ndmpd_free_nlist_v3(session);
		break;

	default:
		ndmpd_log(LOG_DEBUG, "Unknown version %d",
		    session->ns_protocol_version);
	}
}

/*
 * fh_cmpv3
 *
 * Comparison function used in sorting the Nlist based on their
 * file history info (offset of the entry on the tape)
 *
 * Parameters:
 *   p (input) - pointer to P
 *   q (input) - pointer to Q
 *
 * Returns:
 *  -1: P < Q
 *   0: P = Q
 *   1: P > Q
 */
static int
fh_cmpv3(const void *p,
		const void *q)
{
#define	FH_INFOV3(p)	(((mem_ndmp_name_v3_t *)p)->nm3_fh_info)

	if (FH_INFOV3(p) < FH_INFOV3(q))
		return (-1);
	else if (FH_INFOV3(p) == FH_INFOV3(q))
		return (0);
	else
		return (1);

#undef FH_INFOV3
}

/*
 * ndmp_sort_nlist_v3
 *
 * Sort the recovery list based on their offset on the tape
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   void
 */
void
ndmp_sort_nlist_v3(ndmpd_session_t *session)
{
	if (!session || session->ns_data.dd_nlist_len == 0 ||
	    !session->ns_data.dd_nlist_v3)
		return;

	(void) qsort(session->ns_data.dd_nlist_v3,
	    session->ns_data.dd_nlist_len,
	    sizeof (mem_ndmp_name_v3_t), fh_cmpv3);
}

/*
 * ndmp_send_reply
 *
 * Send the reply, check for error and print the msg if any error
 * occured when sending the reply.
 *
 *   Parameters:
 *     connection (input) - connection pointer.
 *
 *   Return:
 *     void
 */
void
ndmp_send_reply(ndmp_connection_t *connection, void *reply, char *msg)
{
	if (ndmp_send_response(connection, NDMP_NO_ERR, reply) < 0)
		ndmpd_log(LOG_DEBUG, "%s", msg);
}


/*
 * ndmp_mtioctl
 *
 * Performs numerous filemark operations.
 *
 * Parameters:
 * 	fd - file descriptor of the device
 *	cmd - filemark or record command
 * 	count - the number of operations to be performed
 */
int
ndmp_mtioctl(int fd, int cmd, int count)
{
	struct mtop mp;

	mp.mt_op = cmd;
	mp.mt_count = count;
	if (ioctl(fd, MTIOCTOP, &mp) < 0) {
		ndmpd_log(LOG_ERR, "Failed to send command to tape: %m.");
		return (-1);
	}

	return (0);
}

/*
 * quad_to_long_long
 *
 * Convert type quad to longlong_t
 */
u_longlong_t
quad_to_long_long(ndmp_u_quad q)
{
	u_longlong_t ull;

	ull = ((u_longlong_t)q.high << 32) + q.low;
	return (ull);
}

/*
 * long_long_to_quad
 *
 * Convert long long to quad type
 */
ndmp_u_quad
long_long_to_quad(u_longlong_t ull)
{
	ndmp_u_quad q;

	q.high = (u_long)(ull >> 32);
	q.low = (u_long)ull;
	return (q);
}

/*
 * ndmp_set_socket_nodelay
 *
 * Set the TCP socket option to nodelay mode
 */
void
ndmp_set_socket_nodelay(int sock)
{
	int flag = 1;
	(void) setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof (flag));
}

/*
 * ndmp_set_socket_snd_buf
 *
 * Set the socket send buffer size
 */
void
ndmp_set_socket_snd_buf(int sock, int size)
{
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof (size)) < 0)
		ndmpd_log(LOG_DEBUG, "SO_SNDBUF failed errno=%d", errno);
}

/*
 * ndmp_set_socket_rcv_buf
 *
 * Set the socket receive buffer size
 */
void
ndmp_set_socket_rcv_buf(int sock, int size)
{
	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof (size)) < 0)
		ndmpd_log(LOG_DEBUG, "SO_RCVBUF failed errno=%d", errno);
}

/*
 * ndmp_buffer_get_size
 *
 * Return the NDMP transfer buffer size
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   buffer size
 */
long
ndmp_buffer_get_size(ndmpd_session_t *session)
{
	long xfer_size;

	if (session == NULL)
		return (0);

	if (session->ns_data.dd_mover.addr_type == NDMP_ADDR_TCP) {
		xfer_size = atoi(ndmpd_get_prop_default(NDMP_MOVER_RECSIZE,
		    (char *)"60"));
		if (xfer_size > 0)
			xfer_size *= KB;
		else
			xfer_size = REMOTE_RECORD_SIZE;
		ndmpd_log(LOG_DEBUG, "Remote operation: %ld", xfer_size);
	} else {
		ndmpd_log(LOG_DEBUG,
		    "Local operation: %lu", session->ns_mover.md_record_size);
		if ((xfer_size = session->ns_mover.md_record_size) == 0)
			xfer_size = MAX_RECORD_SIZE;
	}

	ndmpd_log(LOG_DEBUG, "xfer_size: %ld", xfer_size);

	return (xfer_size);
}

/*
 * ndmp_lbr_init
 *
 * Initialize the LBR/NDMP backup parameters
 *
 * lbr save information about the session.
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   0: on success
 *  -1: otherwise
 */
int
ndmp_lbr_init(ndmpd_session_t *session)
{
	if (session->ns_ndmp_lbr_params != NULL) {
		ndmpd_log(LOG_DEBUG, "ndmp_lbr_params already allocated.");
		return (0);
	}

	session->ns_ndmp_lbr_params = ndmp_malloc(sizeof (ndmp_lbr_params_t));
	if (session->ns_ndmp_lbr_params == NULL)
		return (-1);

	session->ns_ndmp_lbr_params->nlp_session = session;

	ndmpd_log(LOG_DEBUG, "ndmp_lbr_params allocated.");

	(void) cond_init(&session->ns_ndmp_lbr_params->nlp_cv, 0, NULL);
	(void) mutex_init(&session->ns_lock, 0, NULL);
	session->ns_nref = 0;

	return (0);
}

/*
 * ndmp_lbr_cleanup
 *
 * Deallocate and cleanup all NDMP/LBR parameters
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   0: on success
 *  -1: otherwise
 */
void
ndmp_lbr_cleanup(ndmpd_session_t *session)
{
	/*
	 * If in 3-way restore, the connection close is detected after
	 * check in tape_read(), the reader thread of mover may wait forever
	 * for the tape to be changed.  Force the reader thread to exit.
	 */
	nlp_event_rv_set(session, -2);
	nlp_event_nw(session);

	ndmp_stop_buffer_worker(session);
	ndmp_waitfor_op(session);
	ndmp_free_reader_writer_ipc(session);
	if (session->ns_ndmp_lbr_params) {
		tlm_release_list(session->ns_ndmp_lbr_params->nlp_exl);
		tlm_release_list(session->ns_ndmp_lbr_params->nlp_inc);
		(void) cond_destroy(&session->ns_ndmp_lbr_params->nlp_cv);
		(void) mutex_destroy(&session->ns_lock);
	}

	NDMP_FREE(session->ns_ndmp_lbr_params);
}

/*
 * nlp_ref_nw
 *
 * Increase the references to the NDMP/LBR parameter to prevent
 * unwanted release
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   void
 */
void
nlp_ref_nw(ndmpd_session_t *session)
{
	ndmp_lbr_params_t *nlp;

	(void) mutex_lock(&nlp_mtx);
	if ((nlp = ndmp_get_nlp(session)) != NULL) {
		nlp->nlp_nw++;
		ndmpd_log(LOG_DEBUG, "nw: %d", nlp->nlp_nw);
	} else
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
	(void) mutex_unlock(&nlp_mtx);
}

/*
 * nlp_unref_nw
 *
 * Decrease the references to the NDMP/LBR parameter before
 * release
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   void
 */
void
nlp_unref_nw(ndmpd_session_t *session)
{
	ndmp_lbr_params_t *nlp;

	(void) mutex_lock(&nlp_mtx);
	if ((nlp = ndmp_get_nlp(session)) != NULL) {
		ndmpd_log(LOG_DEBUG, "nw: %d", nlp->nlp_nw);
		if (nlp->nlp_nw > 0)
			nlp->nlp_nw--;
	} else
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
	(void) mutex_unlock(&nlp_mtx);
}

/*
 * nlp_wait_nw
 *
 * Wait for a NDMP/LBR parameter to get available
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   void
 */
void
nlp_wait_nw(ndmpd_session_t *session)
{
	ndmp_lbr_params_t *nlp;

	(void) mutex_lock(&nlp_mtx);
	if ((nlp = ndmp_get_nlp(session)) != NULL) {
		ndmpd_log(LOG_DEBUG, "nw: %d", nlp->nlp_nw);
		if (nlp->nlp_nw > 0) {
			ndmpd_log(LOG_DEBUG, "Waiting");
			while ((nlp->nlp_flag & NLP_READY) == 0){
				(void) cond_wait(&nlp->nlp_cv, &nlp_mtx);
			}
		}
	} else
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
	(void) mutex_unlock(&nlp_mtx);
}

/*
 * nlp_event_nw
 *
 * Signal that a NDMP/LBR parameter is available to wake up the
 * threads waiting on that
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   void
 */
void
nlp_event_nw(ndmpd_session_t *session)
{
	ndmp_lbr_params_t *nlp;

	(void) mutex_lock(&nlp_mtx);
	if ((nlp = ndmp_get_nlp(session)) != NULL) {
		if (nlp->nlp_nw > 0) {
			ndmpd_log(LOG_DEBUG, "nw: %d", nlp->nlp_nw);
			nlp->nlp_flag |= NLP_READY;
			(void) cond_signal(&nlp->nlp_cv);
		}
	} else
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
	(void) mutex_unlock(&nlp_mtx);
}

/*
 * nlp_event_rv_get
 *
 * Get the return value for each NLP
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   return value
 */
int
nlp_event_rv_get(ndmpd_session_t *session)
{
	ndmp_lbr_params_t *nlp;

	if ((nlp = ndmp_get_nlp(session)) == NULL) {
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
		return (0);
	}

	return (nlp->nlp_rv);
}

/*
 * nlp_event_rv_set
 *
 * Set the return value for an NLP
 *
 * Parameters:
 *   session (input) - session pointer.
 *   rv (input) - return value
 *
 * Returns:
 *   void
 */
void
nlp_event_rv_set(ndmpd_session_t *session,
    int rv)
{
	ndmp_lbr_params_t *nlp;

	(void) mutex_lock(&nlp_mtx);
	if (rv != 0)
		ndmpd_log(LOG_DEBUG, "rv: %d", rv);

	if ((nlp = ndmp_get_nlp(session)) != NULL)
		nlp->nlp_rv = rv;
	else
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
	(void) mutex_unlock(&nlp_mtx);
}

/*
 * ndmp_stop_local_reader
 *
 * Stops a mover reader thread (for local backup only)
 *
 * Parameters:
 *   session (input) - session pointer
 *   cmds (input) - reader/writer command struct
 *
 * Returns:
 *   void
 */
void
ndmp_stop_local_reader(ndmpd_session_t *session, tlm_commands_t *cmds)
{
	if (session != NULL) {
		if (session->ns_data.dd_sock == -1) {
			/*
			 * 2-way restore.
			 */
			ndmpd_log(LOG_DEBUG, "2-way restore");
			if (cmds != NULL && cmds->tcs_reader_count > 0) {
				nlp_event_rv_set(session, -2);
				nlp_event_nw(session);
			}
		}
	}
}

/*
 * Stops a mover reader thread (for remote backup only)
 *
 * Parameters:
 *   session (input) - session pointer
 *   cmds (input) - reader/writer command struct
 *
 * Returns:
 *   void
 */
void
ndmp_stop_remote_reader(ndmpd_session_t *session)
{
	if (session != NULL) {
		if (session->ns_data.dd_sock >= 0) {
			 /* 3-way restore. */
			(void) close(session->ns_data.dd_sock);
			session->ns_data.dd_sock = -1;
		}
	}
}

/*
 * ndmp_stop_buffer_worker
 *
 * Stop all reader and writer threads for a specific buffer.
 *
 * Parameters:
 *   session (input) - session pointer
 *
 * Returns:
 *   void
 */
void
ndmp_stop_buffer_worker(ndmpd_session_t *session)
{
	ndmp_lbr_params_t *nlp;
	tlm_commands_t *cmds;

	//session->ns_tape.td_pos = 0;
	if ((nlp = ndmp_get_nlp(session)) == NULL) {
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
	} else {
		cmds = &nlp->nlp_cmds;
		if (cmds->tcs_command == NULL) {
			ndmpd_log(LOG_DEBUG, "cmds->tcs_command == NULL");
		} else {
			cmds->tcs_reader = cmds->tcs_writer = TLM_ABORT;
			cmds->tcs_command->tc_reader = TLM_ABORT;
			cmds->tcs_command->tc_writer = TLM_ABORT;
			while (cmds->tcs_reader_count > 0 ||
			    cmds->tcs_writer_count > 0) {
				(void)pthread_yield();
			}
		}
	}
}

/*
 * ndmp_stop_reader_thread
 *
 * Stop only the reader threads of a specific buffer
 *
 * Parameters:
 *   session (input) - session pointer
 *
 * Returns:
 *   void
 */
void
ndmp_stop_reader_thread(ndmpd_session_t *session)
{
	ndmp_lbr_params_t *nlp;
	tlm_commands_t *cmds;

	if ((nlp = ndmp_get_nlp(session)) == NULL) {
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
	} else {
		cmds = &nlp->nlp_cmds;
		if (cmds->tcs_command == NULL) {
			ndmpd_log(LOG_DEBUG, "cmds->tcs_command == NULL");
		} else {
			cmds->tcs_reader = TLM_ABORT;
			cmds->tcs_command->tc_reader = TLM_ABORT;
			while (cmds->tcs_reader_count > 0) {
				(void)pthread_yield();
//				ndmpd_log(LOG_DEBUG,
//				    "trying to stop reader thread");
				//(void) sleep(1);
			}
		}
	}
}

/*
 * ndmp_stop_reader_thread
 *
 * Stop only the writer threads of a specific buffer
 *
 * Parameters:
 *   session (input) - session pointer
 *
 * Returns:
 *   void
 */
void
ndmp_stop_writer_thread(ndmpd_session_t *session)
{
	ndmp_lbr_params_t *nlp;
	tlm_commands_t *cmds;

	if ((nlp = ndmp_get_nlp(session)) == NULL)
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
	else {
		cmds = &nlp->nlp_cmds;
		if (cmds->tcs_command == NULL)
			ndmpd_log(LOG_DEBUG, "cmds->tcs_command == NULL");
		else {
			cmds->tcs_writer = TLM_ABORT;
			cmds->tcs_command->tc_writer = TLM_ABORT;
			while (cmds->tcs_writer_count > 0) {
				(void)pthread_yield();
			}
		}
	}
}

/*
 * ndmp_free_reader_writer_ipc
 *
 * Free and release the reader/writer buffers and the IPC structure
 * for reader and writer threads.
 *
 * Parameters:
 *   session (input) - session pointer
 *
 * Returns:
 *   void
 */
void
ndmp_free_reader_writer_ipc(ndmpd_session_t *session)
{
	ndmp_lbr_params_t *nlp;
	tlm_commands_t *cmds;

	if ((nlp = ndmp_get_nlp(session)) != NULL) {
		cmds = &nlp->nlp_cmds;
		if (cmds->tcs_command != NULL) {
			ndmpd_log(LOG_DEBUG, "cmds->tcs_command->tc_ref: %d",
			    cmds->tcs_command->tc_ref);
			tlm_release_reader_writer_ipc(cmds->tcs_command);
		}
	}
}

/*
 * ndmp_waitfor_op
 *
 * Wait for a session reference count to drop to zero
 *
 * Parameters:
 *   session (input) - session pointer
 *
 * Returns:
 *   void
 */
void
ndmp_waitfor_op(ndmpd_session_t *session)
{
	if (session != NULL) {
		while (session->ns_nref > 0) {
			pthread_yield();
			ndmpd_log(LOG_DEBUG,
			    "waiting for session nref: %d", session->ns_nref);
		}
	}
}

/*
 * ndmp_session_ref
 *
 * Increment the reference count of the session
 *
 * Parameters:
 *   session (input) - session pointer
 *
 * Returns:
 *   void
 */
void
ndmp_session_ref(ndmpd_session_t *session)
{
	(void) mutex_lock(&session->ns_lock);
	session->ns_nref++;
	(void) mutex_unlock(&session->ns_lock);
}

/*
 * ndmp_session_unref
 *
 * Decrement the reference count of the session
 *
 * Parameters:
 *   session (input) - session pointer
 *
 * Returns:
 *   void
 */
void
ndmp_session_unref(ndmpd_session_t *session)
{
	(void) mutex_lock(&session->ns_lock);
	session->ns_nref--;
	(void) mutex_unlock(&session->ns_lock);
}

/*
 * ndmp_valid_v3addr_type
 *
 * Make sure that the NDMP address is from any of the
 * valid types
 *
 * Parameters:
 *   type (input) - address type
 *
 * Returns:
 *   1: valid
 *   0: invalid
 */
bool_t
ndmp_valid_v3addr_type(ndmp_addr_type type)
{
	bool_t rv;

	switch (type) {
	case NDMP_ADDR_LOCAL:
	case NDMP_ADDR_TCP:
	case NDMP_ADDR_FC:
	case NDMP_ADDR_IPC:
		rv = TRUE;
		break;
	default:
		rv = FALSE;
	}

	return (rv);
}

/*
 * ndmp_copy_addr_v3
 *
 * Copy NDMP address from source to destination (V2 and V3 only)
 *
 * Parameters:
 *   dst (ouput) - destination address
 *   src (input) - source address
 *
 * Returns:
 *   void
 */
void
ndmp_copy_addr_v3(ndmp_addr_v3 *dst, ndmp_addr_v3 *src)
{
	dst->addr_type = src->addr_type;
	switch (src->addr_type) {
	case NDMP_ADDR_LOCAL:
		/* nothing */
		break;
	case NDMP_ADDR_TCP:
		dst->tcp_ip_v3 = htonl(src->tcp_ip_v3);
		dst->tcp_port_v3 = src->tcp_port_v3;
		break;
	case NDMP_ADDR_FC:
	case NDMP_ADDR_IPC:
	default:
		break;
	}
}

/*
 * ndmp_copy_addr_v4
 *
 * Copy NDMP address from source to destination. V4 has a extra
 * environment list inside the address too which needs to be copied.
 *
 * Parameters:
 *   dst (ouput) - destination address
 *   src (input) - source address
 *
 * Returns:
 *   void
 */
void
ndmp_copy_addr_v4(ndmp_addr_v4 *dst, ndmp_addr_v4 *src)
{
	unsigned int i;

	dst->addr_type = src->addr_type;
	dst->tcp_len_v4 = src->tcp_len_v4;
	switch (src->addr_type) {
	case NDMP_ADDR_LOCAL:
		/* nothing */
		break;
	case NDMP_ADDR_TCP:
		dst->tcp_addr_v4 = ndmp_malloc(sizeof (ndmp_tcp_addr_v4) *
		    src->tcp_len_v4);
		if (dst->tcp_addr_v4 == 0)
			return;

		for (i = 0; i < src->tcp_len_v4; i++) {
			dst->tcp_ip_v4(i) = htonl(src->tcp_ip_v4(i));
			dst->tcp_port_v4(i) = src->tcp_port_v4(i);
			dst->tcp_env_v4(i).addr_env_len = 0; /* Solaris */
			dst->tcp_env_v4(i).addr_env_val = 0; /* Solaris */
		}
		break;
	case NDMP_ADDR_FC:
	case NDMP_ADDR_IPC:
	default:
		break;
	}
}

/*
 * ndmp_connect_sock_v3
 *
 * Creates a socket and connects to the specified address/port
 *
 * Parameters:
 *   addr (input) - IP address
 *   port (input) - port number
 *
 * Returns:
 *   0: on success
 *  -1: otherwise
 */
int
ndmp_connect_sock_v3(u_long addr, u_short port)
{
	int sock;
	struct sockaddr_in sin;
	int flag = 1;

	ndmpd_log(LOG_DEBUG, "addr %s:%d", inet_ntoa(IN_ADDR(addr)), port);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		ndmpd_log(LOG_DEBUG, "Socket error: %m");
		return (-1);
	}

	(void) memset((void *) &sin, 0, sizeof (sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(addr);
	sin.sin_port = htons(port);
	if (connect(sock, (struct sockaddr *)&sin, sizeof (sin)) < 0) {
		ndmpd_log(LOG_DEBUG, "Connect error: %m");
		(void) close(sock);
		sock = -1;
	} else {
		if (ndmp_sbs > 0)
			ndmp_set_socket_snd_buf(sock, ndmp_sbs*KB);
		if (ndmp_rbs > 0)
			ndmp_set_socket_rcv_buf(sock, ndmp_rbs*KB);

		ndmp_set_socket_nodelay(sock);
		(void) setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &flag,
		    sizeof (flag));

		ndmpd_log(LOG_DEBUG, "sock %d", sock);
	}

	return (sock);
}

char *
getIPfromNIC(char *nicname) {
	// IPv4
	char *ip;
	struct sockaddr_in *ipaddr;

	int fd=0;
	struct ifreq ifr;

	// get IP from interface
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	/* I want to get an IPv4 IP address */
	ifr.ifr_addr.sa_family = AF_INET;
	/* I want IP address attached to nicname */
	strncpy(ifr.ifr_name, nicname, IFNAMSIZ-1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);

	//ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
	ipaddr = (struct sockaddr_in *)(void *)&ifr.ifr_addr;
	ip = inet_ntoa(ipaddr->sin_addr);

	return ip;
}

/*
 * ndmp_create_socket
 *
 * Creates a socket for listening for accepting data connections.
 *
 * Parameters:
 *   session (input)  - session pointer.
 *   addr    (output) - location to store address of socket.
 *   port    (output) - location to store port of socket.
 *
 * Returns:
 *   0 - success.
 *  -1 - error.
 */
int
ndmp_create_socket(u_long *addr, u_short *port)
{
	char *p;
	socklen_t length;
	int sd;
	struct sockaddr_in sin;

	if(strcmp(ndmpd_get_prop(NDMP_SERVE_NIC),"")==0)
		p = getIPfromNIC(ndmpd_get_prop(NDMP_LISTEN_NIC));
	else
		p = getIPfromNIC(ndmpd_get_prop(NDMP_SERVE_NIC));

	ndmpd_log(LOG_DEBUG, "ndmp_create_socket with IP %s",p);

	if (!p || *p == 0) {
		ndmpd_log(LOG_ERR, "Undetermined network port.");
		return (-1);
	}

	*addr = inet_addr(p);

	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd < 0) {
		ndmpd_log(LOG_DEBUG, "Socket error: %m");
		return (-1);
	}

	(void) memset((void *) &sin, 0, sizeof (sin));

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = 0;

	length = (socklen_t)sizeof (sin);

	if (bind(sd, (struct sockaddr *)&sin, sizeof (sin)) < 0) {
		ndmpd_log(LOG_DEBUG, "Bind error: %m");
		(void) close(sd);
		sd = -1;
	} else if (getsockname(sd, (struct sockaddr *)&sin, &length) < 0) {
		ndmpd_log(LOG_DEBUG, "getsockname error: %m");
		(void) close(sd);
		sd = -1;
	} else if (listen(sd, 5) < 0) {
		ndmpd_log(LOG_DEBUG, "Listen error: %m");
		(void) close(sd);
		sd = -1;
	} else
		*port = sin.sin_port;

	return (sd);
}

/*
 * cctime
 *
 * Convert the specified time into a string.  It's like
 * ctime(), but:
 *     - chops the trailing '\n' of ctime.
 *     - and returns "the epoch" if time is 0.
 *
 * Returns:
 *     "": invalid argument.
 *     "the epoch": if time is 0.
 *     string format of the time.
 */
char *
cctime(time_t *t)
{
	char *bp, *cp;
	char tbuf[BUFFER_SIZE];

	if (!t)
		return ("");

	if (*t == (time_t)0)
		return ("the epoch");

	bp = ctime_r(t, tbuf);
	cp = strchr(bp, '\n');
	if (cp)
		*cp = '\0';

	return (bp);
}

/*
 * ndmp_new_job_name
 *
 * Create a job name for each backup/restore to keep track
 *
 * Parameters:
 *   jname (output) - job name
 *
 * Returns:
 *   jname
 */

char *
ndmp_new_job_name(char *jname)
{
	time_t lt;
	lt =time(NULL);
	if (jname != NULL) {
		(void) snprintf(jname, MAX_BACKUP_JOB_NAME, "%s%ld",
		    NDMP_RCF_BASENAME, lt);
		ndmpd_log(LOG_DEBUG, "jname: \"%s\"", jname);
	}

	return (jname);
}

/*
 * fs_is_valid_logvol
 *
 * Check if the log path exists
 *
 * Parameters:
 *   path (input) - log path
 *
 * Returns:
 *   FALSE: invalid
 *   TRUE: valid
 */
bool_t
fs_is_valid_logvol(char *path)
{
	struct stat st;

	if (stat(path, &st) < 0)
		return (FALSE);

	return (TRUE);
}

/*
 * ndmpd_mk_temp
 *
 * Make a temporary file using the working directory path and the
 * jobname
 *
 * Parameters:
 *   buf (output) - the temporary file name path
 *
 * Returns:
 *   buf
 */
char *
ndmpd_mk_temp(char *buf)
{
	char fname[MAX_BACKUP_JOB_NAME];
	const char *dir;
	char *rv;

	if (!buf)
		return (NULL);

	dir = ndmpd_get_prop(NDMP_DEBUG_PATH);
	if (dir == 0 || *dir == '\0') {
		ndmpd_log(LOG_DEBUG, "NDMP work path not specified");
		return (0);
	}

	if (!fs_is_valid_logvol((char *)dir)) {
		ndmpd_log(LOG_ERR,
		    "Log file path cannot be on system volumes.");
		return (0);
	}

	dir += strspn(dir, " \t");
	if (!*dir) {
		ndmpd_log(LOG_DEBUG, "NDMP work path not specified");
		return (0);
	}

	rv = buf;
	(void) ndmp_new_job_name(fname);
	(void) tlm_cat_path(buf, (char *)dir, fname);

	return (rv);
}

/*
 * ndmpd_make_bk_dir_path
 *
 * Make a directory path for temporary files under the NDMP
 * working directory.
 *
 * Parameters:
 *   buf (output) - result path
 *   fname (input) - the file name
 *
 * Returns:
 *   buf
 */
char *
ndmpd_make_bk_dir_path(char *buf, char *fname)
{
	const char *p;
	char *name;
	char path[PATH_MAX];

	if (!buf || !fname || !*fname)
		return (NULL);

	p = ndmpd_get_prop(NDMP_DEBUG_PATH);
	if (p == NULL || *p == '\0' || !fs_is_valid_logvol((char *)p)) {
		return (NULL);
	}

	(void) strlcpy(path, (char *)p, PATH_MAX);
	(void) trim_whitespace(path);

	if ((name = strrchr(fname, '/')) == 0)
		name = fname;

	(void) tlm_cat_path(buf, path, name);
	return (buf);
}

/*
 * ndmpd_make_exc_list
 *
 * Make a list of files that should not be backed up.
 *
 * Parameters:
 *   void
 *
 * Returns:
 *   list - array of character strings
 */
char **
ndmpd_make_exc_list(void)
{
	char *val, **cpp;
	int i, n;

	n = sizeof (exls);
	if ((cpp = ndmp_malloc(n)) != NULL) {
		for (i = 0; exls[i] != NULL; i++)
			cpp[i] = exls[i];

		/*
		 * If ndmpd_get_prop returns NULL, the array will be
		 * null-terminated.
		 */
		val = ndmpd_get_prop(NDMP_DEBUG_PATH);
		cpp[i] = val;
	}

	return (cpp);
}

/*
 * ndmp_get_bk_dir_ino
 *
 * Get the inode number of the backup directory
 */
int
ndmp_get_bk_dir_ino(ndmp_lbr_params_t *nlp)
{
	int rv;
	struct stat st;

	if (stat(nlp->nlp_backup_path, &st) != 0) {
		rv = -1;
		ndmpd_log(LOG_DEBUG, "Getting inode # of \"%s\"", nlp->nlp_backup_path);
	} else {
		rv = 0;
		nlp->nlp_bkdirino = st.st_ino;
		ndmpd_log(LOG_DEBUG, "nlp_bkdirino: %u", (u_int)nlp->nlp_bkdirino);
	}

	return (rv);
}

/*
 * ndmp_check_utf8magic
 *
 * Check if the magic string for exists in the tar header. This
 * magic string (which also indicates that the file names are in
 * UTF8 format) is used as a crest to indetify our own tapes.
 * This checking is always done before all restores except DAR
 * restores.
 */
bool_t
ndmp_check_utf8magic(tlm_cmd_t *cmd)
{
	char *cp;
	int err, len, actual_size;

	if (cmd == NULL) {
		ndmpd_log(LOG_DEBUG, "cmd == NULL");
		return (FALSE);
	}
	if (cmd->tc_buffers == NULL) {
		ndmpd_log(LOG_DEBUG, "cmd->tc_buffers == NULL");
		return (FALSE);
	}

	/* wait until the first buffer gets full. */
	tlm_buffer_in_buf_wait(cmd->tc_buffers);

	err = actual_size = 0;

	setReadBufDone(cmd->tc_buffers);

	cp = tlm_get_read_buffer(RECORDSIZE, &err, cmd->tc_buffers,
	    &actual_size);

	if (cp == NULL) {
		setReadBufDone(cmd->tc_buffers);
		ndmpd_log(LOG_DEBUG, "Can't read from buffers, err: %d", err);
		return (FALSE);
	}
	len = strlen(NDMPUTF8MAGIC);
	if (actual_size < len) {
		setReadBufDone(cmd->tc_buffers);
		ndmpd_log(LOG_DEBUG, "Not enough data in the buffers");
		return (FALSE);
	}

	bool_t bt = ((strncmp(cp, NDMPUTF8MAGIC, len) == 0) ? TRUE : FALSE);
	setReadBufDone(cmd->tc_buffers);

	return bt;
}

/*
 * ndmp_get_cur_bk_time
 *
 * Get the backup checkpoint time.
 */
int
ndmp_get_cur_bk_time(ndmp_lbr_params_t *nlp, time_t *tp, char *jname)
{
	int err=0;

	if (!nlp || !nlp->nlp_backup_path || !tp) {
		ndmpd_log(LOG_DEBUG, "Invalid argument");
		return (-1);
	}

	*tp = time(NULL);

	if (err != 0) {
		ndmpd_log(LOG_DEBUG, "Can't checkpoint time");
	} else {
		ndmpd_log(LOG_DEBUG, "%s", cctime(tp));
	}

	return (err);
}

/*
 * get_relative_path
 */
char *
ndmp_get_relative_path(char *base, char *fullpath)
{
	char *p = fullpath;

	if (!base || !*base)
		return (fullpath);

	while (*base) {
		if (*base != *p)
			break;
		p++; base++;
	}

	if (*p == '/')
		p++;

	return ((*base) ? fullpath : p);
}

/*
 * ndmp_get_nlp
 *
 * Get NDMP local backup parameters
 *
 * Parameter:
 *   session cooke
 *
 * Returns:
 *   LBR structure
 */
ndmp_lbr_params_t *
ndmp_get_nlp(void *cookie)
{
	if (cookie == NULL){
		ndmpd_log(LOG_DEBUG, "ndmp_get_nlp, cookie= NULL ");
		return (NULL);
	}

	return (((ndmpd_session_t *)cookie)->ns_ndmp_lbr_params);
}

/*
 * ndmp_load_params
 *
 * Load the parameters.
 *
 * Parameter:
 *   void
 *
 * Returns:
 *   void
 */
void
ndmp_load_params(void)
{
	ndmp_dump_path_node = ndmpd_get_prop_yorn(NDMP_DUMP_PATHNODE_ENV) ?
	    TRUE : FALSE;
	ndmp_tar_path_node = ndmpd_get_prop_yorn(NDMP_TAR_PATHNODE_ENV) ?
	    TRUE : FALSE;
	ndmp_ignore_ctime =
	    ndmpd_get_prop_yorn(NDMP_IGNCTIME_ENV) ? TRUE : FALSE;
	ndmp_include_lmtime = ndmpd_get_prop_yorn(NDMP_INCLMTIME_ENV) ?
	    TRUE : FALSE;

	ndmp_full_restore_path = ndmpd_get_prop_yorn(NDMP_FULL_RESTORE_PATH) ?
	    TRUE : FALSE;

	ndmp_fhinode = ndmpd_get_prop_yorn(NDMP_FHIST_INCR_ENV) ? TRUE : FALSE;

	if ((ndmp_ver = atoi(ndmpd_get_prop(NDMP_VERSION_ENV))) == 0)
		ndmp_ver = NDMPVER;
}

/*
 * randomize
 *
 * Randomize the contents of a buffer
 *
 * Parameter:
 *   buffer (output) - destination buffer
 *   size (input) - buffer size
 *
 * Returns:
 *   void
 */
void
randomize(unsigned char *buffer, int size)
{
	/* LINTED improper alignment */
	unsigned int *p = (unsigned int *)(void *)buffer;
	unsigned int dwlen = size / sizeof (unsigned int);
	unsigned int remlen = size % sizeof (unsigned int);
	unsigned int tmp;
	unsigned int i;

	for (i = 0; i < dwlen; i++)
		*p++ = random();

	if (remlen) {
		tmp = random();
		(void) memcpy(p, &tmp, remlen);
	}
}

/*
 * ndmpd_get_file_entry_type
 *
 * Converts the mode to the NDMP file type
 *
 * Parameter:
 *   mode (input) - file mode
 *   ftype (output) - file type
 *
 * Returns:
 *   void
 */
void
ndmpd_get_file_entry_type(int mode, ndmp_file_type *ftype)
{
	switch (mode & S_IFMT) {
	case S_IFIFO:
		*ftype = NDMP_FILE_FIFO;
		break;
	case S_IFCHR:
		*ftype = NDMP_FILE_CSPEC;
		break;
	case S_IFDIR:
		*ftype = NDMP_FILE_DIR;
		break;
	case S_IFBLK:
		*ftype = NDMP_FILE_BSPEC;
		break;
	case S_IFREG:
		*ftype = NDMP_FILE_REG;
		break;
	case S_IFLNK:
		*ftype = NDMP_FILE_SLINK;
		break;
	default:
		*ftype = NDMP_FILE_SOCK;
		break;
	}
}
