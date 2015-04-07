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

#include <handler.h>

#ifdef QNAP_TS
	#include <mntent.h>
	#include <sys/vfs.h>
	#include <dirent.h>
	#include <snapshot_ts.h>
#else
	/*	getmntinfo	*/
	#include <sys/mount.h>
#endif



#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*	open	*/
#include <sys/fcntl.h>

/*	gethostid*/
#include <unistd.h>

#include <mnt_variable.h>

#include <sys/utsname.h>
#include <unistd.h>





#include <ndmpd.h>
#include <ndmpd_func.h>
#include <ndmpd_util.h>
#include <ndmpd_table.h>
#include <ndmpd_session.h>


/*
 * ************************************************************************
 * NDMP V3 HANDLERS
 * ************************************************************************
 */

/*
 * ndmpd_config_get_host_info_v3
 *
 * This handler handles the ndmp_config_get_host_info request.
 * Host specific information is returned.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/
void
ndmpd_config_get_host_info_v3(ndmp_connection_t *connection, void *body)
{
	ndmp_config_get_host_info_reply_v3 reply;
	char buf[MAXHOSTNAMELEN+1];
	struct utsname uts;
	char hostidstr[16];
	u_long hostid;

	ndmpd_log(LOG_DEBUG, "entering ndmpd_config_get_host_info_v3");

	(void) memset((void*)&reply, 0, sizeof (reply));
	(void) memset(buf, 0, sizeof (buf));
	(void) gethostname(buf, sizeof (buf));


	reply.error = NDMP_NO_ERR;
	reply.hostname = buf;
	(void) uname(&uts);
	reply.os_type = uts.sysname;
	reply.os_vers = uts.release;


	reply.os_type = OS_TYPE;
	reply.os_vers = OS_VERSION;

	sprintf(hostidstr,"%ld",gethostid());

	/*
	 * Convert the hostid to hex. The returned string must match
	 * the string returned by hostid(1).
	 */
	hostid = strtoul(hostidstr, 0, 0);
	(void) snprintf(hostidstr, sizeof (hostidstr), "%lx", hostid);
	reply.hostid = hostidstr;

	ndmp_send_reply(connection, (void *) &reply,
	    "sending ndmp_config_get_host_info reply");
}


/*
 * ndmpd_config_get_connection_type_v3
 *
 * This handler handles the ndmp_config_get_connection_type_request.
 * A list of supported data connection types is returned.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/
void
ndmpd_config_get_connection_type_v3(ndmp_connection_t *connection,
    void *body)
{
	ndmp_config_get_connection_type_reply_v3 reply;
	ndmp_addr_type addr_types[2];

	ndmpd_log(LOG_DEBUG, "entering ndmpd_config_get_connection_type_v3");

	(void) memset((void*)&reply, 0, sizeof (reply));

	reply.error = NDMP_NO_ERR;

	addr_types[0] = NDMP_ADDR_LOCAL;
	addr_types[1] = NDMP_ADDR_TCP;
	reply.addr_types.addr_types_len = 2;
	reply.addr_types.addr_types_val = addr_types;

	ndmp_send_reply(connection, (void *) &reply,
	    "sending config_get_connection_type_v3 reply");
}



/*
 * ndmpd_config_get_auth_attr_v3
 *
 * This handler handles the ndmp_config_get_auth_attr request.
 * Authorization type specific information is returned.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
void
ndmpd_config_get_auth_attr_v3(ndmp_connection_t *connection, void *body)
{
	ndmp_config_get_auth_attr_request *request;
	ndmp_config_get_auth_attr_reply reply;
	ndmpd_session_t *session = ndmp_get_client_data(connection);

	ndmpd_log(LOG_DEBUG, "entering ndmpd_config_get_auth_attr_v3");

	request = (ndmp_config_get_auth_attr_request *)body;

	(void) memset((void*)&reply, 0, sizeof (reply));
	reply.error = NDMP_NO_ERR;
	reply.server_attr.auth_type = request->auth_type;

	switch (request->auth_type) {
	case NDMP_AUTH_TEXT:
		break;
	case NDMP_AUTH_MD5:
		/* Create a 64 bytes random session challenge */
		randomize(session->ns_challenge, MD5_CHALLENGE_SIZE);
		(void) memcpy(reply.server_attr.ndmp_auth_attr_u.challenge,
		    session->ns_challenge, MD5_CHALLENGE_SIZE);
		break;
	case NDMP_AUTH_NONE:
		/* FALL THROUGH */
	default:
		ndmpd_log(LOG_ERR, "Invalid authentication type: %d.",
		    request->auth_type);
		ndmpd_log(LOG_ERR,
		    "Supported authentication types are md5 and cleartext.");
		reply.error = NDMP_ILLEGAL_ARGS_ERR;
		break;
	}

	ndmp_send_reply(connection, (void *) &reply,
	    "sending ndmp_config_get_auth_attr_v3 reply");
}


/*
 * ndmpd_config_get_butype_info_v3
 *
 * This handler handles the ndmp_config_get_butype_info_request.
 * Information about all supported backup types are returned.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/
void
ndmpd_config_get_butype_info_v3(ndmp_connection_t *connection, void *body)
{
	ndmp_config_get_butype_info_reply_v3 reply;
	ndmp_butype_info info[2];
	ndmp_pval envs[9];
	u_long attrs;
	ndmp_pval *envp = envs;

	ndmpd_log(LOG_DEBUG, "entering ndmpd_config_get_butype_info_v3");

	(void) memset((void*)&reply, 0, sizeof (reply));

	/*
	 * Supported environment variables and their default values.
	 * The environment variables for dump and tar format are the
	 * same, because we use the same backup engine for both.
	 */
	NDMP_SETENV(envp, "PREFIX", "");
	NDMP_SETENV(envp, "FILESYSTEM", "");
	NDMP_SETENV(envp, "UPDATE", "");
	NDMP_SETENV(envp, "HIST", "n");
	NDMP_SETENV(envp, "QNAP_EFILE0", "");
	NDMP_SETENV(envp, "QNAP_EDIR0", "");
	NDMP_SETENV(envp, "FILES", "");
	NDMP_SETENV(envp, "SNAPSURE", "NO");
	NDMP_SETENV(envp, "LEVEL", "0");



	attrs = NDMP_BUTYPE_BACKUP_FILE_HISTORY |
	    NDMP_BUTYPE_RECOVER_FILELIST |
	    NDMP_BUTYPE_BACKUP_DIRECT |
	    NDMP_BUTYPE_BACKUP_INCREMENTAL |
	    NDMP_BUTYPE_BACKUP_UTF8 |
	    NDMP_BUTYPE_RECOVER_UTF8;


	/* tar backup type */
	info[0].butype_name = "tar";
	info[0].default_env.default_env_len = ARRAY_LEN(envs, ndmp_pval);
	info[0].default_env.default_env_val = envs;
	info[0].attrs = attrs;

	/* dump backup type */
	info[1].butype_name = "dump";
	info[1].default_env.default_env_len = ARRAY_LEN(envs, ndmp_pval);
	info[1].default_env.default_env_val = envs;
	info[1].attrs = attrs;

	reply.error = NDMP_NO_ERR;
	reply.butype_info.butype_info_len = ARRAY_LEN(info, ndmp_butype_info);
	reply.butype_info.butype_info_val = info;

	ndmp_send_reply(connection, (void *)&reply,
	    "sending ndmp_config_get_butype_info reply");
}

/*
 * ndmpd_config_get_fs_info_v3
 *
 * This handler handles the ndmp_config_get_fs_info_request.
 * Information about all mounted file systems is returned.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/

void
ndmpd_config_get_fs_info_v3(ndmp_connection_t *connection, void *body)
{

	ndmpd_log(LOG_DEBUG, "entering ndmpd_config_get_fs_info_v3");

	ndmp_config_get_fs_info_reply_v3 reply;
	ndmp_fs_info_v3 *fsip, *fsip_save; /* FS info pointer */

	int i, nmnt=0, mntCnt=0;
	int log_dev_len,phy_dev_len;

	ndmp_pval *envp, *save;

	(void) memset((void*)&reply, 0, sizeof (reply));
	reply.error = NDMP_NO_ERR;

#ifdef QNAP_TS

	struct mntent *ent;
	DIR *dirp;
	struct dirent *entry;
	struct stat statbuf;
	char path[PATH_MAX+1];
	FILE *fp = NULL;
	fp = setmntent("/proc/mounts", "r");
	char linkname[PATH_MAX+1];
	int prefix_len = strlen("/share/");
	bool_t supportSnapshot=FALSE;

//	// return if we support SNAPSURE.
//	// For TS, not all model will support it.
//	Snapshot_Feature snap_feature;
//	NAS_Get_Snapshot_Feature(&snap_feature);
//
//	if(snap_feature.support==1){
//		supportSnapshot=TRUE;
//	}else
		supportSnapshot=FALSE;

	if (!fp) {
		nmnt=0;
	}else{
		while ((ent = getmntent(fp))) {

			if (!IS_VALID_FS(ent->mnt_type))
				continue;

			if (strncmp (ent->mnt_dir,"/share/",prefix_len)!=0)
				continue;

			ndmpd_log(LOG_DEBUG, "searching on  %s",ent->mnt_dir);
		    if((dirp = opendir(ent->mnt_dir)) == NULL) {
		    	ndmpd_log(LOG_DEBUG, "open DIR fail");
		        continue ;
		    }
		    while((entry = readdir(dirp)) != NULL) {

		    	if(strcmp(".",entry->d_name) == 0 || strcmp("..",entry->d_name) == 0)
		    		continue;

		        sprintf(path, "/share/%s",entry->d_name);

		        // we have to make sure the folders under the mount point
		        // is exactly the same mount to the linked folder.
		        if(lstat(path,&statbuf)==0){
		        	memset(linkname, PATH_MAX+1,0);
	        		int r = readlink(path, linkname, statbuf.st_size + 1);
	        		linkname[r] = '\0';

		        	ndmpd_log(LOG_DEBUG, "link path = %s",linkname);
		        	if(strncmp(ent->mnt_dir+prefix_len, linkname, strlen(ent->mnt_dir)-prefix_len)==0){
		        		ndmpd_log(LOG_DEBUG, "they are matched.");
						nmnt++;
		        	}
		        	else
		        		ndmpd_log(LOG_DEBUG, "they don't match");
		        }

		    }
		    closedir(dirp);
		}
		endmntent(fp);
	}
	ndmpd_log(LOG_DEBUG, "share folder count = %d",nmnt);

#else

	struct statfs	*mounts, mnt;
	nmnt = getmntinfo (&mounts, MNT_NOWAIT);

#endif

	/* nothing was found, send an empty reply */
	if (nmnt == 0) {
		ndmpd_log(LOG_ERR, "No file system found.");
		ndmp_send_reply(connection, (void *)&reply, "sending ndmp_config_get_fs_info reply");
		return;
	}


	fsip_save = fsip = ndmp_malloc(sizeof (ndmp_fs_info_v3) * nmnt);
	if (!fsip) {
		reply.error = NDMP_NO_MEM_ERR;
		ndmp_send_reply(connection, (void *)&reply, "error sending ndmp_config_get_fs_info reply");
		return;
	}


#ifdef QNAP_TS

	struct statfs mnt;
	uint64_t total_size;
	uint64_t used_size;
	uint64_t avail_size;
	uint64_t total_inodes;
	uint64_t used_inodes;

	fp = setmntent("/proc/mounts", "r");

	if (fp)
	{
		while ((ent = getmntent(fp)))
		{
			if (!IS_VALID_FS(ent->mnt_type))
				continue;

			if (strncmp (ent->mnt_dir,"/share/",7)!=0)
				continue;

			ndmpd_log(LOG_DEBUG, "ent->mnt_dir=%s, ent->mnt_dir",ent->mnt_dir);
			// the share path of TS is different from ES. /share/$LVM/$sharefolder
			// we will report all folder under the LVM
			 if((dirp = opendir(ent->mnt_dir)) == NULL) {
				continue ;
			}

			 if (statfs(ent->mnt_dir, &mnt) == 0) {
				total_size 	= mnt.f_bsize * mnt.f_blocks;
				used_size	= mnt.f_bsize * ((mnt.f_blocks<=mnt.f_bfree)?0:(mnt.f_blocks-mnt.f_bfree));
				avail_size 	= mnt.f_bsize * mnt.f_bfree;
				total_inodes= mnt.f_files;
				used_inodes = ((mnt.f_files<=mnt.f_ffree)?0:(mnt.f_files - mnt.f_ffree));
			 }else
				 continue;

			// find all folders under the LVM volume
			while((entry = readdir(dirp)) != NULL) {

				if(strcmp(".",entry->d_name) == 0 || strcmp("..",entry->d_name) == 0)
					continue;

				sprintf(path, "/share/%s",entry->d_name);

				if(lstat(path,&statbuf)==0){

					memset(linkname, PATH_MAX+1,0);
	        		int r = readlink(path, linkname, statbuf.st_size + 1);
	        		linkname[r] = '\0';

		        	ndmpd_log(LOG_DEBUG, "link path = %s",linkname);
		        	if(strncmp(ent->mnt_dir+prefix_len, linkname, strlen(ent->mnt_dir)-prefix_len)!=0){
		        		continue;
		        	}

					log_dev_len = strlen(path)+2;
					phy_dev_len = strlen(ent->mnt_fsname)+2;

					fsip->fs_logical_device = ndmp_malloc(log_dev_len);
					fsip->fs_physical_device = ndmp_malloc(phy_dev_len);
					fsip->fs_type = ndmp_malloc(MNTTYPE_LEN);

					if (!fsip->fs_logical_device || !fsip->fs_type) {
						reply.error = NDMP_NO_MEM_ERR;
						free(fsip->fs_logical_device);
						free(fsip->fs_type);
						break;
					}

					(void) snprintf(fsip->fs_type, MNTTYPE_LEN, "%s", ent->mnt_type);
					(void) snprintf(fsip->fs_physical_device, phy_dev_len, "%s",ent->mnt_fsname);
					(void) snprintf(fsip->fs_logical_device, log_dev_len, "%s",path);

					fsip->invalid = 0;
					fsip->total_size 	= long_long_to_quad(total_size);
					fsip->used_size	 	= long_long_to_quad(used_size);
					fsip->avail_size 	= long_long_to_quad(avail_size);
					fsip->total_inodes 	= long_long_to_quad(total_inodes);
					fsip->used_inodes 	= long_long_to_quad(used_inodes);
					fsip->fs_status 	= "";

					save = envp = ndmp_malloc(sizeof (ndmp_pval) * V3_N_FS_ENVS);

					if (!envp) {
						reply.error = NDMP_NO_MEM_ERR;
						break;
					}
					(void) memset((void*)save, 0,
						V3_N_FS_ENVS * sizeof (ndmp_pval));

					fsip->fs_env.fs_env_val = envp;
					NDMP_SETENV(envp, "LOCAL", "N");
					NDMP_SETENV(envp, "TYPE", fsip->fs_type);
					NDMP_SETENV(envp, "AVAILABLE_BACKUP", "dump");

					if(supportSnapshot){
						NDMP_SETENV(envp, "SNAPSURE", "Y");
					}else{
						NDMP_SETENV(envp, "SNAPSURE", "N");
					}

					// check if read-only, if it's read-only, it's not available for recovery.
					if(hasmntopt(ent,MNTOPT_RO)){
						NDMP_SETENV(envp, "AVAILABLE_RECOVERY", "dump");
					}

					fsip->fs_env.fs_env_len = envp - save;
					fsip++;
					mntCnt++;

				}

			}
			closedir(dirp);


		}
		endmntent(fp);
	}

#else

	for (mntCnt=0,i=0;i < nmnt;i++)
	{
		mnt = mounts[i];

		log_dev_len = strlen(mnt.f_mntonname)+2;
		phy_dev_len = strlen(mnt.f_mntfromname)+2;

		if (!IS_VALID_FS(mnt.f_fstypename))
			continue;

		if (strncmp (mnt.f_mntonname,"/share/",7)!=0)
			continue;

		fsip->fs_logical_device = ndmp_malloc(log_dev_len);
		fsip->fs_physical_device = ndmp_malloc(phy_dev_len);
		fsip->fs_type = ndmp_malloc(MNTTYPE_LEN);

		if (!fsip->fs_logical_device || !fsip->fs_type) {
			reply.error = NDMP_NO_MEM_ERR;
			free(fsip->fs_logical_device);
			free(fsip->fs_type);
			break;
		}


		(void) snprintf(fsip->fs_type, MNTTYPE_LEN, "%s",mnt.f_fstypename);
		(void) snprintf(fsip->fs_physical_device, phy_dev_len, "%s",mnt.f_mntfromname);
		(void) snprintf(fsip->fs_logical_device, log_dev_len, "%s",mnt.f_mntonname);


		fsip->invalid = 0;
		fsip->total_size 	= long_long_to_quad(mnt.f_bsize * mnt.f_blocks);
		fsip->used_size	 	= long_long_to_quad(mnt.f_bsize * ((mnt.f_blocks<=mnt.f_bfree)?0:(mnt.f_blocks-mnt.f_bfree)));
		fsip->avail_size 	= long_long_to_quad(mnt.f_bsize * mnt.f_bfree);
		fsip->total_inodes 	= long_long_to_quad(mnt.f_files);
		fsip->used_inodes 	= long_long_to_quad(((mnt.f_files<=mnt.f_ffree)?0:(mnt.f_files - mnt.f_ffree)));
		fsip->fs_status 	= "";

		save = envp = ndmp_malloc(sizeof (ndmp_pval) * V3_N_FS_ENVS);

		if (!envp) {
			reply.error = NDMP_NO_MEM_ERR;
			break;
		}
		(void) memset((void*)save, 0,
		    V3_N_FS_ENVS * sizeof (ndmp_pval));

		fsip->fs_env.fs_env_val = envp;
		NDMP_SETENV(envp, "LOCAL", "Y");
		NDMP_SETENV(envp, "TYPE", fsip->fs_type);
		NDMP_SETENV(envp, "AVAILABLE_BACKUP", "dump");
		NDMP_SETENV(envp, "SNAPSURE", "N");

		// check if read-only, if it's read-only, it's not available for recovery.
		if (!(mnt.f_flags & MNT_RDONLY)) {
			NDMP_SETENV(envp, "AVAILABLE_RECOVERY", "dump");
		}

		fsip->fs_env.fs_env_len = envp - save;
		fsip++;
		mntCnt++;
	}
	mounts=NULL; // memory allocated by getmntinfo() can not be freed by free()
#endif

	/* nothing was found, send an empty reply */
	if (mntCnt == 0) {
		ndmpd_log(LOG_ERR, "No file system found.");
		ndmp_send_reply(connection, (void *)&reply, "sending ndmp_config_get_fs_info reply");
		free(fsip_save);
		return;
	}

	if (reply.error == NDMP_NO_ERR) {
		reply.fs_info.fs_info_len = mntCnt;
		reply.fs_info.fs_info_val = fsip_save;
	} else {
		reply.fs_info.fs_info_len = 0;
		reply.fs_info.fs_info_val = NULL;
	}

	ndmp_send_reply(connection, (void *)&reply,	"error sending ndmp_config_get_fs_info reply");

	fsip = fsip_save;
	while (--mntCnt >= 0) {
		free(fsip->fs_logical_device);
		free(fsip->fs_physical_device);

		free(fsip->fs_env.fs_env_val);
		free(fsip->fs_type);
		fsip++;
	}

	free(fsip_save);
}


/*
 * ndmpd_config_get_tape_info_v3
 *
 * This handler handles the ndmp_config_get_tape_info_request.
 * Information about all connected tape drives is returned.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/
void
ndmpd_config_get_tape_info_v3(ndmp_connection_t *connection, void *body)
{
	ndmpd_log(LOG_DEBUG, "entering ndmpd_config_get_tape_info_v3");

	/*	we do not support Tap Device	*/
	ndmp_config_get_tape_info_reply_v3 reply;
	(void) memset((void*)&reply, 0, sizeof (reply));
	ndmp_send_reply(connection, (void *)&reply, "error sending ndmp_config_get_tape_info reply");

}


/*
 * ndmpd_config_get_scsi_info_v3
 *
 * This handler handles the ndmp_config_get_tape_scsi_request.
 * Information about all connected scsi tape stacker and jukeboxes
 * is returned.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/
void
ndmpd_config_get_scsi_info_v3(ndmp_connection_t *connection, void *body)
{

	ndmpd_log(LOG_DEBUG, "entering ndmpd_config_get_scsi_info_v3");
	/*	we do not support SCSI Device	*/
	ndmp_config_get_scsi_info_reply_v3 reply;
	(void) memset((void*)&reply, 0, sizeof (reply));
	ndmp_send_reply(connection, (void *)&reply, "error sending ndmp_config_get_scsi_info reply");


}


/*
 * ndmpd_config_get_server_info_v3
 *
 * This handler handles the ndmp_config_get_server_info request.
 * Host specific information is returned.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/
void
ndmpd_config_get_server_info_v3(ndmp_connection_t *connection, void *body)
{
	ndmp_config_get_server_info_reply_v3 reply;
	ndmp_auth_type auth_types[2];
	char rev_number[10];
	ndmpd_session_t *session = ndmp_get_client_data(connection);

	ndmpd_log(LOG_DEBUG, "entering ndmpd_config_get_server_info_v3");

	(void) memset((void*)&reply, 0, sizeof (reply));
	reply.error = NDMP_NO_ERR;

	if (connection->conn_authorized ||
	    session->ns_protocol_version != NDMPV4) {
		reply.vendor_name = VENDOR_NAME;
		reply.product_name = PRODUCT_NAME;
		reply.revision_number = SOFTWARE_REV;
	} else {
		reply.vendor_name = "\0";
		reply.product_name = "\0";
		reply.revision_number = "\0";
	}

	ndmpd_log(LOG_DEBUG,
	    "vendor \"%s\", product \"%s\" rev \"%s\"",
	    reply.vendor_name, reply.product_name, reply.revision_number);

	auth_types[0] = NDMP_AUTH_TEXT;
	auth_types[1] = NDMP_AUTH_MD5;
	reply.auth_type.auth_type_len = ARRAY_LEN(auth_types, ndmp_auth_type);
	reply.auth_type.auth_type_val = auth_types;

	ndmp_send_reply(connection, (void *)&reply,
	    "error sending ndmp_config_get_server_info reply");
}



/*
 * ************************************************************************
 * NDMP V4 HANDLERS
 * ************************************************************************
 */

/*
 * ndmpd_config_get_butype_info_v4
 *
 * This handler handles the ndmp_config_get_butype_info_request.
 * Information about all supported backup types are returned.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/
void
ndmpd_config_get_butype_info_v4(ndmp_connection_t *connection, void *body)
{
	ndmp_config_get_butype_info_reply_v4 reply;
	ndmp_butype_info info[1];
	//ndmp_pval envs[13];
	ndmp_pval envs[9];
	u_long attrs;
	ndmp_pval *envp = envs;

	ndmpd_log(LOG_DEBUG, "entering ndmpd_config_get_butype_info_v4");

	(void) memset((void*)&reply, 0, sizeof (reply));

	/*
	 * Supported environment variables and their default values.
	 * The environment variables for dump and tar format are the
	 * same, because we use the same backup engine for both.
	 */
	NDMP_SETENV(envp, "PREFIX", "");
	NDMP_SETENV(envp, "FILESYSTEM", "");
	NDMP_SETENV(envp, "UPDATE", "");
	NDMP_SETENV(envp, "HIST", "n");
	NDMP_SETENV(envp, "QNAP_EFILE0", "");
	NDMP_SETENV(envp, "QNAP_EDIR0", "");
	NDMP_SETENV(envp, "FILES", "");
	NDMP_SETENV(envp, "SNAPSURE", "NO");
	NDMP_SETENV(envp, "LEVEL", "0");




	attrs = NDMP_BUTYPE_RECOVER_FILELIST |
	    NDMP_BUTYPE_BACKUP_DIRECT |
	    NDMP_BUTYPE_BACKUP_INCREMENTAL |
	    NDMP_BUTYPE_BACKUP_UTF8 |
	    NDMP_BUTYPE_RECOVER_UTF8 |
	    NDMP_BUTYPE_BACKUP_FH_FILE |
	    NDMP_BUTYPE_BACKUP_FH_DIR |
	    NDMP_BUTYPE_RECOVER_FH_FILE |
	    NDMP_BUTYPE_RECOVER_FH_DIR;


	/* dump backup type */
	info[0].butype_name = "dump";
	info[0].default_env.default_env_len = ARRAY_LEN(envs, ndmp_pval);
	info[0].default_env.default_env_val = envs;
	info[0].attrs = attrs;

	reply.error = NDMP_NO_ERR;
	reply.butype_info.butype_info_len = ARRAY_LEN(info, ndmp_butype_info);
	reply.butype_info.butype_info_val = info;

	ndmp_send_reply(connection, (void *)&reply,
	    "sending ndmp_config_get_butype_info reply");
}


/*
 * ndmpd_config_get_ext_list_v4
 *
 * This handler handles the ndmpd_config_get_ext_list_v4 request.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/
void
ndmpd_config_get_ext_list_v4(ndmp_connection_t *connection, void *body)
{
	ndmpd_log(LOG_DEBUG, "entering ndmpd_config_get_ext_list_v4");

	ndmp_config_get_ext_list_reply_v4 reply;
	ndmpd_session_t *session = ndmp_get_client_data(connection);

	(void) memset((void*)&reply, 0, sizeof (reply));

	if (session->ns_set_ext_list == FALSE)
		reply.error = NDMP_EXT_DANDN_ILLEGAL_ERR;
	else
		reply.error = NDMP_NO_ERR;

	reply.class_list.class_list_val = NULL;
	reply.class_list.class_list_len = 0;


	ndmp_send_reply(connection, (void *)&reply,
	    "error sending ndmp_config_get_ext_list reply");
}

/*
 * ndmpd_config_set_ext_list_v4
 *
 * This handler handles the ndmpd_config_get_ext_list_v4 request.
 *
 * Parameters:
 *   connection (input) - connection handle.
 *   body       (input) - request message body.
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/
void
ndmpd_config_set_ext_list_v4(ndmp_connection_t *connection, void *body)
{
	ndmp_config_set_ext_list_reply_v4 reply;
	ndmpd_session_t *session = ndmp_get_client_data(connection);

	ndmpd_log(LOG_DEBUG, "entering ndmpd_config_set_ext_list_v4");

	(void) memset((void*)&reply, 0, sizeof (reply));
	if (session->ns_set_ext_list == TRUE) {
		reply.error = NDMP_EXT_DANDN_ILLEGAL_ERR;
	} else {
		session->ns_set_ext_list = TRUE;
		/*
		 * NOTE: for now we are not supporting any extension list,
		 * hence this error, when we start to support extensions,
		 * this should be validated
		 */

		reply.error = NDMP_VERSION_NOT_SUPPORTED_ERR;
	}

	ndmp_send_reply(connection, (void *)&reply,
	    "error sending ndmp_config_set_ext_list reply");
}

