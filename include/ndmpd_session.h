/*
 */

#ifndef _NDMPD_SESSION_H
#define	_NDMPD_SESSION_H




/*	ndmp xdr define	*/
#include <ndmp.h>
#include <ndmpd.h>
#include <ndmpd_callbacks.h>


#include <pthread.h>
#include <tlm_buffers.h>


#define	NLP_READY	1


/*
 * Calculate array length based on its size and size of
 * its elements.
 */
#define	ARRAY_LEN(a, t)	(sizeof (a) / sizeof (t))


/*
 * Root inode number of dump format in V2.
 */
#define	ROOT_INODE	2



typedef struct {
	char *bk_path;
	char bk_snap_path[TLM_MAX_PATH_NAME];
	int bk_llevel; /* last backup level */
	time_t bk_ldate; /* last backup date */
	int bk_clevel;	/* current backup level */
	time_t bk_cdate; /* current backup date */
	int bk_map;
	int bk_dirino;
	char *bk_dmpnm;
	char **bk_exl; /* exlude list */
	char **bk_inc; /* include list */
} ndmp_backup_params_t;


typedef struct {
	u_long rs_nf;	/* number of files to restore */
	char *rs_path;
	char *rs_bkpath;
	int *rs_restored;
	int rs_bm;
	int rs_lastidx;
} ndmp_restore_params_t;







typedef struct mem_ndmp_name_v3 {
	char *nm3_opath;
	char *nm3_dpath;
	char *nm3_newnm;
	u_longlong_t nm3_node;
	u_longlong_t nm3_fh_info;
	ndmp_error nm3_err;
} mem_ndmp_name_v3_t;


typedef struct ndmpd_file_handler {
	int fh_fd;
	u_long fh_mode;
	u_long fh_class;
	void *fh_cookie;
	ndmpd_file_handler_func_t *fh_func;
	struct ndmpd_file_handler *fh_next;
} ndmpd_file_handler_t;

/*	we don't support scsi and tape	*/
// typedef struct ndmpd_session_scsi_desc {
	// int sd_is_open;
	// int sd_devid;
	// bool_t sd_valid_target_set;
	// int sd_sid;
	// int sd_lun;
	// char sd_adapter_name[SCSI_MAX_NAME];
// } ndmpd_session_scsi_desc_t;

// typedef struct ndmpd_session_tape_desc {
	// int td_fd;			/* tape device file descriptor */
	// u_long td_record_count;	/* number of records written */
	// ndmp_tape_open_mode td_mode;	/* tape device open mode */
	// u_longlong_t td_pos;	/* current position on the current tape */
	// int td_sid;
	// int td_lun;
	// char td_adapter_name[SCSI_MAX_NAME];
	// u_long td_eom_seen:1,
		// td_io_err:1,
		// td_write:1;
// } ndmpd_session_tape_desc_t;

 typedef struct ndmpd_session_mover_desc {
	 ndmp_mover_state md_state;	/* current state */
	 ndmp_mover_mode md_mode;	/* current mode */
	 ndmp_mover_pause_reason md_pause_reason;	/* current reason */
	 ndmp_mover_halt_reason md_halt_reason;	/* current reason */
	 u_longlong_t md_data_written;	/* total written to tape */
	 u_longlong_t md_seek_position;	/* current seek position */
	 u_longlong_t md_bytes_left_to_read; /* #bytes to end of seek window */
	 u_longlong_t md_window_offset;	/* valid data window begin */
	 u_longlong_t md_window_length;	/* valid data window length */
	 u_longlong_t md_position;	/* current data stream pos */
	 bool_t md_pre_cond;		/* used for precondition checks */
	 u_long md_record_size;	/* tape I/O record size */
	 u_long md_record_num;	/* current record num */
	 int md_listen_sock;		/* data conn listen socket */
	 int md_sock;		/* data conn socket */
	 u_long md_r_index;		/* buffer read  index */
	 u_long md_w_index;		/* buffer write index */
	 char *md_buf;		/* data buffer */
	 /*
	  * V2 fields.
	  */
	 u_long md_discard_length;	/* bytes to discard */

	 /*
	  * V3 fields.
	  */
	 ndmp_addr_v3 md_data_addr;
	 /*
	  * V4 fields.
	  */
	 ndmp_addr_v4 md_data_addr_v4;
 } ndmpd_session_mover_desc_t;


 typedef struct ndmpd_module_stats {
   	u_longlong_t ms_bytes_processed;
   	u_longlong_t ms_est_bytes_remaining;
   	u_long ms_est_time_remaining;
 } ndmpd_module_stats;





 /*
  * Parameter structure passed to module start function.
  */
  typedef struct ndmpd_module_params {
 	 void *mp_daemon_cookie;
 	 void **mp_module_cookie;
 	 ushort mp_protocol_version;
 	 ndmp_data_operation mp_operation;
 	 ndmpd_module_stats *mp_stats;
 	 ndmpd_get_env_func_t *mp_get_env_func;
 	 ndmpd_add_env_func_t *mp_add_env_func;
 	 ndmpd_add_env_func_t *mp_set_env_func;
 	 ndmpd_get_name_func_t *mp_get_name_func;
 	 ndmpd_dispatch_func_t *mp_dispatch_func;
 	 ndmpd_done_func_t *mp_done_func;
 //	 ndmpd_log_func_t *mp_log_func;
 	 ndmpd_add_file_handler_func_t *mp_add_file_handler_func;
 	 ndmpd_remove_file_handler_func_t *mp_remove_file_handler_func;
 	 ndmpd_write_func_t *mp_write_func;
 	 ndmpd_file_history_path_func_t *mp_file_history_path_func;
 	 ndmpd_file_history_dir_func_t *mp_file_history_dir_func;
 	 ndmpd_file_history_node_func_t *mp_file_history_node_func;
 	 ndmpd_read_func_t *mp_read_func;
 	 ndmpd_seek_func_t *mp_seek_func;
 	 ndmpd_file_recovered_func_t *mp_file_recovered_func;
 	 /*
 	  * NDMP V3 params.
 	  */
 	 ndmpd_log_func_v3_t *mp_log_func_v3;
  } ndmpd_module_params_t;

  /*
   * Module function prototypes.
   */
typedef int module_start_func_t(ndmpd_module_params_t *);
typedef int module_abort_func_t(void *);

typedef struct ndmpd_session_data_module {
	void *dm_module_cookie;	/* sent as abort_func param */
	module_start_func_t *dm_start_func;	/* start function */
	module_abort_func_t *dm_abort_func;	/* abort function */
	ndmpd_module_stats dm_stats;	/* statistics buffer */
} ndmpd_session_data_module_t;

typedef struct ndmpd_session_data_desc {
	/*
	 * Common fields.
	 */
	ndmp_data_operation dd_operation;	/* current operation */
	bool_t dd_abort;		/* abort operation flag */
	bool_t dd_io_ready;		/* mover sock read for I/O */
	ndmp_pval *dd_env;	/* environment from backup or recover request */
	u_long dd_env_len;		/* environment length */
	u_long dd_nlist_len;	/* recover file list length */
	int dd_sock;		/* listen and data socket */
	u_longlong_t dd_read_offset;	/* data read seek offset */
	u_longlong_t dd_read_length;	/* data read length */
	u_longlong_t dd_data_size;	/* data size to be backed up */

	/*	this module will defined the callback functions.	*/
	ndmpd_session_data_module_t dd_module;

	ndmp_data_state dd_state;	/* current state */
	ndmp_data_halt_reason dd_halt_reason;		/* current reason */
	/*
	 * V2 fields.
	 */
	ndmp_name *dd_nlist;	/* recover file list */
	ndmp_mover_addr dd_mover;	/* mover address */
	/*
	 * V3 fields.
	 */
	mem_ndmp_name_v3_t *dd_nlist_v3;
	ndmp_addr_v3 dd_data_addr;
	int dd_listen_sock;	/* socket for listening for remote */
				/* mover connections */
	u_longlong_t dd_bytes_left_to_read;
	u_longlong_t dd_position;
	u_longlong_t dd_discard_length;
	/*
	 * V4 fields.
	 */
	ndmp_addr_v4 dd_data_addr_v4;
} ndmpd_session_data_desc_t;

 typedef struct ndmpd_session_file_history {
	 ndmp_fh_unix_path *fh_path_entries;
	 ndmp_fh_unix_dir *fh_dir_entries;
	 ndmp_fh_unix_node *fh_node_entries;
	 char *fh_path_name_buf;
	 char *fh_dir_name_buf;
	 u_long fh_path_index;
	 u_long fh_dir_index;
	 u_long fh_node_index;
	 u_long fh_path_name_buf_index;
	 u_long fh_dir_name_buf_index;
 } ndmpd_session_file_history_t;

 typedef struct ndmpd_session_file_history_v3 {
	 ndmp_file_v3 *fh_files;
	 ndmp_dir_v3 *fh_dirs;
	 ndmp_node_v3 *fh_nodes;
	 ndmp_file_name_v3 *fh_file_names;
	 ndmp_file_name_v3 *fh_dir_names;
	 ndmp_file_stat_v3 *fh_file_stats;
	 ndmp_file_stat_v3 *fh_node_stats;
	 char *fh_file_name_buf;
	 char *fh_dir_name_buf;
	 u_long fh_file_index;
	 u_long fh_dir_index;
	 u_long fh_node_index;
	 u_long fh_file_name_buf_index;
	 u_long fh_dir_name_buf_index;
 } ndmpd_session_file_history_v3_t;



/*	back recovery  */
 typedef struct ndmp_lbr_params {
	 struct ndmpd_session *nlp_session;
	 int nlp_flags;

	 ndmp_backup_params_t bk_params;
	 ndmp_restore_params_t rs_params;
 #define	nlp_backup_path			bk_params.bk_path
 #define	nlp_snap_backup_path	bk_params.bk_snap_path
 #define	nlp_llevel	bk_params.bk_llevel
 #define	nlp_ldate	bk_params.bk_ldate
 #define	nlp_clevel	bk_params.bk_clevel
 #define	nlp_cdate	bk_params.bk_cdate
 #define	nlp_bkdirino	bk_params.bk_dirino
 #define	nlp_dmpnm	bk_params.bk_dmpnm
 #define	nlp_exl		bk_params.bk_exl
 #define	nlp_inc		bk_params.bk_inc

 #define	nlp_nfiles	rs_params.rs_nf
 #define	nlp_restore_path	rs_params.rs_path
 #define	nlp_restore_bk_path	rs_params.rs_bkpath
 #define	nlp_restored	rs_params.rs_restored
 #define	nlp_lastidx	rs_params.rs_lastidx

	 ndmpd_module_params_t *nlp_params;
	 tlm_job_stats_t *nlp_jstat;
	 lbr_fhlog_call_backs_t *nlp_logcallbacks;
	 tlm_commands_t nlp_cmds;
	 struct {
		 /*
		  * nw: shows the number of threads waiting for a request
		  * to be processed.
		  * rv: if error occurred when processing a request.
		  */
		 int ev_nw;
		 int ev_rv;
	 } nlp_event;
	 cond_t	nlp_cv;
	 int	nlp_flag;
 #define	nlp_nw	nlp_event.ev_nw
 #define	nlp_rv	nlp_event.ev_rv
	 u_longlong_t nlp_bytes_total;
 } ndmp_lbr_params_t;





typedef struct ndmpd_session {
	ndmp_connection_t *ns_connection;	/* NDMP connection to client */
	bool_t ns_eof;		/* connection EOF flag */
	short ns_protocol_version;	/* connection protocol version */

	//ndmpd_session_scsi_desc_t ns_scsi;
	//ndmpd_session_tape_desc_t ns_tape;

	ndmpd_session_mover_desc_t ns_mover;
	ndmpd_session_data_desc_t ns_data;

	ndmpd_session_file_history_t ns_fh;

	ndmpd_file_handler_t *ns_file_handler_list; /* for I/O multiplexing */
	int ns_nref;
	ndmp_lbr_params_t *ns_ndmp_lbr_params;
	mutex_t ns_lock;

	/*
	 * NDMP V3
	 * Tape, SCSI, mover, data and file handlers will
	 * be shared between V2 and V3.
	 */
	ndmpd_session_file_history_v3_t ns_fh_v3;
	unsigned char ns_challenge[MD5_CHALLENGE_SIZE];  /* For MD5 */

	/*
	 * NDMP V4 related data
	 */
	bool_t ns_set_ext_list;

	/* handling of hardlink, hardlink queue head */
	struct hardlink_q *hardlink_q;
} ndmpd_session_t;


/*
 * pthread call arg parameters
 */
typedef struct {
	int nw_sock;
	long nw_ipaddr;
	ndmp_con_handler_func_t nw_con_handler_func;
} ndmpd_worker_arg_t;

typedef struct {
	char *br_jname;
	ndmp_lbr_params_t *br_nlp;
	tlm_commands_t *br_cmds;
	pthread_barrier_t br_barrier;
} backup_reader_arg_t;

typedef struct {
	ndmpd_session_t *tr_session;
	ndmpd_module_params_t *tr_mod_params;
	tlm_commands_t *tr_cmds;
	pthread_barrier_t br_barrier;
} ndmp_tar_reader_arg_t;







#define	MOD_SETENV(m, n, v) \
	(*(m)->mp_set_env_func)((m)->mp_daemon_cookie, n, v)

#define	MOD_GETENV(m, e) \
	(*(m)->mp_get_env_func)((m)->mp_daemon_cookie, e)

#define	MOD_GETNAME(m, i) \
	(*(m)->mp_get_name_func)((m)->mp_daemon_cookie, i)

#define	MOD_LOG(m, ...)	\
	(*(m)->mp_log_func)((m)->mp_daemon_cookie, __VA_ARGS__)

#define	MOD_READ(m, b, s) \
	(*(m)->mp_read_func)((m)->mp_daemon_cookie, b, s)

#define	MOD_WRITE(m, b, s) \
	(*(m)->mp_write_func)((m)->mp_daemon_cookie, b, s)

#define	MOD_DONE(m, e) \
	(*(m)->mp_done_func)((m)->mp_daemon_cookie, e)

#define	MOD_FILERECOVERD(m, n, e) \
	(*(m)->mp_file_recovered_func)((m)->mp_daemon_cookie, n, e)

extern int ndmp_log_msg_id;

#define	MOD_LOGV3(m, t, ...) \
	(*(m)->mp_log_func_v3)((m)->mp_daemon_cookie, (t), \
	++ndmp_log_msg_id, __VA_ARGS__)

#define	MOD_LOGCONTV3(m, t, ...) \
	(*(m)->mp_log_func_v3)((m)->mp_daemon_cookie, \
	(t), ndmp_log_msg_id, __VA_ARGS__)


/*
 * Module function prototypes.
 */
module_start_func_t ndmpd_tar_backup_starter_v3;
module_abort_func_t ndmpd_tar_backup_abort_v3;

module_start_func_t ndmpd_tar_restore_starter_v3;
module_abort_func_t ndmpd_tar_restore_abort_v3;


#endif /* _NDMPD_H */
