

#include <ndmpd_snapshot.h>

#include <tlm.h>


#ifdef QNAP_TS
	#include <mntent.h>
	#include <sys/vfs.h>
	#include <dirent.h>
	#include <snapshot_ts.h>

#else
	/*	getmntinfo	*/
	#include <sys/mount.h>
#endif


#ifdef QNAP_TS
	static int	get_lvm_volname(char *volname, int len, char *path);
	static int do_ts_snapshot(char *vol_path, char* snapshot_name, char *snapshot_path);
#else
	static int	get_zfs_volname(char *volname, int len, char *path);
#endif

static int	ndmpd_create_snapshot(char *volname, char *jname,char *snapshot_path);

/*
 *
 * 	SNAPSHOT FUNCTIONS
 *
 *
 * */


ndmp_chkpnt_vol_t *chkpnt_vols = NULL;







#include<assert.h>
/*
 * ndmp_start_check_point
 *
 * This function will parse the path, vol_name, to get the real volume name.
 * for the volume is necessary. If it is, a checkpoint is created.
 * This function should be called before the NDMP backup is started.
 *
 * Parameters:
 *   vol_name (input) - name of the volume
 *
 * Returns:
 *   >=0 : on success, returns snapshot_id. For ES, the snapshot ID always equal to zero.
 *   										For TS, the snapshot ID should larger than zero.
  *  -1 : otherwise
 */
int
ndmpd_take_snapshot(char *vol_name, char *jname,char *snapshot_path)
{
	int erc = 0;
	char vol[VOL_MAXNAMELEN];

	ndmpd_log(LOG_DEBUG, "ndmpd_take_snapshot vol=%s jname=%s",vol_name,jname);

#ifdef QNAP_TS
	if (vol_name == 0 || get_lvm_volname(vol, sizeof (vol), vol_name) == -1)
			return (-1);
#else
	if (vol_name == 0 || get_zfs_volname(vol, sizeof (vol), vol_name) == -1)
			return (-1);
#endif

	ndmpd_log(LOG_DEBUG, "ndmpd_take_snapshot vol found=%s",vol);

	erc = ndmpd_create_snapshot(vol, jname, snapshot_path);

#ifdef QNAP_TS
	if(erc>0){
		// in TS, the snapshot will returne a volume name. There will be more than one share folder
		// share the same volume. We have to append the share folder to make the right path.
		strncpy(snapshot_path+strlen(snapshot_path),vol_name+strlen("/share"),strlen(vol_name)-strlen("/share"));
		ndmpd_log(LOG_DEBUG, "ndmpd_take_snapshot snapshot path found=%s",snapshot_path);
	}
#else
	if(erc==0){
		ndmpd_log(LOG_DEBUG, "ndmpd_take_snapshot snapshot path found=%s",snapshot_path);
	}
#endif

	ndmpd_log(LOG_DEBUG, "erc=%d",erc);


	return (erc);
}


#ifdef QNAP_TS
/*
 * ndmp_delete_snapshot
 *
 * This function will parse the path, vol_name, to get the real volume name.
 * Returns:
 *   0: on success
 *   -1: otherwise
 */
int
ndmpd_delete_snapshot(int snapshot_id)
{
	ndmpd_log(LOG_DEBUG, "deleting snapshot for TS.");
	return NAS_Snapshot_Delete(NULL, snapshot_id);
}
#else
/*
 * ndmp_delete_snapshot
 *
 * This function will parse the path, vol_name, to get the real volume name.
 * It will then check via ndmp_remove_chk_pnt_vol to see if removing a check
 * point for the volume is necessary. If it is, a checkpoint is removed.
 * This function should be called after NDMP backup is finished.
 *
 * Parameters:
 *   vol_name (input) - name of the volume
 *
 * Returns:
 *   0: on success
 *   -1: otherwise
 */
int
ndmpd_delete_snapshot(char *vol_name, char *jname)
{
	int erc = 0;
//	char vol[VOL_MAXNAMELEN];
//
//	if (vol_name == 0 ||
//	    get_zfsvolname(vol, sizeof (vol), vol_name))
//		return (0);
//
//	if (ndmp_remove_chk_pnt_vol(vol) == 0)
//		erc = chkpnt_backup_successful(vol, jname);
	return (erc);
}
#endif

/*
 * Insert the backup snapshot name into the path.
 *
 * Input:
 * 	name: Origional path name.
 *
 * Output:
 * 	name: Origional name modified to include a snapshot.
 *
 * Returns:
 * 	Origional name modified to include a snapshot.
 */
char *
ndmpd_build_snapshot_name(char *name, char *sname, char *jname)
{
	ndmpd_log(LOG_DEBUG, "ndmpd_build_snapshot_name");

//	zfs_handle_t *zhp;
//
//	char *rest;
//	char volname[VOL_MAXNAMELEN];
//	char mountpoint[PATH_MAX];
//
//	if (get_zfsvolname(volname, VOL_MAXNAMELEN, name) == -1)
//		goto notzfs;
//
//	(void) mutex_lock(&zlib_mtx);
//	if ((zlibh == NULL) ||
//	    (zhp = zfs_open(zlibh, volname, ZFS_TYPE_DATASET)) == NULL) {
//		(void) mutex_unlock(&zlib_mtx);
//		goto notzfs;
//	}
//
//	if (zfs_prop_get(zhp, ZFS_PROP_MOUNTPOINT, mountpoint, PATH_MAX, NULL,
//	    NULL, 0, B_FALSE) != 0) {
//		zfs_close(zhp);
//		(void) mutex_unlock(&zlib_mtx);
//		goto notzfs;
//	}
//
//	zfs_close(zhp);
//	(void) mutex_unlock(&zlib_mtx);
//
//	rest = name + strlen(mountpoint);
//	(void) snprintf(sname, TLM_MAX_PATH_NAME, "%s/%s/bk-%s%s", mountpoint,
//	    TLM_SNAPSHOT_DIR, jname, rest);

	return (sname);

notzfs:
	(void) strlcpy(sname, name, TLM_MAX_PATH_NAME);
	return (sname);
}

/*
 * Remove the checkpoint from a path name.
 *
 * Input:
 * 	name: Full pathname with checkpoint embeded.
 *
 * Output:
 * 	unchkp_name: real pathname with no checkpoint.
 *
 * Returns:
 *	Pointer to the un-checkpointed path.
 */
char *
tlm_remove_checkpoint(char *name, char *unchkp_name)
{
	char *cp;
	int i;
	int plen;

	unchkp_name[0] = name[0];
	plen = strlen(SNAPSHOT_PREFIX);
	for (i = 1; i <= VOLNAME_MAX_LENGTH + 1; i++) {
		switch (name[i]) {
		case '.':
			if (strncmp(&name[i], SNAPSHOT_PREFIX,
			    plen) == 0) {
				unchkp_name[i] = '\0';
				i += plen;
				if (name[i] == '\0') {
					/*
					 * name == "/v1.chkpnt"
					 */
					return (unchkp_name);
				}
				if ((cp = strchr(&name[++i], '/')) != NULL) {
					(void) strlcat(unchkp_name, cp,
					    VOLNAME_MAX_LENGTH + 1);
				}
				return (unchkp_name);
			} else {
				unchkp_name[i] = name[i];
			}
			break;
		case '/':
			return (name);
		case 0:
			return (name);
		default:
			unchkp_name[i] = name[i];
			break;
		}
	}
	return (name);
}


#ifdef QNAP_TS

static
int do_ts_snapshot(char *vol_path, char* snapshot_name, char *snapshot_path)
{
    int ret=0;

    int vol_id, snapshot_id=0;
    char mount_path[TLM_MAX_PATH_NAME] = {0};

    Snapshot_Create_CONFIG ss_config;

    memset(&ss_config, 0, sizeof(Snapshot_Create_CONFIG));

    ndmpd_log(LOG_DEBUG, "get volume id on =%s\n",vol_path);
	if(Volume_Get_Vol_ID_By_Mount_Path(vol_path, &vol_id)==0){
		// take success
		ndmpd_log(LOG_DEBUG, "get volume ID success. ID=%d\n",vol_id);
	}else{
		// get volume id fail
		ndmpd_log(LOG_DEBUG, "get volume ID fail ID=%d\n",vol_id);
		return -1;
	}

	snprintf(ss_config.name, sizeof(ss_config.name), "%s", snapshot_name);

	ndmpd_log(LOG_DEBUG, "taking snapshot with vol id=%d and snapshot name=%s\n", vol_id,ss_config.name);

	snapshot_id = NAS_Snapshot_Create_For_App(vol_id, &ss_config);

	if(snapshot_id>0){
		ndmpd_log(LOG_DEBUG, "take snapshot success. snapshot id=%d\n", snapshot_id);
	}else{
		ndmpd_log(LOG_DEBUG, "take snapshot fail with id=%d\n",snapshot_id);
		return -2;
	}

	ndmpd_log(LOG_DEBUG, "mount snapshot with pid=%d\n",getpid());

	if(NAS_Snapshot_Mount(snapshot_id)==0 && NAS_Snapshot_Mount_Msg(snapshot_id, 0)){
		ndmpd_log(LOG_DEBUG, "mount snapshot success\n");
		Snapshot_Set_PID(snapshot_id,  getpid());
	}else{
		ndmpd_log(LOG_DEBUG, "mount snapshot fail\n");
		return -3;
	}

	if(NAS_Snapshot_Get_Preview_Path(snapshot_id, mount_path, sizeof(mount_path))==0){
			ndmpd_log(LOG_DEBUG, "get snapshot path success on path=%s\n",mount_path);
			strncpy(snapshot_path, mount_path, strlen(mount_path)+1);
			ndmpd_log(LOG_DEBUG, "snapshot_path=%s ",snapshot_path,snapshot_path);
			ret = snapshot_id;
	}else{
		ndmpd_log(LOG_DEBUG, "get snapshot path fail\n");
		return -4;
	}

    return ret;
}
#else
#endif


/*
 * Create a snapshot on the volume
 * Returns:
 *   0: on success
 *   -1: otherwise
 */
static int
ndmpd_create_snapshot(char *volname, char *jname,char *snapshot_path)
{
	ndmpd_log(LOG_DEBUG, "chkpnt_backup_prepare");

	char chk_name[PATH_MAX];
	char *p;
	int rv;

	if (!volname || !*volname)
		return (-1);
	/* Remove the leading slash */
	p = volname;
	while (*p == '/')
		p++;

	(void) snprintf(chk_name, PATH_MAX, "%s@bk-%s", p, jname);

#ifdef QNAP_TS
	return do_ts_snapshot(volname, chk_name, snapshot_path);
#else
//	(void) mutex_lock(&zlib_mtx);
//	if ((rv = zfs_snapshot(zlibh, chk_name, 0, NULL)) == -1) {
//		if (errno == EEXIST) {
//			(void) mutex_unlock(&zlib_mtx);
//			return (0);
//		}
//		ndmpd_log(LOG_DEBUG,
//		    "chkpnt_backup_prepare: %s failed (err=%d)",
//		    chk_name, errno);
//		(void) mutex_unlock(&zlib_mtx);
//		return (rv);
//	}
//	(void) mutex_unlock(&zlib_mtx);
	ndmpd_log(LOG_DEBUG, "ES does not support snapshot right now.(20140213)");
	return (-1);
#endif

	return (0);
}

/*
 * Remove the 'backup' snapshot if backup was successful
 */
int
chkpnt_backup_successful(char *volname, char *jname)
{
	ndmpd_log(LOG_DEBUG, "chkpnt_backup_successful");
	return 0;

}

/*
 * Get the snapshot creation time
 */
int
chkpnt_creationtime_bypattern(char *volname, char *pattern, time_t *tp)
{
	ndmpd_log(LOG_DEBUG, "chkpnt_creationtime_bypattern");
			return 0;

}

#ifdef QNAP_TS
static int
get_lvm_volname(char *volname, int len, char *path){
	// read link test.
	char linkname[VOL_MAXNAMELEN];
	char tmpPath[VOL_MAXNAMELEN];
	char *volPrefix="/share/";
	struct stat sb;
	ssize_t r;
	int itr;

	ndmpd_log(LOG_DEBUG, "path=%s\n",path);

	if(strncmp(volPrefix, path, strlen(volPrefix))==0){
		strncpy(tmpPath, volPrefix, strlen(volPrefix));
		for(itr=strlen(volPrefix);itr<strlen(path);itr++){
			if(path[itr]=='\0' || path[itr]=='/'){
				tmpPath[itr]='\0';
				break;
			}else
				tmpPath[itr]=path[itr];
		}
		ndmpd_log(LOG_DEBUG, "tmpPath=%s\n",tmpPath);

		r = readlink(tmpPath, linkname, len);
		linkname[r] = '\0';
		for(itr=0;itr<r;itr++){
			if(linkname[itr]=='/'){
				linkname[itr] = '\0';
				break;
			}
		}
		sprintf(volname,"/share/%s",linkname);

	}

	printf("get_lvm_volname = '%s'\n", volname);
}

#else
/*
 * Get the ZFS volume name out of the given path
 */
static int
get_zfs_volname(char *volname, int len, char *path)
{
	ndmpd_log(LOG_DEBUG, "get_zfs_volname");

	struct stat stbuf;
	FILE *mntfp;
	int nmnt;
	int rv;
	int i;
	*volname = '\0';
	struct statfs	*mounts, mnt;
	nmnt = getmntinfo (&mounts, MNT_NOWAIT);
	rv=-1;
	for (i=0;i < nmnt;i++)
	{
		mnt = mounts[i];
		if (strcmp (mnt.f_mntonname,path)==0){
			ndmpd_log(LOG_DEBUG, "vol=%s fs=%s",mnt.f_mntfromname,mnt.f_fstypename);
			if(strcmp(mnt.f_fstypename, MNTTYPE_ZFS) == 0){
				strlcpy(volname, mnt.f_mntfromname, len);
				rv=0;
			}
			break;
		}
	}
	return (rv);
}
#endif

/*
 * Check if the volume type is snapshot volume
 */
bool_t
fs_is_chkpntvol(char *path)
{
	ndmpd_log(LOG_DEBUG, "get_zfs_volname");
					return TRUE;

}


/*
 * Check if the volume is capable of checkpoints
 */
bool_t
fs_is_chkpnt_enabled(char *path)
{
	ndmpd_log(LOG_DEBUG, "fs_is_chkpnt_enabled");
						return TRUE;
}

/*
 * Check if the volume is read-only
 */
bool_t
fs_is_rdonly(char *path)
{
	return (fs_is_chkpntvol(path));
}



