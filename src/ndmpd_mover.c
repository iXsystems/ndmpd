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

#include <ndmp.h>
#include <ndmpd.h>
#include <ndmpd_util.h>
#include <ndmpd_func.h>
#include <ndmpd_session.h>


#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mtio.h>



static int	discard_data_v3(ndmpd_session_t *session, u_long length);

int ndmp_max_mover_recsize = MAX_MOVER_RECSIZE; /* patchable */



/*
 * ************************************************************************
 * NDMP V3 HANDLERS
 * ************************************************************************
 */

/*
 * ndmpd_mover_get_state_v3
 *
 * This handler handles the ndmp_mover_get_state_request.
 * Status information for the mover state machine is returned.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
///*ARGSUSED*/
//void
//ndmpd_mover_get_state_v3(ndmp_connection_t *connection, void *body)
//{
//}
//
//
///*
// * ndmpd_mover_listen_v3
// *
// * This handler handles ndmp_mover_listen_requests.
// * A TCP/IP socket is created that is used to listen for
// * and accept data connections initiated by a remote
// * data server.
// *
// * Parameters:
// *   connection (input) - connection handle.
// *   body       (input) - request message body.
// *
// * Returns:
// *   void
// */
//void
//ndmpd_mover_listen_v3(ndmp_connection_t *connection, void *body)
//{
//}
//
//
///*
// * ndmpd_mover_continue_v3
// *
// * This handler handles ndmp_mover_continue_requests.
// *
// * Parameters:
// *   connection (input) - connection handle.
// *   body       (input) - request message body.
// *
// * Returns:
// *   void
// */
///*ARGSUSED*/
//void
//ndmpd_mover_continue_v3(ndmp_connection_t *connection, void *body)
//{
//}
//
//
///*
// * ndmpd_mover_abort_v3
// *
// * This handler handles mover_abort requests.
// *
// * Parameters:
// *   connection (input) - connection handle.
// *   body       (input) - request message body.
// *
// * Returns:
// *   void
// */
///*ARGSUSED*/
//void
//ndmpd_mover_abort_v3(ndmp_connection_t *connection, void *body)
//{
//}
//
//
///*
// * ndmpd_mover_set_window_v3
// *
// * This handler handles mover_set_window requests.
// *
// *
// * Parameters:
// *   connection (input) - connection handle.
// *   body       (input) - request message body.
// *
// * Returns:
// *   void
// */
//void
//ndmpd_mover_set_window_v3(ndmp_connection_t *connection, void *body)
//{
//}
//
//
///*
// * ndmpd_mover_read_v3
// *
// * This handler handles ndmp_mover_read_requests.
// * If the requested offset is outside of the current window, the mover
// * is paused and a notify_mover_paused request is sent notifying the
// * client that a seek is required. If the requested offest is within
// * the window but not within the current record, then the tape is
// * positioned to the record containing the requested offest. The requested
// * amount of data is then read from the tape device and written to the
// * data connection.
// *
// * Parameters:
// *   connection (input) - connection handle.
// *   body       (input) - request message body.
// *
// * Returns:
// *   void
// */
//void
//ndmpd_mover_read_v3(ndmp_connection_t *connection, void *body)
//{
//}
//
//
///*
// * ndmpd_mover_set_record_size_v3
// *
// * This handler handles mover_set_record_size requests.
// *
// * Parameters:
// *   connection (input) - connection handle.
// *   body       (input) - request message body.
// *
// * Returns:
// *   void
// */
//void
//ndmpd_mover_set_record_size_v3(ndmp_connection_t *connection, void *body)
//{
//}
//
//
///*
// * ndmpd_mover_connect_v3
// *   Request handler. Connects the mover to either a local
// *   or remote data server.
// *
// * Parameters:
// *   connection (input) - connection handle.
// *   body       (input) - request message body.
// *
// * Returns:
// *   void
// */
//void
//ndmpd_mover_connect_v3(ndmp_connection_t *connection, void *body)
//{
//}
//
//
///*
// * ************************************************************************
// * NDMP V4 HANDLERS
// * ************************************************************************
// */
//
///*
// * ndmpd_mover_get_state_v4
// *
// * This handler handles the ndmp_mover_get_state_request.
// * Status information for the mover state machine is returned.
// *
// * Parameters:
// *   connection (input) - connection handle.
// *   body       (input) - request message body.
// *
// * Returns:
// *   void
// */
///*ARGSUSED*/
//void
//ndmpd_mover_get_state_v4(ndmp_connection_t *connection, void *body)
//{
//}
//
//
///*
// * ndmpd_mover_listen_v4
// *
// * This handler handles ndmp_mover_listen_requests.
// * A TCP/IP socket is created that is used to listen for
// * and accept data connections initiated by a remote
// * data server.
// *
// * Parameters:
// *   connection (input) - connection handle.
// *   body       (input) - request message body.
// *
// * Returns:
// *   void
// */
//void
//ndmpd_mover_listen_v4(ndmp_connection_t *connection, void *body)
//{
//}
//
///*
// * ndmpd_mover_connect_v4
// *   Request handler. Connects the mover to either a local
// *   or remote data server.
// *
// * Parameters:
// *   connection (input) - connection handle.
// *   body       (input) - request message body.
// *
// * Returns:
// *   void
// */
//void
//ndmpd_mover_connect_v4(ndmp_connection_t *connection, void *body)
//{
//}


/*
 * ndmpd_local_write
 *
 * Writes data to the mover.
 * Buffers and write data to the tape device.
 * A full tape record is buffered before being written.
 *
 * Parameters:
 *   session    (input) - session pointer.
 *   data       (input) - data to be written.
 *   length     (input) - data length.
 *
 * Returns:
 *   0 - data successfully written.
 *  -1 - error.
 */
int
ndmpd_local_write(ndmpd_session_t *session, char *data, u_long length)
{
	return (0);
}

/*
 * ndmpd_remote_write
 *
 * Writes data to the remote mover.
 *
 * Parameters:
 *   session    (input) - session pointer.
 *   data       (input) - data to be written.
 *   length     (input) - data length.
 *
 * Returns:
 *   0 - data successfully written.
 *  -1 - error.
 */
int
ndmpd_remote_write(ndmpd_session_t *session, char *data, u_long length)
{
	ssize_t n;
	u_long count = 0;

	while (count < length) {
		if (session->ns_eof == TRUE ||
		    session->ns_data.dd_abort == TRUE)
			return (-1);

		if ((n = write(session->ns_data.dd_sock, &data[count], length - count)) < 0) {
			ndmpd_log(LOG_ERR, "Socket write error: %m.");
			session->ns_data.dd_abort = TRUE;
			return (-1);
		}
		count += n;

	}


	return (0);
}

/*
 * ndmpd_local_read
 *
 * Reads data from the local tape device.
 * Full tape records are read and buffered.
 *
 * Parameters:
 *   session (input) - session pointer.
 *   data    (input) - location to store data.
 *   length  (input) - data length.
 *
 * Returns:
 *   0 - data successfully read.
 *  -1 - error.
 *   1 - session terminated or operation aborted.
 */
int
ndmpd_local_read(ndmpd_session_t *session, char *data, u_long length)
{
	return (0);
}



/* *** ndmpd internal functions ***************************************** */

/*
 * ndmpd_mover_init
 *
 * Initialize mover specific session variables.
 * Don't initialize variables such as record_size that need to
 * persist across data operations. A client may open a connection and
 * do multiple backups after setting the record_size.
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   0 - success.
 *  -1 - error.
 */
int
ndmpd_mover_init(ndmpd_session_t *session)
{
	session->ns_mover.md_state = NDMP_MOVER_STATE_IDLE;
	session->ns_mover.md_pause_reason = NDMP_MOVER_PAUSE_NA;
	session->ns_mover.md_halt_reason = NDMP_MOVER_HALT_NA;
	session->ns_mover.md_data_written = 0LL;
	session->ns_mover.md_seek_position = 0LL;
	session->ns_mover.md_bytes_left_to_read = 0LL;
	session->ns_mover.md_window_offset = 0LL;
	session->ns_mover.md_window_length = MAX_WINDOW_SIZE;
	session->ns_mover.md_position = 0LL;
	session->ns_mover.md_discard_length = 0;
	session->ns_mover.md_record_num = 0;
	session->ns_mover.md_record_size = 0;
	session->ns_mover.md_listen_sock = -1;
	session->ns_mover.md_pre_cond = FALSE;
	session->ns_mover.md_sock = -1;
	session->ns_mover.md_r_index = 0;
	session->ns_mover.md_w_index = 0;
	session->ns_mover.md_buf = ndmp_malloc(MAX_RECORD_SIZE);
	if (!session->ns_mover.md_buf)
		return (-1);

	if (ndmp_get_version(session->ns_connection) == NDMPV3) {
		session->ns_mover.md_mode = NDMP_MOVER_MODE_READ;
		(void) memset(&session->ns_mover.md_data_addr, 0,
		    sizeof (ndmp_addr_v3));
	}
	return (0);
}


/*
 * ndmpd_mover_shut_down
 *
 * Shutdown the mover. It closes all the sockets.
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   void
 */
void
ndmpd_mover_shut_down(ndmpd_session_t *session)
{
	if (session->ns_mover.md_listen_sock != -1) {
		ndmpd_log(LOG_DEBUG, "mover.listen_sock: %d",
		    session->ns_mover.md_listen_sock);

		(void) ndmpd_remove_file_handler(session, session->ns_mover.md_listen_sock);
		(void) close(session->ns_mover.md_listen_sock);
		session->ns_mover.md_listen_sock = -1;
	}
	if (session->ns_mover.md_sock != -1) {
		ndmpd_log(LOG_DEBUG, "mover.sock: %d", session->ns_mover.md_sock);
		(void) ndmpd_remove_file_handler(session, session->ns_mover.md_sock);
		(void) close(session->ns_mover.md_sock);
		session->ns_mover.md_sock = -1;
	}
}


/*
 * ndmpd_mover_cleanup
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   void
 */
void
ndmpd_mover_cleanup(ndmpd_session_t *session)
{
	NDMP_FREE(session->ns_mover.md_buf);
}



/*
 * ndmpd_mover_error_send
 *
 * This function sends the notify message to the client.
 *
 * Parameters:
 *   session (input) - session pointer.
 *   reason  (input) - halt reason.
 *
 * Returns:
 *   Error code
 */
int
ndmpd_mover_error_send(ndmpd_session_t *session, ndmp_mover_halt_reason reason)
{
	ndmp_notify_mover_halted_request req;

	req.reason = reason;
	req.text_reason = "";

	return (ndmp_send_request(session->ns_connection,
	    NDMP_NOTIFY_MOVER_HALTED, NDMP_NO_ERR, (void *)&req, 0));
}


/*
 * ndmpd_mover_error_send_v4
 *
 * This function sends the notify message to the client.
 *
 * Parameters:
 *   session (input) - session pointer.
 *   reason  (input) - halt reason.
 *
 * Returns:
 *   Error code
 */
int
ndmpd_mover_error_send_v4(ndmpd_session_t *session,
    ndmp_mover_halt_reason reason)
{
	ndmp_notify_mover_halted_request_v4 req;

	req.reason = reason;

	return (ndmp_send_request(session->ns_connection,
	    NDMP_NOTIFY_MOVER_HALTED, NDMP_NO_ERR, (void *)&req, 0));
}


/*
 * ndmpd_mover_error
 *
 * This function is called when an unrecoverable mover error
 * has been detected. A notify message is sent to the client and the
 * mover is placed into the halted state.
 *
 * Parameters:
 *   session (input) - session pointer.
 *   reason  (input) - halt reason.
 *
 * Returns:
 *   void.
 */
void
ndmpd_mover_error(ndmpd_session_t *session, ndmp_mover_halt_reason reason)
{
	if (session->ns_mover.md_state == NDMP_MOVER_STATE_HALTED ||
	    (session->ns_protocol_version > NDMPV2 &&
	    session->ns_mover.md_state == NDMP_MOVER_STATE_IDLE))
		return;

	if (session->ns_protocol_version == NDMPV4) {
		if (ndmpd_mover_error_send_v4(session, reason) < 0)
			ndmpd_log(LOG_DEBUG,
			    "Error sending notify_mover_halted request");
	} else {
		/* No media error in V3 */
		if (reason == NDMP_MOVER_HALT_MEDIA_ERROR)
			reason = NDMP_MOVER_HALT_INTERNAL_ERROR;
		if (ndmpd_mover_error_send(session, reason) < 0)
			ndmpd_log(LOG_DEBUG,
			    "Error sending notify_mover_halted request");
	}

	if (session->ns_mover.md_listen_sock != -1) {
		(void) ndmpd_remove_file_handler(session,
		    session->ns_mover.md_listen_sock);
		(void) close(session->ns_mover.md_listen_sock);
		session->ns_mover.md_listen_sock = -1;
	}
	if (session->ns_mover.md_sock != -1) {
		(void) ndmpd_remove_file_handler(session,
		    session->ns_mover.md_sock);
		(void) close(session->ns_mover.md_sock);
		session->ns_mover.md_sock = -1;
	}

	session->ns_mover.md_state = NDMP_MOVER_STATE_HALTED;
	session->ns_mover.md_halt_reason = reason;
}

/*
 * ndmpd_local_write_v3
 *
 * Buffers and writes data to the tape device.
 * A full tape record is buffered before being written.
 *
 * Parameters:
 *   session    (input) - session pointer.
 *   data       (input) - data to be written.
 *   length     (input) - data length.
 *
 * Returns:
 *   0 - data successfully written.
 *  -1 - error.
 */
int
ndmpd_local_write_v3(ndmpd_session_t *session, char *data, u_long length)
{
	return (0);
}



/*
 * ndmpd_local_read_v3
 *
 * Reads data from the local tape device.
 * Full tape records are read and buffered.
 *
 * Parameters:
 *   session (input) - session pointer.
 *   data    (input) - location to store data.
 *   length  (input) - data length.
 *
 * Returns:
 *   1 - no read error but no writer running
 *   0 - data successfully read.
 *  -1 - error.
 */
int
ndmpd_local_read_v3(ndmpd_session_t *session, char *data, u_long length)
{

	return (0);
}

/*
 * discard_data_v3
 *
 * Read and discard data from the data connection.
 * Called when a module has called ndmpd_seek() prior to
 * reading all of the data from the previous seek.
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   number of bytes read and discarded.
 *  -1 - error.
 */
static int
discard_data_v3(ndmpd_session_t *session, u_long length)
{
	static char buf[MAX_RECORD_SIZE];
	int n, toread;

	toread = (length < MAX_RECORD_SIZE) ? length :
	    MAX_RECORD_SIZE;

	/* Read and discard the data. */
	n = read(session->ns_data.dd_sock, buf, toread);
	if (n < 0) {
		ndmpd_log(LOG_ERR, "Socket read error: %m.");
		n = -1;
	}

	return (n);
}


/*
 * ndmpd_remote_read_v3
 *
 * Reads data from the remote mover.
 *
 * Parameters:
 *   session (input) - session pointer.
 *   data    (input) - data to be written.
 *   length  (input) - data length.
 *
 * Returns:
 *   0 - data successfully read.
 *  -1 - error.
 */
int
ndmpd_remote_read_v3(ndmpd_session_t *session, char *data, u_long length)
{
	u_long count;
	u_long len;
	ssize_t n;
	ndmp_notify_data_read_request request;
	tlm_job_stats_t *jstat;
	longlong_t fsize;


	count = 0;
	while (count < length) {

		len = length - count;

		/*
		 * If the end of the seek window has been reached then
		 * send an ndmp_read request to the client.
		 * The NDMP client will then send a mover_data_read request to
		 * the remote mover and the mover will send more data.
		 * This condition can occur if the module attempts to read past
		 * a seek window set via a prior call to ndmpd_seek() or
		 * the module has not issued a seek. If no seek was issued then
		 * pretend that a seek was issued to read the entire tape.
		 */
		if (session->ns_data.dd_bytes_left_to_read == 0) {
			/* ndmpd_seek() never called? */
			if (session->ns_data.dd_read_length == 0) {
				session->ns_data.dd_bytes_left_to_read = ~0LL;
				session->ns_data.dd_read_offset = 0LL;
				session->ns_data.dd_read_length = ~0LL;
			} else {
				/*
				 * While restoring a file, restoreFile()
				 * records the number of bytes still need to
				 * be restored.  We use this as a guidance
				 * when asking for data from the tape.
				 */
				jstat = session->ns_ndmp_lbr_params->nlp_jstat;
				fsize = jstat->js_bytes_in_file;

				ndmpd_log(LOG_DEBUG, "bytes_left [%llu / %lu]",
				    fsize, len);

				/*
				 * Fall back to the old way if fsize if too
				 * small.
				 */
				if (fsize < len)
					fsize = len;

				session->ns_data.dd_bytes_left_to_read = fsize;
				session->ns_data.dd_read_offset =
				    session->ns_data.dd_position;
				session->ns_data.dd_read_length = fsize;
			}

			request.offset =
			    long_long_to_quad(session->ns_data.dd_read_offset);
			request.length =
			    long_long_to_quad(session->ns_data.dd_read_length);

			ndmpd_log(LOG_DEBUG, "to NOTIFY_DATA_READ [%llu, %llu]",
			    session->ns_data.dd_read_offset,
			    session->ns_data.dd_read_length);

			if (ndmp_send_request_lock(session->ns_connection,
			    NDMP_NOTIFY_DATA_READ, NDMP_NO_ERR,
			    &request, 0) < 0) {
				ndmpd_log(LOG_DEBUG,
				    "Sending notify_data_read request");
				return (-1);
			}
		}

		/*
		 * If the module called ndmpd_seek() prior to reading all of the
		 * data that the remote mover was requested to send, then the
		 * excess data from the seek has to be discarded.
		 */
		if (session->ns_data.dd_discard_length != 0) {
			n = discard_data_v3(session,
			    (u_long)session->ns_data.dd_discard_length);
			if (n < 0)
				return (-1);

			session->ns_data.dd_discard_length -= n;
			continue;
		}

		/*
		 * Don't attempt to read more data than the remote is sending.
		 */
		if (len > session->ns_data.dd_bytes_left_to_read)
			len = session->ns_data.dd_bytes_left_to_read;

		if ((n = read(session->ns_data.dd_sock, &data[count],
		    len)) < 0) {
			ndmpd_log(LOG_ERR, "Socket read error: %m.");
			return (-1);
		}

		/* read returns 0 if the connection was closed */
		if (n == 0) {
			ndmpd_log(LOG_DEBUG, "n 0 errno %d",
			    errno);
			return (-1);
		}

		count += n;
		session->ns_data.dd_bytes_left_to_read -= n;
		session->ns_data.dd_position += n;
	}
	return (0);
}

