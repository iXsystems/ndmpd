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

#ifndef _NDMPD_FUNC_H_
#define	_NDMPD_FUNC_H_

#include <ndmpd.h>
#include <ndmpd_session.h>

/* gethostbyname */
#include <netdb.h>
/* inet_ntoa */
#include <arpa/inet.h>

#define	VOL_MAXNAMELEN	256

extern ndmp_handler_t ndmp_msghdl_tab[];
extern int ndmp_port;

int  ndmp_send_response(ndmp_connection_t *connection_handle, ndmp_error err, void *reply);
int  ndmp_send_request(ndmp_connection_t *connection_handle,
	ndmp_message message,ndmp_error err, void *request_data, void **reply);
int  ndmp_send_request_lock(ndmp_connection_t *connection_handle, ndmp_message message,
	ndmp_error err, void *request_data, void **reply);
int  ndmp_recv_msg(ndmp_connection_t *connection);
int ndmp_process_messages(ndmp_connection_t *connection, bool_t reply_expected);
void *ndmp_malloc(size_t size);
int ndmp_writeit(void *connection_handle, void* buf, int len);
int ndmp_readit(void *connection_handle, void* buf, int len);
void ndmp_free_message(ndmp_connection_t *connection_handle);
ndmp_connection_t *ndmp_create_xdr_connection(void);
void ndmp_destroy_xdr_connection(ndmp_connection_t *connection_handle);
int ndmp_process_requests(ndmp_connection_t *connection_handle);
void connection_file_handler(void *cookie, int fd, u_long mode);
//void connection_file_handler(void *cookie);
int ndmp_process_messages(ndmp_connection_t *connection, bool_t reply_expected);
int tcp_accept(int listen_sock, unsigned int *inaddr_p);
int tcp_get_peer(int sock, unsigned int *inaddr_p, int *port_p);
int ndmp_get_fd(ndmp_connection_t *connection_handle);
void ndmp_set_client_data(ndmp_connection_t *connection_handle, void *client_data);
void *ndmp_get_client_data(ndmp_connection_t *connection_handle);
void ndmp_set_version(ndmp_connection_t *connection_handle, u_short version);
u_short ndmp_get_version(ndmp_connection_t *connection_handle);
void ndmp_set_authorized(ndmp_connection_t *connection_handle, bool_t authorized);
bool_t ndmp_check_auth_required(ndmp_message message);
ndmp_handler_t *ndmp_get_interface(ndmp_message message);
ndmp_msg_handler_t *ndmp_get_handler(ndmp_connection_t *connection, ndmp_message message);
void ndmp_close(ndmp_connection_t *connection_handle);
void printXDR(ndmp_connection_t *chanlde, ndmp_notify_connected_request *request_data);

#endif /* _NDMPD_FUNC_H_ */
