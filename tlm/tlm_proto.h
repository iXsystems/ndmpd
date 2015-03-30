
#ifndef	_TLM_PROTO_H
#define	_TLM_PROTO_H

#include <tlm.h>
#include <tlm_buffers.h>

#include <dirent.h>
#include <pthread.h>


#define	MAXIORETRY	20

typedef void log_func_t(u_long, char *, ...);

extern longlong_t llmin(longlong_t, longlong_t);
extern unsigned int min(unsigned int, unsigned int);
extern unsigned int max(unsigned int, unsigned int);
extern int oct_atoi(char *p);

extern int tlm_log_fhnode(tlm_job_stats_t *,
    char *,
    char *,
    struct stat *,
    u_longlong_t);

extern int tlm_log_fhdir(tlm_job_stats_t *,
    char *,
    struct stat *,
    struct fs_fhandle *);

extern int tlm_log_fhpath_name(tlm_job_stats_t *,
    char *,
    struct stat *,
    u_longlong_t);


extern void tlm_log_list(char *,
    char **);

extern int tlm_ioctl(int, int, void *);

extern int tlm_library_count(void);

extern bool_t fs_is_rdonly(char *);
extern bool_t fs_is_chkpntvol();
extern int chkpnt_backup_successful();
extern int chkpnt_backup_prepare();
extern int chkpnt_creationtime_bypattern();

extern log_func_t log_debug;
extern log_func_t log_error;
extern bool_t match(char *, char *);

extern void tlm_build_header_checksum(tlm_tar_hdr_t *);
extern int tlm_vfy_tar_checksum(tlm_tar_hdr_t *);
extern int tlm_entry_restored(tlm_job_stats_t *, char *, int);
extern char *strupr(char *);
extern char *parse(char **, char *);
extern int sysattr_rdonly(char *);
extern int sysattr_rw(char *);

extern int tar_putfile(char *,
    char *,
    char *,
    tlm_acls_t *,
    tlm_commands_t *,
    tlm_cmd_t *,
    tlm_job_stats_t *,
    struct hardlink_q *);

extern int tar_putdir(char *,
    tlm_acls_t *,
    tlm_cmd_t *,
    tlm_job_stats_t *);

extern int tar_getfile(tlm_backup_restore_arg_t *);

extern int
tar_getdir(tlm_commands_t *,
    tlm_cmd_t *,
    tlm_job_stats_t *,
    const struct rs_name_maker *,
    int,
    int,
    char **,
    char **,
    int,
    int,
    struct hardlink_q *);
#endif	/* _TLM_PROTO_H */
