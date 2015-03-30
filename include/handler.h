
#ifndef	_HANDLER_H_
#define	_HANDLER_H_




#include <ndmpd.h>
#include <ndmpd_session.h>



/******************** Connect	*****************/
void	ndmpd_connect_open_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_connect_client_auth_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_connect_server_auth_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_connect_close_v3(ndmp_connection_t *connection, void *body);



/******************** Data	*****************/

void		ndmpd_data_get_env_v3(ndmp_connection_t *connection, void *body);
void		ndmpd_data_get_state_v3(ndmp_connection_t *connection, void *body);
void		ndmpd_data_start_backup_v3(ndmp_connection_t *connection, void *body);
void		ndmpd_data_start_recover_v3(ndmp_connection_t *connection, void *body);
void		ndmpd_data_abort_v3(ndmp_connection_t *connection, void *body);
void		ndmpd_data_stop_v3(ndmp_connection_t *connection, void *body);
void		ndmpd_data_listen_v3(ndmp_connection_t *connection, void *body);
void		ndmpd_data_connect_v3(ndmp_connection_t *connection, void *body);
void		ndmpd_data_get_env_v4(ndmp_connection_t *connection, void *body);
void		ndmpd_data_get_state_v4(ndmp_connection_t *connection, void *body);
void		ndmpd_data_connect_v4(ndmp_connection_t *connection, void *body);
void		ndmpd_data_listen_v4(ndmp_connection_t *connection, void *body);
void		ndmpd_data_start_recover_filehist_v4(ndmp_connection_t *connection, void *body);
int			ndmpd_data_error_send(ndmpd_session_t *session, ndmp_data_halt_reason reason);
int			ndmpd_data_error_send_v4(ndmpd_session_t *session, ndmp_data_halt_reason reason);
void		ndmpd_data_error(ndmpd_session_t *session, ndmp_data_halt_reason reason);
void		data_accept_connection_v3(void *cookie, int fd, u_long mode);
int			create_listen_socket_v3(ndmpd_session_t *session, u_long *addr, u_short *port);
ndmp_error	data_connect_sock_v3(ndmpd_session_t *session, u_long addr, u_short port);
ndmp_error	start_backup_v3(ndmpd_session_t *session, char *bu_type, ndmp_pval *env_val, u_long env_len);
ndmp_error	start_recover_v3(ndmpd_session_t *session, char *bu_type, ndmp_pval *env_val, u_long env_len, ndmp_name_v3 *nlist_val, u_long nlist_len);
void		nlp_release_job_stat(ndmpd_session_t *session);
int			ndmpd_data_init(ndmpd_session_t *session);
void		ndmpd_data_cleanup(ndmpd_session_t *session);
static u_longlong_t	ndmpd_data_get_info(ndmpd_session_t *session);







/******************** info	*****************/

/*
 * Number of environment variable for the file system
 * info in V3 net_fs_info.
 */
#define	V3_N_FS_ENVS	4

/*
 * Is the file system a valid one to be reported to the
 * clients?
 */
#define	MNTTYPE_ZFS		"zfs"		/* ZFS file system */
#define	MNTTYPE_UFS		"ufs"		/* Unix file system */
#define	MNTTYPE_SMBFS	"smbfs"		/* SMBFS file system */
#define	MNTTYPE_NFS		"nfs"		/* NFS file system */
#define	MNTTYPE_NFS3	"nfs3"		/* NFS Version 3 file system */
#define	MNTTYPE_NFS4	"nfs4"		/* NFS Version 4 file system */
// this is for the support of TS
#define	MNTTYPE_EXT4	"ext4"		/* EXT4 file system */

#define	IS_VALID_FS(fs) (fs && ( \
	strcasecmp(fs, MNTTYPE_UFS) == 0 || \
	strcasecmp(fs, MNTTYPE_ZFS) == 0 || \
	strcasecmp(fs, MNTTYPE_NFS) == 0 || \
	strcasecmp(fs, MNTTYPE_NFS3) == 0 || \
	strcasecmp(fs, MNTTYPE_NFS4) == 0 || \
	strcasecmp(fs, MNTTYPE_EXT4) == 0))

#define	MNTTYPE_LEN	10

void	ndmpd_config_get_host_info_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_config_get_connection_type_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_config_get_auth_attr_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_config_get_butype_info_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_config_get_fs_info_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_config_get_tape_info_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_config_get_scsi_info_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_config_get_server_info_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_config_get_butype_info_v4(ndmp_connection_t *connection, void *body);
void	ndmpd_config_get_ext_list_v4(ndmp_connection_t *connection, void *body);
void	ndmpd_config_set_ext_list_v4(ndmp_connection_t *connection, void *body);



/******************** Mover	*****************/

/*
 * Maximum mover record size
 */
#define	MAX_MOVER_RECSIZE	(512*KB)

#define	TAPE_READ_ERR		-1
#define	TAPE_NO_WRITER_ERR	-2

#define	NDMP_APILOG(s, t, m, ...) \
{ \
	if (((ndmpd_session_t *)(s))->ns_protocol_version == NDMPV4) \
		(void) ndmpd_api_log_v4(s, t, m, __VA_ARGS__); \
	else if (((ndmpd_session_t *)(s))->ns_protocol_version == NDMPV3) \
		(void) ndmpd_api_log_v3(s, t, m, __VA_ARGS__); \
	else \
		(void) ndmpd_api_log_v3(s, __VA_ARGS__); \
}

void	ndmpd_mover_get_state_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_mover_listen_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_mover_continue_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_mover_abort_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_mover_set_window_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_mover_read_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_mover_set_record_size_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_mover_connect_v3(ndmp_connection_t *connection, void *body);
void	ndmpd_mover_get_state_v4(ndmp_connection_t *connection, void *body);
void	ndmpd_mover_listen_v4(ndmp_connection_t *connection, void *body);
void	ndmpd_mover_connect_v4(ndmp_connection_t *connection, void *body);
void	ndmpd_write_eom(int fd);
int		ndmpd_local_write(ndmpd_session_t *session, char *data, u_long length);
int		ndmpd_remote_write(ndmpd_session_t *session, char *data, u_long length);
int		ndmpd_local_read(ndmpd_session_t *session, char *data, u_long length);

int		ndmpd_mover_init(ndmpd_session_t *session);
void	ndmpd_mover_shut_down(ndmpd_session_t *session);
void	ndmpd_mover_cleanup(ndmpd_session_t *session);
ndmp_error	ndmpd_mover_connect(ndmpd_session_t *session, ndmp_mover_mode mover_mode);
int		ndmpd_mover_seek(ndmpd_session_t *session, u_longlong_t offset, u_longlong_t length);
int		mover_tape_reader(ndmpd_session_t *session);
int		mover_socket_writer(ndmpd_session_t *session);
int		mover_socket_reader(ndmpd_session_t *session);
int		mover_tape_writer(ndmpd_session_t *session);
int		ndmpd_mover_wait_v3(ndmpd_session_t *session);
int		ndmpd_mover_error_send(ndmpd_session_t *session, ndmp_mover_halt_reason reason);
int		ndmpd_mover_error_send_v4(ndmpd_session_t *session, ndmp_mover_halt_reason reason);
void	ndmpd_mover_error(ndmpd_session_t *session, ndmp_mover_halt_reason reason);
int		ndmpd_local_write_v3(ndmpd_session_t *session, char *data, u_long length);
int		ndmpd_local_read_v3(ndmpd_session_t *session, char *data, u_long length);;


int		ndmpd_remote_read_v3(ndmpd_session_t *session, char *data, u_long length);






#endif	/* !_HANDLER_H_ */
