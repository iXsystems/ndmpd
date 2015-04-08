/*
 * Copyright 2009 Sun Microsystems, Inc.  
 * Copyright 2015 Marcelo Araujo <araujo@FreeBSD.org>.
 * All rights reserved.
 *
 * Use is subject to license terms.
 */

/*
 * BSD 3 Clause License
 *
 * Copyright (c) 2007, The Storage Networking Industry Association.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *      - Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in
 *        the documentation and/or other materials provided with the
 *        distribution.
 *
 *      - Neither the name of The Storage Networking Industry Association (SNIA)
 *        nor the names of its contributors may be used to endorse or promote
 *        products derived from this software without specific prior written
 *        permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ndmpd_snapshot.h>
#include <tlm.h>

#include<assert.h>

/* getmntinfo */
#include <sys/mount.h>

static int get_zfs_volname(char *volname, int len, char *path);
static int ndmpd_create_snapshot(char *volname, char *jname,char *snapshot_path);

/*
 *
 * 	SNAPSHOT FUNCTIONS
 *
 *
 * */
ndmp_chkpnt_vol_t *chkpnt_vols = NULL;

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

	if (vol_name == 0 || get_zfs_volname(vol, sizeof (vol), vol_name) == -1)
			return (-1);

	ndmpd_log(LOG_DEBUG, "ndmpd_take_snapshot vol found=%s",vol);

	erc = ndmpd_create_snapshot(vol, jname, snapshot_path);

	if(erc==0){
		ndmpd_log(LOG_DEBUG, "ndmpd_take_snapshot snapshot path found=%s",snapshot_path);
	}

	ndmpd_log(LOG_DEBUG, "erc=%d",erc);

	return (erc);
}

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
	/* XXX: TO DO */
	return (0);
}

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

	/* XXX: TODO */
	ndmpd_log(LOG_DEBUG, "Snapshot not supported yet.");
	return (-1);
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
