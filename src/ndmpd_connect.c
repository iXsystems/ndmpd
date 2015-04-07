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

#include <handler.h>
#include <md5.h>

#include <ndmpd.h>
#include <ndmpd_table.h>
#include <ndmpd_session.h>
#include <ndmpd_func.h>
#include <ndmpd_util.h>

#include <ndmpd_prop.h>


extern int ndmp_ver;

int ndmp_connect_list_add(ndmp_connection_t *connection, int *id);

static int ndmpd_connect_auth_text (char *uname, char *auth_id,char *auth_password);
static int ndmpd_connect_auth_md5 (char *uname, char *auth_id, char *auth_digest,
	unsigned char *auth_challenge);
static struct conn_list *ndmp_connect_list_find	(ndmp_connection_t *connection);
static void create_md5_digest (unsigned char *digest, char *passwd,unsigned char *challenge);

#ifndef LIST_FOREACH
#define	LIST_FOREACH(var, head, field)					\
	for ((var) = (head)->lh_first; (var); (var) = (var)->field.le_next)
#endif /* LIST_FOREACH */

/*
 * List of active connections.
 */
struct conn_list {
	LIST_ENTRY(conn_list) cl_q;
	int cl_id;
	ndmp_connection_t *cl_conn;
};
LIST_HEAD(cl_head, conn_list);

/*
 * Head of the active connections.
 */
static struct cl_head cl_head;

mutex_t cl_mutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * Set this variable to non-zero to print verbose information.
 */
int ndmp_connect_print_verbose = 0;

/*
 * ndmpd_connect_open_v3
 *
 * This handler sets the protocol version to be used on the connection.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */

void
ndmpd_connect_open_v3(ndmp_connection_t *connection, void *body)
{
	ndmp_connect_open_request *request = (ndmp_connect_open_request *)body;
	ndmp_connect_open_reply reply;
	ndmpd_session_t *session;

	reply.error = NDMP_NO_ERR;

	if (!(session = (ndmpd_session_t *)ndmp_get_client_data(connection)))
		return;

	if (session->ns_mover.md_state != NDMP_MOVER_STATE_IDLE ||
			session->ns_data.dd_state != NDMP_DATA_STATE_IDLE)
		reply.error = NDMP_ILLEGAL_STATE_ERR;
	else if (request->protocol_version > ndmp_ver)
		reply.error = NDMP_ILLEGAL_ARGS_ERR;

	if (request->protocol_version > ndmp_ver)
			reply.error = NDMP_ILLEGAL_ARGS_ERR;

	ndmp_send_reply(connection, (void *) &reply,"sending connect_open reply");

	/*
	 * Set the protocol version.
	 * Must wait until after sending the reply since the reply
	 * must be sent using the same protocol version that was used
	 * to process the request.
	 */
	if (reply.error == NDMP_NO_ERR) {
		ndmpd_log(LOG_DEBUG, "set ver to: %d",request->protocol_version);
		ndmp_set_version(connection, request->protocol_version);
		session->ns_protocol_version = request->protocol_version;
	}
}

/*
 * ************************************************************************
 * NDMP V3 HANDLERS
 * ************************************************************************
 */

/*
 * ndmpd_connect_client_auth_v3
 *
 * This handler authorizes the NDMP connection.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   msginfo    (input) - request message.
 *
 * Returns:
 *   void
 */
void
ndmpd_connect_client_auth_v3(ndmp_connection_t *connection, void *body)
{
	ndmp_connect_client_auth_request_v3 *request;
	ndmp_connect_client_auth_reply_v3 reply;
	ndmp_auth_text_v3 *auth;
	ndmpd_session_t *session;
	ndmp_auth_md5_v3 *md5;
	struct in_addr addr;
	char *uname;
	char *type;

	request = (ndmp_connect_client_auth_request_v3 *)body;
	ndmpd_log(LOG_DEBUG, "auth_type %s",
	    request->auth_data.auth_type == NDMP_AUTH_NONE ? "None" :
	    request->auth_data.auth_type == NDMP_AUTH_TEXT ? "Text" :
	    request->auth_data.auth_type == NDMP_AUTH_MD5 ? "MD5" : "Invalid");

	reply.error = NDMP_NO_ERR;

	switch (request->auth_data.auth_type) {
	case NDMP_AUTH_NONE:
		type = "none";
		reply.error = NDMP_NOT_SUPPORTED_ERR;
		//ndmpd_audit_connect(connection, ENOTSUP);
		break;
	case NDMP_AUTH_TEXT:
		/* Check authorization.  */
		if ((uname = ndmpd_get_prop(NDMP_CLEARTEXT_USERNAME)) == NULL ||
		    *uname == 0) {
			ndmpd_log(LOG_ERR, "Authorization denied.");
			ndmpd_log(LOG_ERR, "User name is not set at server.");
			reply.error = NDMP_NOT_AUTHORIZED_ERR;
			ndmp_set_authorized(connection, FALSE);
			ndmp_send_reply(connection, (void *) &reply,
			    "sending ndmp_connect_client_auth reply");

			return;
		}
		type = "text";
		auth = &request->auth_data.ndmp_auth_data_v3_u.auth_text;
		reply.error = ndmpd_connect_auth_text(uname, auth->auth_id,
		    auth->auth_password);
		break;
	case NDMP_AUTH_MD5:
		/* Check authorization.  */
		if ((uname = ndmpd_get_prop(NDMP_CRAM_MD5_USERNAME)) == NULL ||
		    *uname == 0) {
			ndmpd_log(LOG_ERR, "Authorization denied.");
			ndmpd_log(LOG_ERR, "User name is not set at server.");
			reply.error = NDMP_NOT_AUTHORIZED_ERR;
			ndmp_set_authorized(connection, FALSE);
			ndmp_send_reply(connection, (void *) &reply,
			    "sending ndmp_connect_client_auth reply");

			return;
		}
		type = "md5";
		session = ndmp_get_client_data(connection);
		md5 = &request->auth_data.ndmp_auth_data_v3_u.auth_md5;
		reply.error = ndmpd_connect_auth_md5(uname, md5->auth_id,
		    md5->auth_digest, session->ns_challenge);
		break;
	default:
		type = "unknown";
		reply.error = NDMP_ILLEGAL_ARGS_ERR;
	}

	if (reply.error == NDMP_NO_ERR) {
		ndmp_set_authorized(connection, TRUE);
	} else {
		ndmp_set_authorized(connection, FALSE);
		if (tcp_get_peer(connection->conn_sock, &addr.s_addr,
		    NULL) != -1) {
			ndmpd_log(LOG_ERR,
			    "Authorization(%s) denied for %s.", type,
			    inet_ntoa(IN_ADDR(addr)));
		}
	}

	ndmp_send_reply(connection, (void *) &reply,
	    "sending ndmp_connect_auth reply");
}

/*
 * ndmpd_connect_server_auth_v3
 *
 * This handler authenticates the server to the client.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   msginfo    (input) - request message.
 *
 * Returns:
 *   void
 */
void
ndmpd_connect_server_auth_v3(ndmp_connection_t *connection, void *body)
{
	ndmp_connect_server_auth_request *request;
	ndmp_connect_server_auth_reply reply;

	request = (ndmp_connect_server_auth_request *)body;

	ndmpd_log(LOG_DEBUG, "auth_type:%s",
	    request->client_attr.auth_type == NDMP_AUTH_NONE ? "None" :
	    (request->client_attr.auth_type == NDMP_AUTH_TEXT ? "Text" :
	    (request->client_attr.auth_type == NDMP_AUTH_MD5 ? "MD5" :
	    "Invalid")));

	reply.error = NDMP_NO_ERR;
	reply.auth_result.auth_type = request->client_attr.auth_type;
	switch (request->client_attr.auth_type) {
	case NDMP_AUTH_NONE:
		break;
	case NDMP_AUTH_TEXT:
		reply.auth_result.ndmp_auth_data_u.auth_text.user = "ndmpd";
		reply.auth_result.ndmp_auth_data_u.auth_text.password = "ndmpsdk";
		break;
	case NDMP_AUTH_MD5:
		reply.error = NDMP_ILLEGAL_ARGS_ERR;
		break;
	default:
		reply.error = NDMP_ILLEGAL_ARGS_ERR;
	}

	ndmp_send_reply(connection, (void *) &reply,
	    "sending ndmp_connect_auth reply");
}

/*
 * ndmpd_connect_close_v3
 *
 * Close the connection to the DMA.
 * Send the SHUTDOWN message before closing the socket connection to the DMA.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   msginfo    (input) - request message.
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/
void
ndmpd_connect_close_v3(ndmp_connection_t *connection, void *body)
{
	ndmpd_session_t *session;
	ndmp_notify_connected_request req;

	if (!(session = (ndmpd_session_t *)ndmp_get_client_data(connection)))
		return;

	/* Send the SHUTDOWN message before closing the connection. */
	req.reason = NDMP_SHUTDOWN;
	req.protocol_version = session->ns_protocol_version;
	req.text_reason = "Connection closed by server.";

	if (ndmp_send_request(connection, NDMP_NOTIFY_CONNECTION_STATUS,
	    NDMP_NO_ERR, (void *) &req, 0) < 0) {
		return;
	}

	ndmp_close(connection);
	session->ns_eof = TRUE;
}

/*
 * ************************************************************************
 * NDMP V4 HANDLERS
 * ************************************************************************
 */

/*
 * ************************************************************************
 * LOCALS
 * ************************************************************************
 */

/*
 * create_md5_digest
 *
 * This function uses the MD5 message-digest algorithm described
 * in RFC1321 to authenticate the client using a shared secret (password).
 * The message used to compute the MD5 digest is a concatenation of password,
 * null padding, the 64 byte fixed length challenge and a repeat of the
 * password. The length of the null padding is chosen to result in a 128 byte
 * fixed length message. The lengh of the padding can be computed as
 * 64 - 2*(length of the password). The client digest is computed using the
 * server challenge from the NDMP_CONFIG_GET_AUTH_ATTR reply.
 *
 * Parameters:
 *   digest (output) - 16 bytes MD5 digest
 *   passwd (input) - user password
 *   challenge (input) - 64 bytes server challenge
 *
 * Returns:
 *   void
 */
static void
create_md5_digest(unsigned char *digest, char *passwd, unsigned char *challenge)
{
	char buf[130];
	char *p = &buf[0];
	int len, i;
	MD5_CTX md;
	char *pwd;

	*p = 0;
	pwd = passwd;
	if ((len = strlen(pwd)) > MD5_PASS_LIMIT)
		len = MD5_PASS_LIMIT;
	(void) memcpy(p, pwd, len);
	p += len;

	for (i = 0; i < MD5_CHALLENGE_SIZE - 2 * len; i++)
		*p++ = 0;

	(void) memcpy(p, challenge, MD5_CHALLENGE_SIZE);
	p += MD5_CHALLENGE_SIZE;
	(void) strlcpy(p, pwd, MD5_PASS_LIMIT);

	MD5Init(&md);
	MD5Update(&md, buf, 128);
	MD5Final(digest, &md);
}

/*
 * ndmp_connect_list_find
 *
 * Find the element in the active connection list.
 *
 * Parameters:
 *   connection (input) - connection handler.
 *
 * Returns:
 *   NULL - error
 *   connection list element pointer
 */
static struct conn_list
*ndmp_connect_list_find(ndmp_connection_t *connection)
{
	struct conn_list *clp;

	LIST_FOREACH(clp, &cl_head, cl_q) {
		if (clp->cl_conn == connection) {
			(void) mutex_unlock(&cl_mutex);
			return (clp);
		}
	}
	return (NULL);
}

/*
 * ndmpconnect_list_add
 *
 * Add the new connection to the list of the active connections.
 *
 * Parameters:
 *   connection (input) - connection handler.
 *   id (input/output) - pointer to connection id.
 *
 * Returns:
 *   0 - success
 *  -1 - error
 */
int
ndmp_connect_list_add(ndmp_connection_t *connection, int *id)
{
	struct conn_list *clp;

	if (connection == NULL) {
		return (-1);
	}

	if ((clp = ndmp_malloc(sizeof (struct conn_list))) == NULL)
		return (-1);

	clp->cl_conn = connection;
	clp->cl_id = *id;

	(void) mutex_lock(&cl_mutex);
	LIST_INSERT_HEAD(&cl_head, clp, cl_q);
	(*id)++;
	(void) mutex_unlock(&cl_mutex);

	return (0);
}

/*
 * ndmp_connect_list_del
 *
 * Delete the specified connection from the list.
 *
 * Parameters:
 *   connection (input) - connection handler.
 *
 * Returns:
 *   0 - success
 *  -1 - error
 */
int
ndmp_connect_list_del(ndmp_connection_t *connection)
{
	struct conn_list *clp;

	(void) mutex_lock(&cl_mutex);
	if (!(clp = ndmp_connect_list_find(connection))) {
		(void) mutex_unlock(&cl_mutex);
		ndmpd_log(LOG_DEBUG, "connection not found");
		return (-1);
	}

	LIST_REMOVE(clp, cl_q);
	(void) mutex_unlock(&cl_mutex);
	free(clp);

	return (0);
}

/*
 * ndmpd_connect_auth_text
 *
 * Checks text authorization.
 *
 * Parameters:
 *   auth_id (input) - user name
 *   auth_password(input) - password
 *
 * Returns:
 *   NDMP_NO_ERR: on success
 *   Other NDMP_ error: invalid user name and password
 */
int
ndmpd_connect_auth_text(char *uname, char *auth_id, char *auth_password)
{
	char *passwd, *dec_passwd;
	int rv;

	if (strcmp(uname, auth_id) != 0) {
		rv = NDMP_NOT_AUTHORIZED_ERR;
	} else {
		passwd = ndmpd_get_prop(NDMP_CLEARTEXT_PASSWORD);
		if (!passwd || !*passwd) {
			rv = NDMP_NOT_AUTHORIZED_ERR;
		} else {
			dec_passwd = ndmp_base64_decode(passwd);
			if (dec_passwd == NULL || *dec_passwd == 0)
				rv = NDMP_NOT_AUTHORIZED_ERR;
			else if (strcmp(auth_password, dec_passwd) != 0)
				rv = NDMP_NOT_AUTHORIZED_ERR;
			else
				rv = NDMP_NO_ERR;

			free(dec_passwd);
		}
	}

	if (rv == NDMP_NO_ERR)
		ndmpd_log(LOG_DEBUG, "Authorization granted.");
	else
		ndmpd_log(LOG_ERR, "Authorization denied.");

	return (rv);
}

/*
 * ndmpd_connect_auth_md5
 *
 * Checks MD5 authorization.
 *
 * Parameters:
 *   auth_id (input) - user name
 *   auth_digest(input) - MD5 digest
 * 	This is a 16 bytes digest info which is a MD5 transform of 128 bytes
 * 	message (password + padding + server challenge + password). Server
 * 	challenge is a 64 bytes random string per NDMP session sent out to the
 * 	client on demand (See NDMP_CONFIG_GET_AUTH_ATTR command).
 *
 * Returns:
 *   NDMP_NO_ERR: on success
 *   Other NDMP_ error: invalid user name and password
 */
int
ndmpd_connect_auth_md5(char *uname, char *auth_id, char *auth_digest,
    unsigned char *auth_challenge)
{
	char *passwd, *dec_passwd;
	unsigned char digest[16];
	int rv;

	if (strcmp(uname, auth_id) != 0)
		rv = NDMP_NOT_AUTHORIZED_ERR;
	else {
		passwd = ndmpd_get_prop(NDMP_CRAM_MD5_PASSWORD);
		if (passwd == NULL || *passwd == 0)
			rv = NDMP_NOT_AUTHORIZED_ERR;
		else {
			dec_passwd = ndmp_base64_decode(passwd);

			if (dec_passwd == NULL || *dec_passwd == 0) {
				rv = NDMP_NOT_AUTHORIZED_ERR;
			} else {
				create_md5_digest(digest, dec_passwd, auth_challenge);

				if (memcmp(digest, auth_digest,
				    sizeof (digest)) != 0) {
					rv = NDMP_NOT_AUTHORIZED_ERR;
				} else {
					rv = NDMP_NO_ERR;
				}
			}
			free(dec_passwd);
		}
	}

	if (rv == NDMP_NO_ERR) {
		ndmpd_log(LOG_DEBUG, "Authorization granted.");
	} else {
		ndmpd_log(LOG_ERR, "Authorization denied.");
	}

	return (rv);
}
