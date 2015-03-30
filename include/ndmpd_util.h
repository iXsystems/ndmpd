
#ifndef _NDMPD_UTIL_H_
#define	_NDMPD_UTIL_H_

#include <errno.h>

#include <ndmpd.h>
#include <ndmpd_session.h>
#include <ndmpd_callbacks.h>




#include <sys/queue.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <sys/mtio.h>


/*
 * The counter for creating unique names with "ndmp.%d" format.
 */
#define	NDMP_RCF_BASENAME	"ndmp."



#define	IN_ADDR(x) \
	(*(struct in_addr *)&x)

/*	defined in ndmpd_func	*/
extern int ndmp_full_restore_path;
extern int ndmp_ver;



int 		ndmpd_select(ndmpd_session_t *session, bool_t block, u_long class_mask);

int			ndmpd_add_file_handler(ndmpd_session_t *session, void *cookie, int fd,
							u_long mode, u_long class, ndmpd_file_handler_func_t *func);
void 		ndmp_set_socket_nodelay(int);
void 		ndmp_set_socket_snd_buf(int, int);
void 		ndmp_set_socket_rcv_buf(int, int);

ndmp_u_quad	long_long_to_quad(u_longlong_t ull);

void 		nlp_event_nw(ndmpd_session_t *session);
ndmp_lbr_params_t * ndmp_get_nlp(void *cookie);
int 		nlp_event_rv_get(ndmpd_session_t *session);
void 		nlp_event_rv_set(ndmpd_session_t *session,int rv);

void 		ndmp_send_reply(ndmp_connection_t *connection, void *reply, char *msg);

int 		ndmp_mtioctl(int fd, int cmd, int count);

int 		ndmpd_remove_file_handler(ndmpd_session_t *session, int fd);

char 		*getIPfromNIC(char *nicname);
int 		ndmp_create_socket(u_long *addr, u_short *port);


ndmp_error 	ndmpd_save_env(ndmpd_session_t *session, ndmp_pval *env, u_long envlen);
void 		ndmpd_free_env(ndmpd_session_t *session);
void 		ndmpd_free_tcp(ndmpd_session_t *session);

void 		ndmp_session_ref(ndmpd_session_t *session);
void		ndmp_session_unref(ndmpd_session_t *session);


char		*ndmp_new_job_name(char *jname);
int   		ndmp_get_cur_bk_time(ndmp_lbr_params_t *nlp, time_t *tp, char *jname);
long  		ndmp_buffer_get_size(ndmpd_session_t *session);
void		ndmpd_get_file_entry_type(int mode, ndmp_file_type *ftype);
char*		ndmp_get_relative_path(char *base, char *fullpath);


void		ndmp_stop_buffer_worker(ndmpd_session_t *session);
void		ndmp_waitfor_op(ndmpd_session_t *session);
void		ndmp_free_reader_writer_ipc(ndmpd_session_t *session);

void		ndmp_lbr_cleanup(ndmpd_session_t *session);

bool_t  	ndmp_valid_v3addr_type(ndmp_addr_type type);
int			ndmp_connect_sock_v3(u_long addr, u_short port);
char		**ndmpd_make_exc_list(void);




bool_t		fs_is_valid_logvol(char *path);
char 		*ndmpd_mk_temp(char *buf);
char 		*ndmpd_make_bk_dir_path(char *buf, char *fname);
void		ndmp_stop_writer_thread(ndmpd_session_t *session);

void		ndmp_stop_local_reader(ndmpd_session_t *session, tlm_commands_t *cmds);
void		ndmp_stop_remote_reader(ndmpd_session_t *session);

void		ndmp_sort_nlist_v3(ndmpd_session_t *session);
bool_t		ndmp_check_utf8magic(tlm_cmd_t *cmd);

u_longlong_t	quad_to_long_long(ndmp_u_quad q);
void		ndmp_stop_reader_thread(ndmpd_session_t *session);


void		ndmpd_free_nlist(ndmpd_session_t *session);
ndmp_error	ndmpd_save_nlist_v3(ndmpd_session_t *session, ndmp_name_v3 *nlist, u_long nlistlen);

void		ndmpd_free_nlist_v3(ndmpd_session_t *session);


void		ndmp_load_params(void);
void 		randomize(unsigned char *buffer, int size);

int 		ndmp_get_bk_dir_ino(ndmp_lbr_params_t *nlp);
void 		ndmp_copy_addr_v4(ndmp_addr_v4 *dst, ndmp_addr_v4 *src);
int			ndmp_lbr_init(ndmpd_session_t *session);
void		ndmp_copy_addr_v3(ndmp_addr_v3 *dst, ndmp_addr_v3 *src);

char 		*cctime(time_t *t);


char *ndmp_base64_encode(char *);
char *ndmp_base64_decode(char *);


#endif /* _NDMPD_FUNC_H_ */
