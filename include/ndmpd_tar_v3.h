#ifndef _NDMPD_TAR_V3_
#define _NDMPD_TAR_V3_

#include <ndmpd_util.h>
#include <ctype.h>
/*
// * IS_LBR_BKTYPE shows if the backup type is one of these
// * 'F' of 'f': 'Full' backup type.
// * 'A' of 'a': 'Archive' backup type.
// * 'I' of 'i': 'Incremental' backup type.
// * 'D' of 'd': 'Differntial' backup type.
// */
#define	IS_LBR_BKTYPE(t)	(((t) && strchr("FAID", toupper(t))) ? 1 : 0)


extern bool_t ndmp_ignore_ctime;
extern bool_t ndmp_include_lmtime;



/*	Defined	*/
ndmp_error	ndmp_restore_get_params_v3(ndmpd_session_t *session, ndmpd_module_params_t *params);
ndmp_error	ndmp_backup_get_params_v3(ndmpd_session_t *session, ndmpd_module_params_t *params);
int	ndmp_send_recovery_stat_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp, int idx, int stat);


void setWriteBufDone(tlm_buffers_t *bufs);
void setReadBufDone(tlm_buffers_t *bufs);


#endif /* _NDMPD_TAR_V3_ */
