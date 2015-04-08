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

#include "ndmpd.h"
#include "ndmpd_func.h"
#include "ndmpd_util.h"
#include "ndmpd_table.h"

#include "signal.h"

#include <assert.h>


int ndmp_process_messages(ndmp_connection_t *connection, bool_t reply_expected);

/* we do not support NDMP version 1, start from NDMP version 2 */
int ndmp_ver = 2;

/*
 * The NDMP listening port number
 */
int ndmp_port = 0;

/*
 * Restore path mechanism definition
 * 0 means partial path restore and
 * 1 means full path restore.
 * Refer to NDMP_FULL_RESTORE_PATH for partial path and full path definition.
 */
int ndmp_full_restore_path = 1;


/* Debug Function START	*/
void
printXDR(ndmp_connection_t *chanlde, 
	ndmp_notify_connected_request *request_data){
	if (request_data) {
		ndmpd_log(LOG_DEBUG,"ndmp_notify_connected_request reason = %d",
			(request_data->reason));
		ndmpd_log(LOG_DEBUG,"ndmp_notify_connected_request protocol_version = %d ",
			(request_data->protocol_version));
		ndmpd_log(LOG_DEBUG,"ndmp_notify_connected_request text_reason = %s ",
			(request_data->text_reason));
	}
	
	ndmpd_log(LOG_DEBUG, "connection->conn_sock = %d",chanlde->conn_sock);
	ndmpd_log(LOG_DEBUG, "connection->conn_xdrs");
	
	if (chanlde->conn_xdrs.x_op==XDR_ENCODE)
		ndmpd_log(LOG_DEBUG, "connection->conn_xdrs = XDR_ENCODE");
	else if (chanlde->conn_xdrs.x_op==XDR_DECODE)
		ndmpd_log(LOG_DEBUG, "connection->conn_xdrs = XDR_DECODE");
	else
		ndmpd_log(LOG_DEBUG, "connection->conn_xdrs = %d", chanlde->conn_xdrs.x_op);
	
	ndmpd_log(LOG_DEBUG, "connection->conn_my_sequence = %lu", chanlde->conn_my_sequence);
	ndmpd_log(LOG_DEBUG, "connection->conn_authorized = %d", chanlde->conn_authorized);
	ndmpd_log(LOG_DEBUG, "connection->conn_eof = %d", chanlde->conn_eof);
	ndmpd_log(LOG_DEBUG, "connection->conn_msginfo");
	ndmpd_log(LOG_DEBUG, "connection->conn_msginfo.mi_hdr");
	ndmpd_log(LOG_DEBUG, "connection->conn_msginfo.mi_hdr.sequence = %lu",
		chanlde->conn_msginfo.mi_hdr.sequence);
	ndmpd_log(LOG_DEBUG, "connection->conn_msginfo.mi_hdr.time_stamp = %lu",
		chanlde->conn_msginfo.mi_hdr.time_stamp);
	ndmpd_log(LOG_DEBUG, "connection->conn_msginfo.mi_hdr.message_type = %x",
		chanlde->conn_msginfo.mi_hdr.message_type);
	ndmpd_log(LOG_DEBUG, "connection->conn_msginfo.mi_hdr.message = %x",
		chanlde->conn_msginfo.mi_hdr.message);
	ndmpd_log(LOG_DEBUG, "connection->conn_msginfo.mi_hdr.reply_sequence = %lu",
		chanlde->conn_msginfo.mi_hdr.reply_sequence);
	ndmpd_log(LOG_DEBUG, "connection->conn_msginfo.mi_hdr.error = %x",
		chanlde->conn_msginfo.mi_hdr.error);
	ndmpd_log(LOG_DEBUG, "connection->conn_msginfo.mi_handler - no way to print it.");
	ndmpd_log(LOG_DEBUG, "connection->conn_version=%d",chanlde->conn_version);
}

/* Debug Function END	*/
void *
ndmp_malloc(size_t size)
{
	void *data;

	if ((data = calloc(1, size)) == NULL)
		ndmpd_log(LOG_ERR, "Out of memory.");

	return (data);
}

/**************   send and receive main function for NDMPD XDR *************************/
/*
 * ndmp_send_response
 *
 * Send an NDMP reply message.
 *
 * Parameters:
 *   connection_handle  (input)  - connection pointer.
 *   err	       (input)  - error code to place in header.
 *   reply	     (input)  - reply message body.
 *
 * Returns:
 *   0 - successful send.
 *  -1 - error.
 *
 * Notes:
 *   - The body is only sent if the error code is NDMP_NO_ERR.
 */
int 
ndmp_send_response(ndmp_connection_t *connection_handle, ndmp_error err,
    void *reply)
{
	ndmp_connection_t *connection = (ndmp_connection_t *)connection_handle;
	ndmp_header header;
	struct timeval time;

	(void) gettimeofday(&time, 0);

	header.sequence = ++(connection->conn_my_sequence);
	header.time_stamp = time.tv_sec;
	header.message_type = NDMP_MESSAGE_REPLY;
	header.message = connection->conn_msginfo.mi_hdr.message;
	header.reply_sequence = connection->conn_msginfo.mi_hdr.sequence;
	header.error = err;

	connection->conn_xdrs.x_op = XDR_ENCODE;
	if (!xdr_ndmp_header(&connection->conn_xdrs, &header)) {
		ndmpd_log(LOG_DEBUG, "Sending message 0x%x: "
		    "encoding reply header",
		    header.message);
		(void) xdrrec_endofrecord(&connection->conn_xdrs, 1);
		return (-1);
	}
	if (err == NDMP_NO_ERR &&
	    connection->conn_msginfo.mi_handler->mh_xdr_reply &&
	    reply) {
		if (!(*connection->conn_msginfo.mi_handler->mh_xdr_reply)(
		    &connection->conn_xdrs, reply)) {
			ndmpd_log(LOG_DEBUG,
			    "Sending message 0x%x: encoding reply body",
			    header.message);
			(void) xdrrec_endofrecord(&connection->conn_xdrs, 1);
			return (-1);
		}
	}
	(void) xdrrec_endofrecord(&connection->conn_xdrs, 1);
	return (0);
}

/*
 * ndmp_send_request
 *
 * Send an NDMP request message.
 *
 * Parameters:
 *   connection_handle (input) - connection pointer.
 *   message (input) - message number.
 *   err (input)  - error code to place in header.
 *   request_data (input) - message body.
 *   reply (output) - reply message. If 0, reply will be
 *				discarded.
 *
 * Returns:
 *   0	- successful send.
 *  -1	- error.
 *   otherwise - error from reply header.
 *
 * Notes:
 *   - The reply body is only returned if the error code is NDMP_NO_ERR.
 */
int
ndmp_send_request(ndmp_connection_t *connection_handle, ndmp_message message,
    ndmp_error err, void *request_data, void **reply)
{
	ndmp_connection_t *connection = (ndmp_connection_t *)connection_handle;
	ndmp_header header;
	ndmp_msg_handler_t *handler;
	int r;
	struct timeval time;

	/* Lookup info necessary for processing this request. */
	if (!(handler = ndmp_get_handler(connection, message))) {
		ndmpd_log(LOG_DEBUG, "Sending message 0x%x: not supported",
		    message);

		return (-1);
	}

	(void) gettimeofday(&time, 0);

	header.sequence = ++(connection->conn_my_sequence);
	header.time_stamp = time.tv_sec;
	header.message_type = NDMP_MESSAGE_REQUEST;
	header.message = message;
	header.reply_sequence = 0;
	header.error = err;

	connection->conn_xdrs.x_op = XDR_ENCODE;
	/* copy header to XDR structure */
	if (!xdr_ndmp_header(&connection->conn_xdrs, &header)) {
		ndmpd_log(LOG_DEBUG,
		    "Sending message 0x%x: encoding request header", message);
		(void) xdrrec_endofrecord(&connection->conn_xdrs, 1);

		return (-1);
	}

	ndmpd_log(LOG_DEBUG,"got handler");

	if (err == NDMP_NO_ERR && handler->mh_xdr_request && request_data) {
		if (!(*handler->mh_xdr_request)(&connection->conn_xdrs,request_data)) {
			ndmpd_log(LOG_DEBUG,"Sending message 0x%x: encoding request body",message);
			(void) xdrrec_endofrecord(&connection->conn_xdrs, 1);

			return (-1);
		}
	}

	/* flush output	*/
	(void) xdrrec_endofrecord(&connection->conn_xdrs, 1);

	if (handler->mh_xdr_reply == 0) {
		ndmpd_log(LOG_DEBUG, "handler->mh_xdr_reply == 0");

		return (0);
	}

	/*
	 * Process messages until the reply to this request has been
	 * processed.
	 */
	for (;;) {
		ndmpd_log(LOG_DEBUG, "ndmp_process_messages");

		r = ndmp_process_messages(connection, TRUE);

		ndmpd_log(LOG_DEBUG, "ndmp_process_messages result=%d",r);

		/* connection error? */
		if (r == NDMP_PROC_ERR)
			return (NDMP_PROC_ERR);

		/* no reply received? */
		if (r == NDMP_PROC_REP)
			continue;

		/* reply received? */
		if (r == NDMP_PROC_MSG) {
			ndmpd_log(LOG_DEBUG, "message=%x, reply message=%x",
				message,connection->conn_msginfo.mi_hdr.message);

			if (message !=
			    connection->conn_msginfo.mi_hdr.message) {
				ndmpd_log(LOG_DEBUG,
				    "Received unexpected reply 0x%x",
				    connection->conn_msginfo.mi_hdr.message);
				ndmp_free_message(connection_handle);
				return (-1);
			}

			if (reply != NULL)
				*reply = connection->conn_msginfo.mi_body;
			else
				ndmp_free_message(connection_handle);

			return (connection->conn_msginfo.mi_hdr.error);
		}

		/* error handling reply */
		ndmpd_log(LOG_DEBUG, "error handling reply !!!!!!");

		return (-1);
	}
	return(-1);
}

/*
 * A wrapper for ndmp_send_request with locks.
 */
int
ndmp_send_request_lock(ndmp_connection_t *connection_handle,
    ndmp_message message, ndmp_error err, void *request_data, void **reply)
{
	int rv;
	ndmp_connection_t *connection = (ndmp_connection_t *)connection_handle;

	(void) pthread_mutex_lock(&connection->conn_lock);

	rv = ndmp_send_request(connection_handle, message, err, request_data,
	    reply);

	(void) pthread_mutex_unlock(&connection->conn_lock);

	return (rv);
}

/*
 * ndmp_recv_msg
 *
 * Read the next message.
 *
 * Parameters:
 *   connection (input)  - connection pointer.
 *   msg	(output) - received message.
 *
 * Returns:
 *   0 - Message successfully received.
 *   error number - Message related error.
 *  -1 - Error decoding the message header.
 */
int
ndmp_recv_msg(ndmp_connection_t *connection)
{
	bool_t(*xdr_func) (XDR *, ...) = NULL;

	/* Decode the header. */
	connection->conn_xdrs.x_op = XDR_DECODE;

	(void) xdrrec_skiprecord(&connection->conn_xdrs);

	/* extract header */
	if (!xdr_ndmp_header(&connection->conn_xdrs,
	    &connection->conn_msginfo.mi_hdr))
		return (-1);

	/* Lookup info necessary for processing this message. */
	if ((connection->conn_msginfo.mi_handler = ndmp_get_handler(connection,
	    connection->conn_msginfo.mi_hdr.message)) == 0) {
		ndmpd_log(LOG_DEBUG, "Message 0x%x not supported",
		    connection->conn_msginfo.mi_hdr.message);
		return (NDMP_NOT_SUPPORTED_ERR);
	}
	connection->conn_msginfo.mi_body = 0;

	if (connection->conn_msginfo.mi_hdr.error != NDMP_NO_ERR)
		return (0);

	/* Determine body type */
	if (connection->conn_msginfo.mi_hdr.message_type ==
	    NDMP_MESSAGE_REQUEST) {
		if (ndmp_check_auth_required(
		    connection->conn_msginfo.mi_hdr.message) &&
		    !connection->conn_authorized) {
			ndmpd_log(LOG_DEBUG,
			    "Processing request 0x%x:connection not authorized",
			    connection->conn_msginfo.mi_hdr.message);
			return (NDMP_NOT_AUTHORIZED_ERR);
		}
		if (connection->conn_msginfo.mi_handler->mh_sizeof_request >
		    0) {
			xdr_func =
			    connection->conn_msginfo.mi_handler->mh_xdr_request;
			if (xdr_func == NULL) {
				ndmpd_log(LOG_DEBUG,
				    "Processing request 0x%x: no xdr function "
				    "in handler table",
				    connection->conn_msginfo.mi_hdr.message);
				return (NDMP_NOT_SUPPORTED_ERR);
			}
			connection->conn_msginfo.mi_body = ndmp_malloc(
			    connection->conn_msginfo.mi_handler->
			    mh_sizeof_request);
			if (connection->conn_msginfo.mi_body == NULL)
				return (NDMP_NO_MEM_ERR);

			(void) memset(connection->conn_msginfo.mi_body, 0,
			    connection->conn_msginfo.mi_handler->
			    mh_sizeof_request);
		}
	} else {
		if (connection->conn_msginfo.mi_handler->mh_sizeof_reply > 0) {
			xdr_func =
			    connection->conn_msginfo.mi_handler->mh_xdr_reply;
			if (xdr_func == NULL) {
				ndmpd_log(LOG_DEBUG,
				    "Processing reply 0x%x: no xdr function "
				    "in handler table",
				    connection->conn_msginfo.mi_hdr.message);
				return (NDMP_NOT_SUPPORTED_ERR);
			}
			connection->conn_msginfo.mi_body = ndmp_malloc(
			    connection->conn_msginfo.mi_handler->
			    mh_sizeof_reply);
			if (connection->conn_msginfo.mi_body == NULL)
				return (NDMP_NO_MEM_ERR);

			(void) memset(connection->conn_msginfo.mi_body, 0,
			    connection->conn_msginfo.mi_handler->
			    mh_sizeof_reply);
		}
	}

	/* Decode message arguments if needed */
	if (xdr_func) {
		if (!(*xdr_func)(&connection->conn_xdrs,
		    connection->conn_msginfo.mi_body)) {
			ndmpd_log(LOG_DEBUG,
			    "Processing message 0x%x: error decoding arguments",
			    connection->conn_msginfo.mi_hdr.message);
			free(connection->conn_msginfo.mi_body);
			connection->conn_msginfo.mi_body = 0;
			return (NDMP_XDR_DECODE_ERR);
		}
	}
	return (0);
}

/************** End of send and receive main function for NDMPD XDR *************************/

/*
 * Low level write routine called by the xdrrec library.
 *
 * Parameters:
 *   connection (input) - connection pointer.
 *   buf	(input) - location to store received data.
 *   len	(input) - max number of bytes to read.
 *
 * Returns:
 *   >0 - number of bytes sent.
 *   -1 - error.
 */
int
ndmp_writeit(void *connection_handle, void *buf, int len)
{
	ndmp_connection_t *connection = (ndmp_connection_t *)connection_handle;
	register int n;
	register int cnt;
	
	for (cnt = len; cnt > 0; cnt -= n, buf = (int *)buf + n) {
		if ((n = write(connection->conn_sock, buf, cnt)) < 0) {
			connection->conn_eof = TRUE;
			return (-1);
		}
	}

	return (len);
}

/*
 * Low level read routine called by the xdrrec library.
 *
 * Parameters:
 *   connection (input) - connection pointer.
 *   buf	(input) - location to store received data.
 *   len	(input) - max number of bytes to read.
 *
 * Returns:
 *   >0 - number of bytes received.
 *   -1 - error.
 */
int
ndmp_readit(void *connection_handle, void* buf, int len)
{
	ndmp_connection_t *connection = (ndmp_connection_t *)connection_handle;

	len = read(connection->conn_sock, buf, len);
	if (len <= 0) {
		/* ndmp_connection_t has been closed. */
		connection->conn_eof = TRUE;
		return (-1);
	}
	
	return (len);
}

/*
 * ndmp_free_message
 *
 * Free the memory of NDMP message body.
 *
 * Parameters:
 *   connection_handle  (input)  - connection pointer.
 *
 * Returns:
 *   void
 *
 */
void
ndmp_free_message(ndmp_connection_t *connection_handle)
{
	ndmp_connection_t *connection = (ndmp_connection_t *)connection_handle;

	if (connection->conn_msginfo.mi_handler == NULL ||
	    connection->conn_msginfo.mi_body == NULL)
		return;

	connection->conn_xdrs.x_op = XDR_FREE;
	/*	free the data buffer from XDR.	*/
	if (connection->conn_msginfo.mi_hdr.message_type ==
	    NDMP_MESSAGE_REQUEST) {
		if (connection->conn_msginfo.mi_handler->mh_xdr_request)
			(*connection->conn_msginfo.mi_handler->mh_xdr_request)(
			    &connection->conn_xdrs,
			    connection->conn_msginfo.mi_body);
	} else {
		if (connection->conn_msginfo.mi_handler->mh_xdr_reply)
			(*connection->conn_msginfo.mi_handler->mh_xdr_reply)(
			    &connection->conn_xdrs,
			    connection->conn_msginfo.mi_body);
	}

	(void) free(connection->conn_msginfo.mi_body);
	connection->conn_msginfo.mi_body = 0;
}

/*
 * Allocate and initialize a connection structure for XDR
 *
 * Returns:
 *   NULL - error
 *   connection pointer
 *
 * Notes:
 *   The returned connection should be destroyed using
 *   ndmp_destroy_xdr_connection().
 */
ndmp_connection_t *
ndmp_create_xdr_connection(void)
{
	ndmp_connection_t *connection;

	connection = ndmp_malloc(sizeof (ndmp_connection_t));

	if (connection == NULL)
		return (NULL);

	connection->conn_sock = -1;
	connection->conn_my_sequence = 0;
	connection->conn_authorized = FALSE;
	connection->conn_eof = FALSE;
	connection->conn_msginfo.mi_body = 0;

	/* we don't support version v1 */
	if(ndmp_ver<2)
		connection->conn_version = 2;
	else
		connection->conn_version = ndmp_ver;

	connection->conn_client_data = 0;
	(void) pthread_mutex_init(&connection->conn_lock, NULL);
	connection->conn_xdrs.x_ops = 0;

	xdrrec_create(&connection->conn_xdrs, 0, 0, (caddr_t)connection, ndmp_readit, ndmp_writeit);

	if (connection->conn_xdrs.x_ops == 0) {		
		/* free mutex */
		pthread_mutex_destroy(&connection->conn_lock);
		(void) close(connection->conn_sock);
		free(connection);
		return (0);
	}
	return ((ndmp_connection_t *)connection);
}

/*
 * Shutdown a connection and release allocated resources for XDR
 * 
 *
 *
 * Parameters:
 *   connection_handle (Input) - connection handle.
 *
 * Returns:
 *   void
 */
void
ndmp_destroy_xdr_connection(ndmp_connection_t *connection_handle)
{
	ndmp_connection_t *connection = (ndmp_connection_t *)connection_handle;

	xdr_destroy(&connection->conn_xdrs);

	(void) pthread_mutex_destroy(&connection->conn_lock);

	if (connection->conn_sock >= 0) {
		(void) close(connection->conn_sock);
		connection->conn_sock = -1;
	}
	free(connection);
}

/*
 * ndmp_process_requests
 *
 * Reads the next request message into the stream buffer.
 * Processes messages until the stream buffer is empty.
 *
 * Parameters:
 *   connection_handle (input) - connection handle.
 *
 * Returns:
 *   0 - 1 or more messages successfully processed.
 *  -1 - error; connection no longer established.
 */
int
ndmp_process_requests(ndmp_connection_t *connection_handle)
{
	int rv = 0;
	ndmp_connection_t *connection = (ndmp_connection_t *)connection_handle;

	(void) mutex_lock(&connection->conn_lock);
	if (ndmp_process_messages(connection, FALSE) < 0)
		rv = -1;

	(void) mutex_unlock(&connection->conn_lock);

	return (rv);
}

/*
 * connection_file_handler
 *
 * ndmp_connection_t file handler function.
 * Called by ndmpd_select when data is available to be read on the
 * NDMP connection.
 *
 * Parameters:
 *   cookie (input) - session pointer.
 *   fd      (input) - connection file descriptor.
 *   mode    (input) - select mode.
 *
 * Returns:
 *   void.
 */
/*ARGSUSED*/
void
connection_file_handler(void *cookie, int fd, u_long mode)
{
	ndmpd_session_t *session = (ndmpd_session_t *)cookie;

	if (ndmp_process_requests(session->ns_connection) < 0)
		session->ns_eof = TRUE;
}

/*
 * ndmp_process_messages
 *
 * Reads the next message into the stream buffer.
 * Processes messages until the stream buffer is empty.
 *
 * This function processes all data in the stream buffer before returning.
 * This allows functions like poll() to be used to determine when new
 * messages have arrived. If only some of the messages in the stream buffer
 * were processed and then poll was called, poll() could block waiting for
 * a message that had already been received and read into the stream buffer.
 *
 * This function processes both request and reply messages.
 * Request messages are dispatched using the appropriate function from the
 * message handling table.
 * Only one reply messages may be pending receipt at a time.
 * A reply message, if received, is placed in connection->conn_msginfo
 * before returning to the caller.
 * Errors are reported if a reply is received but not expected or if
 * more than one reply message is received
 *
 * Parameters:
 *   connection     (input)  - connection pointer.
 *   reply_expected (output) - TRUE  - a reply message is expected.
 *			     FALSE - no reply message is expected and
 *			     an error will be reported if a reply
 *			     is received.
 *
 * Returns:
 *   NDMP_PROC_REP_ERR - 1 or more messages successfully processed,
 *   	error processing reply message.
 *   NDMP_PROC_REP_ERR - 1 or more messages successfully processed,
 *	reply seen.
 *   NDMP_PROC_REP_ERR - 1 or more messages successfully processed,
 * 	no reply seen.
 *   NDMP_PROC_REP_ERR - error; connection no longer established.
 *
 * Notes:
 *   If the peer is generating a large number of requests, a caller
 *   looking for a reply will be blocked while the requests are handled.
 *   This is because this function does not return until the stream
 *   buffer is empty.
 *   Code needs to be added to allow a return if the stream buffer
 *   is not empty but there is data available on the socket. This will
 *   prevent poll() from blocking and prevent a caller looking for a reply
 *   from getting blocked by a bunch of requests.
 */
int
ndmp_process_messages(ndmp_connection_t *connection, bool_t reply_expected)
{
	msg_info_t reply_msginfo;
	bool_t reply_read = FALSE;
	bool_t reply_error = FALSE;
	int err;

	ndmpd_log(LOG_DEBUG, "reply_expected: %s",
			reply_expected == TRUE ? "TRUE" : "FALSE");

	(void) memset((void *)&reply_msginfo, 0, sizeof (msg_info_t));

	do {
		(void) memset((void *)&connection->conn_msginfo, 0,
		    sizeof (msg_info_t));

		if ((err = ndmp_recv_msg(connection)) != NDMP_NO_ERR) {
			ndmpd_log(LOG_DEBUG, "ndmp_recv_msg(connection)) != NDMP_NO_ERR");

			if (connection->conn_eof) {
				ndmpd_log(LOG_DEBUG, "detected EOF");
				return (NDMP_PROC_ERR);
			}
			if (err < 1) {
				ndmpd_log(LOG_DEBUG, "error decoding header");

				/*
				 * Error occurred decoding the header.
				 * Don't send a reply since we don't know
				 * the message or if the message was even
				 * a request message.  To be safe, assume
				 * that the message was a reply if a reply
				 * was expected. Need to do this to prevent
				 * hanging ndmp_send_request() waiting for a
				 * reply.  Don't set reply_read so that the
				 * reply will be processed if it is received
				 * later.
				 */
				if (reply_read == FALSE)
					reply_error = TRUE;

				continue;
			}
			if (connection->conn_msginfo.mi_hdr.message_type != NDMP_MESSAGE_REQUEST) {
				ndmpd_log(LOG_DEBUG, "received reply: 0x%x",
					connection->conn_msginfo.mi_hdr.message);
				if (reply_expected == FALSE ||
				    reply_read == TRUE)
					ndmpd_log(LOG_DEBUG,"Unexpected reply message: 0x%x",
						connection->conn_msginfo.mi_hdr.message);

				ndmp_free_message((ndmp_connection_t *)connection);

				if (reply_read == FALSE) {
					reply_read = TRUE;
					reply_error = TRUE;
				}
				continue;
			}
			
			ndmpd_log(LOG_DEBUG, "received request: 0x%x",
				connection->conn_msginfo.mi_hdr.message);

			(void) ndmp_send_response((ndmp_connection_t *)connection, err, NULL);
			
			ndmp_free_message((ndmp_connection_t *)connection);
			
			continue;
		}
		if (connection->conn_msginfo.mi_hdr.message_type!= NDMP_MESSAGE_REQUEST) {
			ndmpd_log(LOG_DEBUG, "received reply: 0x%x",
				connection->conn_msginfo.mi_hdr.message);

			if (reply_expected == FALSE || reply_read == TRUE) {
			
				ndmpd_log(LOG_DEBUG,"Unexpected reply message: 0x%x",
					connection->conn_msginfo.mi_hdr.message);
				
				ndmp_free_message((ndmp_connection_t *)connection);
				continue;
			}
			reply_read = TRUE;
			reply_msginfo = connection->conn_msginfo;
			continue;
		}
		
		ndmpd_log(LOG_DEBUG, "received request: 0x%x",
			connection->conn_msginfo.mi_hdr.message);

		/*
		 * The following is needed to catch an improperly constructed
		 * handler table or to deal with an NDMP client that is not
		 * conforming to the negotiated protocol version.
		 */
		 
		
		/* check if the handler exist and assigned. */
		if (connection->conn_msginfo.mi_handler == NULL ||
			connection->conn_msginfo.mi_handler->mh_func == NULL) {
			ndmpd_log(LOG_DEBUG, "No handler for message 0x%x",
				connection->conn_msginfo.mi_hdr.message);

			(void) ndmp_send_response((ndmp_connection_t *)connection,
				NDMP_NOT_SUPPORTED_ERR, NULL);
			/* free allocated memory before leave the function */
			ndmp_free_message((ndmp_connection_t *)connection);
			continue;
		}
		/*
		 * Call the handler function.
		 * The handler will send any necessary reply.
		 */
		(*connection->conn_msginfo.mi_handler->mh_func)
			(connection,connection->conn_msginfo.mi_body);

		/* free allocated memory before leave the function */
		ndmp_free_message((ndmp_connection_t *)connection);


	} while (xdrrec_eof(&connection->conn_xdrs) == FALSE &&
	    connection->conn_eof == FALSE);

	if (connection->conn_eof == TRUE) {
		if (reply_msginfo.mi_body)
			free(reply_msginfo.mi_body);
		return (NDMP_PROC_ERR);
	}
	if (reply_error) {
		if (reply_msginfo.mi_body)
			free(reply_msginfo.mi_body);
		return (NDMP_PROC_REP_ERR);
	}
	if (reply_read) {
		connection->conn_msginfo = reply_msginfo;
		return (NDMP_PROC_MSG);
	}
	return (NDMP_PROC_REP);
}

/*
 * tcp_accept
 *
 * A wrapper around accept for retrying and getting the IP address
 *
 * Parameters:
 *   listen_sock (input) - the socket for listening
 *   inaddr_p (output) - the IP address of peer connection
 *
 * Returns:
 *   socket for the accepted connection
 *   -1: error
 */
int
tcp_accept(int listen_sock, unsigned int *inaddr_p)
{
	struct sockaddr_in	sin;
	int			sock;
	socklen_t	sl;
	int			try;

	for (try = 0; try < 3; try++) {
		sl = sizeof (sin);
		sock = accept(listen_sock, (struct sockaddr *)&sin, &sl);
		if (sock < 0) {
			continue;
		}
		*inaddr_p = sin.sin_addr.s_addr;
		return (sock);
	}
	return (-1);
}

/*
 * tcp_get_peer
 *
 * Get the peer IP address for a connection
 *
 * Parameters:
 *   sock (input) - the active socket
 *   inaddr_p (output) - the IP address of peer connection
 *   port_p (output) - the port number of peer connection
 *
 * Returns:
 *   socket for the accepted connection
 *   -1: error
 */
int
tcp_get_peer(int sock, unsigned int *inaddr_p, int *port_p)
{
	struct sockaddr_in sin;
	int  rc;
	socklen_t length;

	length = sizeof (sin);
	rc = getpeername(sock, (struct sockaddr *)&sin, &length);
	if (rc != 0)
		return (-1);

	if (inaddr_p)
		*inaddr_p = sin.sin_addr.s_addr;

	if (port_p)
		*port_p = ntohs(sin.sin_port);

	return (sock);

}

/*
 * ndmp_get_fd
 *
 * Returns the connection file descriptor.
 *
 * Parameters:
 *   connection_handle (input) - connection handle
 *
 * Returns:
 *   >=0 - file descriptor.
 *   -1  - connection not open.
 */
int
ndmp_get_fd(ndmp_connection_t *connection_handle)
{
	return (((ndmp_connection_t *)connection_handle)->conn_sock);
}

/*
 * ndmp_set_client_data
 *
 * This function provides a means for the library client to provide
 * a pointer to some user data structure that is retrievable by
 * each message handler via ndmp_get_client_data.
 *
 * Parameters:
 *   connection_handle  (input) - connection handle.
 *   client_data	(input) - user data pointer.
 *
 * Returns:
 *   void
 */
void
ndmp_set_client_data(ndmp_connection_t *connection_handle, void *client_data)
{
	((ndmp_connection_t *)connection_handle)->conn_client_data =
	    client_data;
}

/*
 * ndmp_get_client_data
 *
 * This function provides a means for the library client to provide
 * a pointer to some user data structure that is retrievable by
 * each message handler via ndmp_get_client_data.
 *
 * Parameters:
 *   connection_handle (input) - connection handle.
 *
 * Returns:
 *   client data pointer.
 */
void *
ndmp_get_client_data(ndmp_connection_t *connection_handle)
{
	return (((ndmp_connection_t *)connection_handle)->conn_client_data);
}

/*
 * ndmp_set_version
 *
 * Sets the NDMP protocol version to be used on the connection.
 *
 * Parameters:
 *   connection_handle  (input) - connection handle.
 *   version	   (input) - protocol version.
 *
 * Returns:
 *   void
 */
void
ndmp_set_version(ndmp_connection_t *connection_handle, u_short version)
{
	((ndmp_connection_t *)connection_handle)->conn_version = version;
}

/*
 * ndmp_get_version
 *
 * Gets the NDMP protocol version in use on the connection.
 *
 * Parameters:
 *   connection_handle  (input) - connection handle.
 *   version	   (input) - protocol version.
 *
 * Returns:
 *   void
 */
u_short
ndmp_get_version(ndmp_connection_t *connection_handle)
{
	return (((ndmp_connection_t *)connection_handle)->conn_version);
}

/*
 * ndmp_set_authorized
 *
 * Mark the connection as either having been authorized or not.
 *
 * Parameters:
 *   connection_handle  (input) - connection handle.
 *   authorized	(input) - TRUE or FALSE.
 *
 * Returns:
 *   void
 */
void
ndmp_set_authorized(ndmp_connection_t *connection_handle, bool_t authorized)
{
	((ndmp_connection_t *)connection_handle)->conn_authorized = authorized;
}

/*
 * ndmp_check_auth_required
 *
 * Check if the connection needs to be authenticated before
 * this message is being processed.
 *
 * Parameters:
 *   message (input) - message number.
 *
 * Returns:
 *   TRUE - required
 *   FALSE - not required
 */
bool_t
ndmp_check_auth_required(ndmp_message message)
{
	bool_t auth_req = FALSE;
	ndmp_handler_t *ni = ndmp_get_interface(message);

	if (ni)
		auth_req = ni->hd_msgs[message & 0xff].hm_auth_required;

	return (auth_req);
}

/*
 * ndmp_get_interface
 *
 * Return the NDMP interface (e.g. config, scsi, tape) for the
 * specific message.
 *
 * Parameters:
 *   message (input) - message number.
 *
 * Returns:
 *   NULL - message not found.
 *   pointer to handler info.
 */
ndmp_handler_t *
ndmp_get_interface(ndmp_message message)
{
	ndmp_handler_t *ni = &ndmp_msghdl_tab[(message >> 8) % INT_MAXCMD];

	if ((message & 0xff) >= (unsigned int)ni->hd_cnt)
		return (NULL);

	/* Sanity check */
	if (ni->hd_msgs[message & 0xff].hm_message != message){
		return (NULL);

	}

	return (ni);
}

/*
 * ndmp_get_handler
 *
 * Return the handler info for the specified NDMP message.
 *
 * Parameters:
 *   connection (input) - connection pointer.
 *   message (input) - message number.
 *
 * Returns:
 *   NULL - message not found.
 *   pointer to handler info.
 */
ndmp_msg_handler_t *
ndmp_get_handler(ndmp_connection_t *connection, ndmp_message message)
{
	ndmp_msg_handler_t *handler = NULL;

	ndmp_handler_t *ni = ndmp_get_interface(message);
	int ver = connection->conn_version;

	ndmpd_log(LOG_DEBUG, "protocol version=%d",ver);

	/* we don't support v1, we'll handle here just for safety.	*/
	if(ver<2)
		ver=2;
	if (ni)
		handler = &ni->hd_msgs[message & 0xff].hm_msg_v[ver - 2];

	return (handler);
}

/*
 * ndmp_close
 *
 * Close a connection.
 *
 * Parameters:
 *   connection_handle (Input) - connection handle.
 *
 * Returns:
 *   void
 */
void
ndmp_close(ndmp_connection_t *connection_handle)
{
	ndmp_connection_t *connection = (ndmp_connection_t *)connection_handle;

	if (connection->conn_sock >= 0) {
		(void) mutex_destroy(&connection->conn_lock);
		(void) close(connection->conn_sock);
		connection->conn_sock = -1;
	}
	connection->conn_eof = TRUE;
}
