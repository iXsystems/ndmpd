/*
 *
 */

#ifndef	_TLM_BUFFERS_H_
#define	_TLM_BUFFERS_H_

#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <pthread.h>
#include <ndmpd.h>
#include "tlm.h"


#ifndef RECORDSIZE
#define	RECORDSIZE	512
#endif /* !RECORDSIZE */

#define	DOTDOT_DIR	".."
#define	IS_DOTDOT(s)	(strcmp(s, DOTDOT_DIR) == 0)
#define	SLASH	'/'

#define	NDMP_MAX_SELECTIONS	64




typedef struct	tlm_buffer {
	char	*tb_buffer_data;	/* area to be used for I/O */
	long	tb_buffer_size;	/* number of valid bytes in the buffer */
	long	tb_buffer_spot;	/* current location in the I/O buffer */
	longlong_t tb_seek_spot;	/* for BACKUP */
				/* where in the file this buffer stops. */
				/* this is used for the Multi Volume */
				/* Header record. */
	longlong_t tb_file_size;	/* for BACKUP */
					/* how much of the file is left. */
	long	tb_full	: 1,
		tb_eot	: 1,
		tb_eof	: 1,
		tb_write_buf_filled,
		tb_read_buf_read;
	int	tb_errno;	/* I/O error values */
} tlm_buffer_t;


/*
 * Flags for tlm_buffers.
 */
#define	TLM_BUF_IN_READY	0x00000001
#define	TLM_BUF_OUT_READY	0x00000002

typedef struct	tlm_buffers {
	int	tbs_ref;	/* number of threads using this */
	short	tbs_buffer_in;	/* buffer to be filled */
	short	tbs_buffer_out;	/* buffer to be emptied */
				/* these are indexes into tlm_buffers */
	mutex_t	tbs_mtx;
	cond_t	tbs_in_cv;
	cond_t	tbs_out_cv;
	uint32_t	tbs_flags;
	long	tbs_data_transfer_size;	/* max size of read/write buffer */
	longlong_t tbs_offset;
	tlm_buffer_t tbs_buffer[TLM_TAPE_BUFFERS];
} tlm_buffers_t;

typedef struct	tlm_cmd {
	int	tc_ref;			/* number of threads using this */
	mutex_t	tc_mtx;
	cond_t	tc_cv;
	uint32_t	tc_flags;
	int	tc_reader;		/* writer to reader */
	int	tc_writer;		/* reader to writer */
	char	tc_file_name[TLM_MAX_PATH_NAME]; /* name of last file */
						/* for restore */
	tlm_buffers_t *tc_buffers; /* reader-writer speedup buffers */
} tlm_cmd_t;

typedef struct	tlm_commands {
	int	tcs_reader;	/* commands to all readers */
	int	tcs_writer;	/* commands to all writers */
	int	tcs_reader_count;	/* number of active readers */
	int	tcs_writer_count;	/* number of active writers */
	int	tcs_error;	/* worker errors */
	char	tcs_message[TLM_LINE_SIZE]; /* worker message back to user */
	tlm_cmd_t *tcs_command;	/* IPC area between read-write */
} tlm_commands_t;


typedef struct	tlm_job_stats {
	char	js_job_name[TLM_MAX_BACKUP_JOB_NAME];
	longlong_t js_bytes_total;	/* tape bytes in or out so far */
	longlong_t js_bytes_in_file;	/* remaining data in a file */
	longlong_t js_files_so_far;	/* files backed up so far */
	longlong_t js_files_total;	/* number of files to be backed up */
	int	js_errors;
	time_t	js_start_time;		/* start time (GMT time) */
	time_t	js_start_ltime;		/* start time (local time) */
	time_t	js_stop_time;		/* stop time (local time) */
	time_t	js_chkpnt_time;		/* checkpoint creation (GMT time) */
	void	*js_callbacks;
} tlm_job_stats_t;


struct full_dir_info {
	fs_fhandle_t fd_dir_fh;
	char fd_dir_name[TLM_MAX_PATH_NAME];
};

/*
 * For more info please refer to
 * "Functional Specification Document: Usgin new LBR engine in NDMP",
 * Revision: 0.2
 * Document No.: 101438.
 * the "File history of backup" section
 */
typedef struct lbr_fhlog_call_backs {
	void *fh_cookie;
	int (*fh_logpname)();
	int (*fh_log_dir)();
	int (*fh_log_node)();
} lbr_fhlog_call_backs_t;


typedef struct bk_selector {
	void *bs_cookie;
	int bs_level;
	int bs_ldate;
	bool_t (*bs_fn)(struct bk_selector *bks, struct stat *s);
} bk_selector_t;


/*
 * Call back structure to create new name for objects at restore time.
 */
struct rs_name_maker;
typedef char *(*rsm_fp_t)(const struct rs_name_maker *,
	char *buf,
	int pos,
	char *path);

struct rs_name_maker {
	rsm_fp_t rn_fp;
	void *rn_nlp;
};

/*
 *  RSFLG_OVR_*: overwriting policies.  Refer to LBR FSD for more info.
 *  RSFLG_MATCH_WCARD: should wildcards be supported in the selection list.
 *  RSFLG_IGNORE_CASE: should the compare be case-insensetive.  NDMP needs
 *  	case-sensetive name comparison.
 */
#define	RSFLG_OVR_ALWAYS	0x00000001
#define	RSFLG_OVR_NEVER		0x00000002
#define	RSFLG_OVR_UPDATE	0x00000004
#define	RSFLG_MATCH_WCARD	0x00000008
#define	RSFLG_IGNORE_CASE	0x00000010


/*
 * Different cases where two paths can match with each other.
 * Parent means that the current path, is parent of an entry in
 * the selection list.
 * Child means that the current path, is child of an entry in the
 * selection list.
 */
#define	PM_NONE		0
#define	PM_EXACT	1
#define	PM_PARENT	2
#define	PM_CHILD	3

extern tlm_job_stats_t *tlm_new_job_stats(char *);
extern tlm_job_stats_t *tlm_ref_job_stats(char *);
extern void tlm_un_ref_job_stats(char *);
extern bool_t tlm_is_excluded(char *, char *, char **);




tlm_buffers_t *tlm_allocate_buffers(bool_t, long);
tlm_buffer_t *tlm_buffer_advance_in_idx(tlm_buffers_t *);
tlm_buffer_t *tlm_buffer_advance_out_idx(tlm_buffers_t *);
tlm_buffer_t *tlm_buffer_in_buf(tlm_buffers_t *, int *);
tlm_buffer_t *tlm_buffer_out_buf(tlm_buffers_t *, int *);
void tlm_buffer_mark_empty(tlm_buffer_t *);
void tlm_buffer_release_in_buf(tlm_buffers_t *);
void tlm_buffer_release_out_buf(tlm_buffers_t *);
void tlm_buffer_in_buf_wait(tlm_buffers_t *);
void tlm_buffer_out_buf_wait(tlm_buffers_t *);
void tlm_buffer_in_buf_timed_wait(tlm_buffers_t *, unsigned);
void tlm_buffer_out_buf_timed_wait(tlm_buffers_t *, unsigned);
char *tlm_get_write_buffer(long, long *, tlm_buffers_t *, int);
char *tlm_get_read_buffer(int, int *, tlm_buffers_t *, int *);

void tlm_release_buffers(tlm_buffers_t *);
tlm_cmd_t *tlm_create_reader_writer_ipc(bool_t, long);
void tlm_release_reader_writer_ipc(tlm_cmd_t *);

void tlm_cmd_wait(tlm_cmd_t *cmd, uint32_t event_type);
void	tlm_cmd_signal(tlm_cmd_t *cmd, uint32_t event_type);
int	tlm_tarhdr_size(void);

typedef int (*path_hist_func_t)(lbr_fhlog_call_backs_t *,
    char *,
    struct stat *,
    u_longlong_t);

typedef int (*dir_hist_func_t)(lbr_fhlog_call_backs_t *,
    char *,
    struct stat *);

typedef int (*node_hist_func_t)(lbr_fhlog_call_backs_t *,
    char *,
    char *,
    struct stat *,
    u_longlong_t);

lbr_fhlog_call_backs_t *lbrlog_callbacks_init(void *,
    path_hist_func_t,
    dir_hist_func_t,
    node_hist_func_t);

typedef struct {
	tlm_commands_t *ba_commands;
	tlm_cmd_t *ba_cmd;
	char *ba_job;
	char *ba_dir;
	char *ba_sels[NDMP_MAX_SELECTIONS];
	pthread_barrier_t ba_barrier;
} tlm_backup_restore_arg_t;


extern void write_tar_eof(tlm_cmd_t *);

#endif	/* _TLM_BUFFERS_H_ */
