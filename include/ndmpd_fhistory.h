/*
 */

#ifndef _NDMPD_FHISTORY_H
#define	_NDMPD_FHISTORY_H




/*	defined in ndmpd_fhistory.c	*/
int	ndmpd_api_file_history_dir_v3(void *cookie, char *name, u_long node,
			u_long parent);
int	ndmpd_api_file_history_node_v3(void *cookie, u_long node,
			struct stat *file_stat, u_longlong_t fh_info);
int	ndmpd_api_file_history_file_v3(void *cookie, char *name,
			struct stat *file_stat, u_longlong_t fh_info);


void ndmpd_file_history_init(ndmpd_session_t *session);
void ndmpd_file_history_cleanup(ndmpd_session_t *session, bool_t send_flag);

int ndmpd_fhpath_v3_cb(lbr_fhlog_call_backs_t *cbp, char *path, struct stat *stp,u_longlong_t off);
int	ndmpd_fhdir_v3_cb(lbr_fhlog_call_backs_t *cbp, char *dir, struct stat *stp);
int	ndmpd_fhnode_v3_cb(lbr_fhlog_call_backs_t *cbp, char *dir, char *file,struct stat *stp, u_longlong_t off);
int ndmpd_path_restored_v3(lbr_fhlog_call_backs_t *cbp, char *name, struct stat *st, u_longlong_t ll_idx);










#endif /* _NDMPD_FHISTORY_H */
