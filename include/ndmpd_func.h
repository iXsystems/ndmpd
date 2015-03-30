#ifndef _NDMPD_FUNC_H_
#define	_NDMPD_FUNC_H_

#include <ndmpd.h>
#include <ndmpd_session.h>

/*	gethostbyname	*/
#include <netdb.h>

/*	inet_ntoa	*/
#include <arpa/inet.h>


#define	VOL_MAXNAMELEN	256

extern ndmp_handler_t ndmp_msghdl_tab[];

int 				ndmp_send_response(ndmp_connection_t *connection_handle, ndmp_error err, void *reply);
int 				ndmp_send_request(ndmp_connection_t *connection_handle, ndmp_message message,ndmp_error err, void *request_data, void **reply);
int 				ndmp_send_request_lock(ndmp_connection_t *connection_handle, ndmp_message message, ndmp_error err, void *request_data, void **reply);
int 				ndmp_recv_msg(ndmp_connection_t *connection);
int					ndmp_process_messages(ndmp_connection_t *connection, bool_t reply_expected);
void*				ndmp_malloc(size_t size);
int					ndmp_writeit(void *connection_handle, void* buf, int len);
int					ndmp_readit(void *connection_handle, void* buf, int len);
void				ndmp_free_message(ndmp_connection_t *connection_handle);
ndmp_connection_t*	ndmp_create_xdr_connection(void);
void				ndmp_destroy_xdr_connection(ndmp_connection_t *connection_handle);
int					ndmp_process_requests(ndmp_connection_t *connection_handle);
void				connection_file_handler(void *cookie, int fd, u_long mode);
int					ndmp_process_messages(ndmp_connection_t *connection, bool_t reply_expected);
int					tcp_accept(int listen_sock, unsigned int *inaddr_p);
int					tcp_get_peer(int sock, unsigned int *inaddr_p, int *port_p);
int					ndmp_get_fd(ndmp_connection_t *connection_handle);
void				ndmp_set_client_data(ndmp_connection_t *connection_handle, void *client_data);
void*				ndmp_get_client_data(ndmp_connection_t *connection_handle);
void				ndmp_set_version(ndmp_connection_t *connection_handle, u_short version);
u_short				ndmp_get_version(ndmp_connection_t *connection_handle);
void				ndmp_set_authorized(ndmp_connection_t *connection_handle, bool_t authorized);
bool_t				ndmp_check_auth_required(ndmp_message message);
ndmp_handler_t*		ndmp_get_interface(ndmp_message message);
ndmp_msg_handler_t*	ndmp_get_handler(ndmp_connection_t *connection, ndmp_message message);
void				ndmp_close(ndmp_connection_t *connection_handle);

// posix function.
#ifdef QNAP_TS
size_t	strlcpy(char * __restrict dst, const char * __restrict src, size_t siz);
size_t	strlcat(char * __restrict dst, const char * __restrict src, size_t siz);
#endif

#endif /* _NDMPD_FUNC_H_ */


