#ifndef _NDMPD_SNAPSHOT_H_
#define	_NDMPD_SNAPSHOT_H_

/*
 * SNAPSHOT
 * */

#include <ndmpd.h>
#include <ndmpd_func.h>
#include <mnt_variable.h>


#define	SNAPSHOT_PREFIX	".zfs"
#define	SNAPSHOT_DIR	".zfs/snapshot"

#define	VOLNAME_MAX_LENGTH	255



typedef struct chkpnt_param {
	char *chp_name;
	bool_t chp_found;
} chkpnt_param_t;

unsigned int	ndmp_add_chk_pnt_vol(char *vol_name);

//int	ndmpd_take_snapshot(char *vol_name, char *jname, char *snapshot_path);

//#ifdef QNAP_TS
//	int	ndmpd_delete_snapshot(int snapshot_id);
//#else
//	int	ndmpd_delete_snapshot(char *vol_name, char *jname);
//#endif
bool_t fs_is_rdonly(char *);
bool_t fs_is_chkpntvol();
int chkpnt_backup_successful();
int chkpnt_backup_prepare();

#ifdef QNAP_TS

#else
	int get_zfsvolname(char *, int, char *);
#endif

int chkpnt_creationtime_bypattern();

char 		*ndmpd_build_snapshot_name(char *name, char *sname, char *jname);

#endif /* _NDMPD_SNAPSHOT_H_ */


