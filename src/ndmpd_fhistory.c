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

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ndmpd.h>
#include <dirent.h>



#include <ndmpd.h>
#include <ndmpd_tar_v3.h>
#include <ndmpd_session.h>
#include <ndmpd_util.h>
#include <ndmpd_func.h>

#include <ndmpd_fhistory.h>



#define	N_PATH_ENTRIES	1000
#define	N_FILE_ENTRIES	N_PATH_ENTRIES
#define	N_DIR_ENTRIES	1000
#define	N_NODE_ENTRIES	1000

/* Figure an average of 32 bytes per path name */
#define	PATH_NAMEBUF_SIZE	(N_PATH_ENTRIES * 32)

/* Figure an average of 16 bytes per file name */
#define	DIR_NAMEBUF_SIZE	(N_PATH_ENTRIES * 16)


static void ndmpd_file_history_cleanup_v3(ndmpd_session_t *session,
    bool_t send_flag);
static ndmpd_module_params_t *get_params(void *cookie);


/*
 * Each file history as a separate message to the client.
 */

/*	defined in ndmpd_tar_v3.c	*/
extern char *get_bk_path_v3(ndmpd_module_params_t *params);



/*
 * Check if it's "." or ".."
 */
static bool_t
rootfs_dot_or_dotdot(char *name)
{
	if (*name != '.')
		return (FALSE);

	if ((name[1] == 0) || (name[1] == '.' && name[2] == 0))
		return (TRUE);

	return (FALSE);
}



/*
 * ************************************************************************
 * NDMP V3 HANDLERS
 * ************************************************************************
 */

/*
 * ndmpd_api_file_history_file_v3
 *
 * Add a file history file entry to the buffer.
 * History data is buffered until the buffer is filled.
 * Full buffers are then sent to the client.
 *
 * Parameters:
 *   cookie   (input) - session pointer.
 *   name     (input) - file name.
 *		      NULL forces buffered data to be sent.
 *   file_stat (input) - file status pointer.
 *   fh_info  (input) - data stream position of file data used during
 *		      fast restore.
 *
 * Returns:
 *   0 - success
 *  -1 - error
 */
int
ndmpd_api_file_history_file_v3(void *cookie, char *name,
    struct stat *file_stat, u_longlong_t fh_info)
{
	ndmpd_log(LOG_DEBUG, "ndmpd_api_file_history_file_v3");

	ndmpd_session_t *session = (ndmpd_session_t *)cookie;
	ndmp_file_v3 *file_entry;
	ndmp_file_name_v3 *file_name_entry;
	ndmp_file_stat_v3 *file_stat_entry;
	ndmp_fh_add_file_request_v3 request;

	if (name == NULL && session->ns_fh_v3.fh_file_index == 0)
		return (0);

	/*
	 * If the buffer does not have space
	 * for the current entry, send the buffered data to the client.
	 * A NULL name indicates that any buffered data should be sent.
	 */
	if (name == NULL ||
	    session->ns_fh_v3.fh_file_index == N_FILE_ENTRIES ||
	    session->ns_fh_v3.fh_file_name_buf_index + strlen(name) + 1 >
	    PATH_NAMEBUF_SIZE) {

		ndmpd_log(LOG_DEBUG, "sending %ld entries",
		    session->ns_fh_v3.fh_file_index);

		request.files.files_len = session->ns_fh_v3.fh_file_index;
		request.files.files_val = session->ns_fh_v3.fh_files;

		if (ndmp_send_request_lock(session->ns_connection,
		    NDMP_FH_ADD_FILE, NDMP_NO_ERR, (void *) &request, 0) < 0) {
			ndmpd_log(LOG_DEBUG,
			    "Sending ndmp_fh_add_file request");
			return (-1);
		}

		session->ns_fh_v3.fh_file_index = 0;
		session->ns_fh_v3.fh_file_name_buf_index = 0;
	}

	if (name == NULL)
		return (0);

	if (session->ns_fh_v3.fh_files == 0) {
		session->ns_fh_v3.fh_files = ndmp_malloc(sizeof (ndmp_file_v3) *
		    N_FILE_ENTRIES);
		if (session->ns_fh_v3.fh_files == 0)
			return (-1);
	}

	if (session->ns_fh_v3.fh_file_names == 0) {
		session->ns_fh_v3.fh_file_names =
		    ndmp_malloc(sizeof (ndmp_file_name_v3) * N_FILE_ENTRIES);
		if (session->ns_fh_v3.fh_file_names == 0)
			return (-1);
	}

	if (session->ns_fh_v3.fh_file_name_buf == 0) {
		session->ns_fh_v3.fh_file_name_buf =
		    ndmp_malloc(sizeof (char) * PATH_NAMEBUF_SIZE);
		if (session->ns_fh_v3.fh_file_name_buf == 0)
			return (-1);
	}

	if (session->ns_fh_v3.fh_file_stats == 0) {
		session->ns_fh_v3.fh_file_stats =
		    ndmp_malloc(sizeof (ndmp_file_stat_v3) * N_FILE_ENTRIES);
		if (session->ns_fh_v3.fh_file_stats == 0)
			return (-1);
	}

	file_entry =
	    &session->ns_fh_v3.fh_files[session->ns_fh_v3.fh_file_index];
	file_name_entry =
	    &session->ns_fh_v3.fh_file_names[session->ns_fh_v3.fh_file_index];
	file_stat_entry =
	    &session->ns_fh_v3.fh_file_stats[session->ns_fh_v3.fh_file_index];
	file_entry->names.names_len = 1;
	file_entry->names.names_val = file_name_entry;
	file_entry->stats.stats_len = 1;
	file_entry->stats.stats_val = file_stat_entry;
	file_entry->node = long_long_to_quad(file_stat->st_ino);
	file_entry->fh_info = long_long_to_quad(fh_info);

	file_name_entry->fs_type = NDMP_FS_UNIX;
	file_name_entry->ndmp_file_name_v3_u.unix_name =
	    &session->ns_fh_v3.fh_file_name_buf[session->
	    ns_fh_v3.fh_file_name_buf_index];
	(void) strlcpy(&session->ns_fh_v3.fh_file_name_buf[session->
	    ns_fh_v3.fh_file_name_buf_index], name, PATH_NAMEBUF_SIZE);
	session->ns_fh_v3.fh_file_name_buf_index += strlen(name) + 1;
	ndmpd_get_file_entry_type(file_stat->st_mode, &file_stat_entry->ftype);

	file_stat_entry->invalid = 0;
	file_stat_entry->fs_type = NDMP_FS_UNIX;
	file_stat_entry->mtime = file_stat->st_mtime;
	file_stat_entry->atime = file_stat->st_atime;
	file_stat_entry->ctime = file_stat->st_ctime;
	file_stat_entry->owner = file_stat->st_uid;
	file_stat_entry->group = file_stat->st_gid;
	file_stat_entry->fattr = file_stat->st_mode & 0x0fff;
	file_stat_entry->size =
	    long_long_to_quad((u_longlong_t)file_stat->st_size);
	file_stat_entry->links = file_stat->st_nlink;

	session->ns_fh_v3.fh_file_index++;

	return (0);
}


/*
 * ndmpd_api_file_history_dir_v3
 *
 * Add a file history dir entry to the buffer.
 * History data is buffered until the buffer is filled.
 * Full buffers are then sent to the client.
 *
 * Parameters:
 *   cookie (input) - session pointer.
 *   name   (input) - file name.
 *		    NULL forces buffered data to be sent.
 *   node   (input) - file inode.
 *   parent (input) - file parent inode.
 *		    Should equal node if the file is the root of
 *		    the filesystem and has no parent.
 *
 * Returns:
 *   0 - success
 *  -1 - error
 */
int
ndmpd_api_file_history_dir_v3(void *cookie, char *name, u_long node, u_long parent)
{

	ndmpd_log(LOG_DEBUG, "ndmpd_api_file_history_dir_v3");

	ndmpd_session_t *session = (ndmpd_session_t *)cookie;
	ndmp_dir_v3 *dir_entry;
	ndmp_file_name_v3 *dir_name_entry;
	ndmp_fh_add_dir_request_v3 request;

	if (name == NULL && session->ns_fh_v3.fh_dir_index == 0)
		return (0);

	/*
	 * If the buffer does not have space
	 * for the current entry, send the buffered data to the client.
	 * A NULL name indicates that any buffered data should be sent.
	 */
	if (name == NULL ||
	    session->ns_fh_v3.fh_dir_index == N_DIR_ENTRIES ||
	    session->ns_fh_v3.fh_dir_name_buf_index + strlen(name) + 1 >
	    DIR_NAMEBUF_SIZE) {

		ndmpd_log(LOG_DEBUG, "sending %ld entries",
		    session->ns_fh_v3.fh_dir_index);

		request.dirs.dirs_val = session->ns_fh_v3.fh_dirs;
		request.dirs.dirs_len = session->ns_fh_v3.fh_dir_index;

		if (ndmp_send_request_lock(session->ns_connection,
		    NDMP_FH_ADD_DIR, NDMP_NO_ERR, (void *) &request, 0) < 0) {
			ndmpd_log(LOG_DEBUG,
			    "Sending ndmp_fh_add_dir request");
			return (-1);
		}

		session->ns_fh_v3.fh_dir_index = 0;
		session->ns_fh_v3.fh_dir_name_buf_index = 0;
	}

	if (name == NULL)
		return (0);

	if (session->ns_fh_v3.fh_dirs == 0) {
		session->ns_fh_v3.fh_dirs =
		    ndmp_malloc(sizeof (ndmp_dir_v3) * N_DIR_ENTRIES);
		if (session->ns_fh_v3.fh_dirs == 0)
			return (-1);
	}

	if (session->ns_fh_v3.fh_dir_names == 0) {
		session->ns_fh_v3.fh_dir_names =
		    ndmp_malloc(sizeof (ndmp_file_name_v3) * N_DIR_ENTRIES);
		if (session->ns_fh_v3.fh_dir_names == 0)
			return (-1);
	}

	if (session->ns_fh_v3.fh_dir_name_buf == 0) {
		session->ns_fh_v3.fh_dir_name_buf =
		    ndmp_malloc(sizeof (char) * DIR_NAMEBUF_SIZE);
		if (session->ns_fh_v3.fh_dir_name_buf == 0)
			return (-1);
	}

	dir_entry = &session->ns_fh_v3.fh_dirs[session->ns_fh_v3.fh_dir_index];
	dir_name_entry =
	    &session->ns_fh_v3.fh_dir_names[session->ns_fh_v3.fh_dir_index];

	dir_name_entry->fs_type = NDMP_FS_UNIX;

	dir_name_entry->ndmp_file_name_v3_u.unix_name = &session->ns_fh_v3.fh_dir_name_buf[session->ns_fh_v3.fh_dir_name_buf_index];

	(void) strlcpy(&session->ns_fh_v3.fh_dir_name_buf[session->ns_fh_v3.fh_dir_name_buf_index], name, PATH_NAMEBUF_SIZE);

	session->ns_fh_v3.fh_dir_name_buf_index += PATH_NAMEBUF_SIZE ;
	//session->ns_fh_v3.fh_dir_name_buf_index += strlen(name)+1 ;

	dir_entry->names.names_len = 1;
	dir_entry->names.names_val = dir_name_entry;
	dir_entry->node = long_long_to_quad(node);
	dir_entry->parent = long_long_to_quad(parent);

	session->ns_fh_v3.fh_dir_index++;

	return (0);
}


/*
 * ndmpd_api_file_history_node_v3
 *
 * Add a file history node entry to the buffer.
 * History data is buffered until the buffer is filled.
 * Full buffers are then sent to the client.
 *
 * Parameters:
 *   cookie   (input) - session pointer.
 *   node     (input) - file inode.
 *		must match a node from a prior ndmpd_api_file_history_dir()
 *		      call.
 *   file_stat (input) - file status pointer.
 *		      0 forces buffered data to be sent.
 *   fh_info  (input) - data stream position of file data used during
 *		      fast restore.
 *
 * Returns:
 *   0 - success
 *  -1 - error.
 */
int
ndmpd_api_file_history_node_v3(void *cookie, u_long node,
    struct stat *file_stat, u_longlong_t fh_info)
{
	ndmpd_log(LOG_DEBUG, "ndmpd_api_file_history_node_v3 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
	ndmpd_session_t *session = (ndmpd_session_t *)cookie;
	ndmp_node_v3 *node_entry;
	ndmp_file_stat_v3 *file_stat_entry;
	ndmp_fh_add_node_request_v3 request;

	if (file_stat == NULL && session->ns_fh_v3.fh_node_index == 0)
		return (0);

	/*
	 * If the buffer does not have space
	 * for the current entry, send the buffered data to the client.
	 * A 0 file_stat pointer indicates that any buffered data should
	 * be sent.
	 */
	if (file_stat == NULL ||
	    session->ns_fh_v3.fh_node_index == N_NODE_ENTRIES) {
		ndmpd_log(LOG_DEBUG, "sending %ld entries",
		    session->ns_fh_v3.fh_node_index);

		/*
		 * Need to send Dir entry as well. Since Dir entry is more
		 * than a Node entry, we may send a Node entry that hasn't
		 * had its Dir entry sent. Therefore, we need to flush Dir
		 * entry as well every time the Dir entry is sent.
		 */
		(void) ndmpd_api_file_history_dir_v3(session, 0, 0, 0);

		request.nodes.nodes_len = session->ns_fh_v3.fh_node_index;
		request.nodes.nodes_val = session->ns_fh_v3.fh_nodes;

		if (ndmp_send_request_lock(session->ns_connection,
		    NDMP_FH_ADD_NODE,
		    NDMP_NO_ERR, (void *) &request, 0) < 0) {
			ndmpd_log(LOG_DEBUG,
			    "Sending ndmp_fh_add_node request");
			return (-1);
		}

		session->ns_fh_v3.fh_node_index = 0;
	}

	if (file_stat == NULL)
		return (0);

	if (session->ns_fh_v3.fh_nodes == 0) {
		session->ns_fh_v3.fh_nodes =
		    ndmp_malloc(sizeof (ndmp_node_v3) * N_NODE_ENTRIES);
		if (session->ns_fh_v3.fh_nodes == 0)
			return (-1);
	}

	if (session->ns_fh_v3.fh_node_stats == 0) {
		session->ns_fh_v3.fh_node_stats =
		    ndmp_malloc(sizeof (ndmp_file_stat_v3) * N_NODE_ENTRIES);
		if (session->ns_fh_v3.fh_node_stats == 0)
			return (-1);
	}

	node_entry =
	    &session->ns_fh_v3.fh_nodes[session->ns_fh_v3.fh_node_index];

	file_stat_entry =
	    &session->ns_fh_v3.fh_node_stats[session->ns_fh_v3.fh_node_index];
	ndmpd_get_file_entry_type(file_stat->st_mode, &file_stat_entry->ftype);

	file_stat_entry->invalid = 0;
	file_stat_entry->fs_type = NDMP_FS_UNIX;
	file_stat_entry->mtime = file_stat->st_mtime;
	file_stat_entry->atime = file_stat->st_atime;
	file_stat_entry->ctime = file_stat->st_ctime;
	file_stat_entry->owner = file_stat->st_uid;
	file_stat_entry->group = file_stat->st_gid;

	// solaris have some issue on this.
//	file_stat_entry->fattr = file_stat->st_mode & 0x0fff;
	file_stat_entry->fattr = file_stat->st_mode;
	file_stat_entry->size =
	    long_long_to_quad((u_longlong_t)file_stat->st_size);
	file_stat_entry->links = file_stat->st_nlink;

	node_entry->stats.stats_len = 1;
	node_entry->stats.stats_val = file_stat_entry;
	node_entry->node = long_long_to_quad((u_longlong_t)node);
	node_entry->fh_info = long_long_to_quad(fh_info);

	session->ns_fh_v3.fh_node_index++;

	return (0);
}


/*
 * ************************************************************************
 * NDMP V4 HANDLERS
 * ************************************************************************
 */


/*
 * ndmpd_fhpath_v3_cb
 *
 * Callback function for file history path information
 */
int
ndmpd_fhpath_v3_cb(lbr_fhlog_call_backs_t *cbp, char *path, struct stat *stp,
    u_longlong_t off)
{
	int err;
	ndmp_lbr_params_t *nlp;
	ndmpd_module_params_t *params;

	if (!cbp) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "cbp is NULL");
	} else if (!cbp->fh_cookie) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "cookie is NULL");
	} else if (!path) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "path is NULL");
	} else if (!(nlp = ndmp_get_nlp(cbp->fh_cookie))) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "nlp is NULL");
	} else
		err = 0;

	if (err != 0)
		return (0);

	ndmpd_log(LOG_DEBUG, "pname(%s)", path);

	err = 0;

	/*
	 * should add file filter here?????
	 *
	 * */

	if (NLP_ISSET(nlp, NLPF_FH)) {
		if (!NLP_ISSET(nlp, NLPF_DIRECT)) {
			ndmpd_log(LOG_DEBUG, "DAR NOT SET!");
			off = 0LL;
		}

		params = get_params(cbp->fh_cookie);
		if (!params || !params->mp_file_history_path_func) {
			err = -1;
		} else {
			char *p = ndmp_get_relative_path(get_bk_path_v3(params),
			    path);
			if ((err = ndmpd_api_file_history_file_v3(cbp->
			    fh_cookie, p, stp, off)) < 0)
				ndmpd_log(LOG_DEBUG, "\"%s\" %d", path, err);
		}
	}

	return (err);
}


/*
 * ndmpd_fhdir_v3_cb
 *
 * Callback function for file history dir information
 */
int
ndmpd_fhdir_v3_cb(lbr_fhlog_call_backs_t *cbp, char *dir, struct stat *stp)
{

	int err;
	u_long ino, pino;
	u_long pos;
	ndmp_lbr_params_t *nlp;
	ndmpd_module_params_t *params;
	DIR *dirp;
	char dirpath[PATH_MAX];
	char path[PATH_MAX];
	struct dirent *entry;
	struct stat statbuf;


	ndmpd_log(LOG_DEBUG, "ndmpd_fhdir_v3_cb");
	if (!cbp) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "cbp is NULL");
	} else if (!cbp->fh_cookie) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "cookie is NULL");
	} else if (!dir) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "dir is NULL");
	} else if (!(nlp = ndmp_get_nlp(cbp->fh_cookie))) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "nlp is NULL");
	} else
		err = 0;

	if (err != 0)
		return (0);

	ndmpd_log(LOG_DEBUG, "d(%s)", dir);

	if (!NLP_ISSET(nlp, NLPF_FH))
		return (0);



	params = nlp->nlp_params;
	if (!params || !params->mp_file_history_dir_func)
		return (-1);


	if (stp->st_ino == nlp->nlp_bkdirino){
		pino = ROOT_INODE;
	}else
		pino = stp->st_ino;


	err = 0;

	dirp = opendir(dir);
	if (dirp == NULL)
		return (0);


    while((entry = readdir(dirp)) != NULL) {

		ino=entry->d_fileno;

		if (pino == ROOT_INODE) {
			if (rootfs_dot_or_dotdot(entry->d_name))
				ino = ROOT_INODE;
		} else if (ino == nlp->nlp_bkdirino && IS_DOTDOT( entry->d_name)) {
			ndmpd_log(LOG_DEBUG, "entry->d_name(%s): %lu", entry->d_name, ino);
			ino = ROOT_INODE;
		}

		err = (*params->mp_file_history_dir_func)(cbp->fh_cookie, entry->d_name, ino, pino);
		if (err < 0) {
			ndmpd_log(LOG_DEBUG, "\"%s\": %d", dir, err);
			break;
		}





    }

	(void) closedir(dirp);
	return (err);
}


/*
 * ndmpd_fhnode_v3_cb
 *
 * Callback function for file history node information
 */
int
ndmpd_fhnode_v3_cb(lbr_fhlog_call_backs_t *cbp, char *dir, char *file,
    struct stat *stp, u_longlong_t off)
{
	int err;
	u_long ino;
	ndmp_lbr_params_t *nlp;
	ndmpd_module_params_t *params;
	if (!cbp) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "cbp is NULL");
	} else if (!cbp->fh_cookie) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "cookie is NULL");
	} else if (!dir) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "dir is NULL");
	} else if (!file) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "file is NULL");
	} else if (!stp) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "stp is NULL");
	} else if (!(nlp = ndmp_get_nlp(cbp->fh_cookie))) {
		err = -1;
		ndmpd_log(LOG_DEBUG, "nlp is NULL");
	} else {
		err = 0;
	}

	if (err != 0)
		return (0);



	ndmpd_log(LOG_DEBUG, "d(%s), f(%s)", dir, file);

	err = 0;
	if (NLP_ISSET(nlp, NLPF_FH)) {
		if (!NLP_ISSET(nlp, NLPF_DIRECT))
			off = 0LL;
		if (stp->st_ino == nlp->nlp_bkdirino) {
			ino = ROOT_INODE;
			ndmpd_log(LOG_DEBUG,
			    "bkroot %d -> %d", stp->st_ino, ROOT_INODE);
		} else
			ino = stp->st_ino;

		params = nlp->nlp_params;
		if (!params || !params->mp_file_history_node_func)
			err = -1;
		else if ((err = (*params->mp_file_history_node_func)(cbp->
		    fh_cookie, ino, stp, off)) < 0)
			ndmpd_log(LOG_DEBUG, "\"%s/%s\" %d", dir, file, err);
	}

	return (err);
}


/*
 * ndmp_send_recovery_stat_v3
 *
 * Send the recovery status to the DMA
 */
int
ndmp_send_recovery_stat_v3(ndmpd_module_params_t *params,
    ndmp_lbr_params_t *nlp, int idx, int stat)
{
	ndmpd_log(LOG_DEBUG, "ndmp_send_recovery_stat_v3");
	int rv;
	mem_ndmp_name_v3_t *ep;

	rv = -1;
	if (!params) {
		ndmpd_log(LOG_DEBUG, "params == NULL");
	} else if (!params->mp_file_recovered_func) {
		ndmpd_log(LOG_DEBUG, "paramsfile_recovered_func == NULL");
	} else if (!nlp) {
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
	} else if (idx < 0) {
		ndmpd_log(LOG_DEBUG, "idx(%d) < 0", idx);
	} else if (!(ep = (mem_ndmp_name_v3_t *)MOD_GETNAME(params, idx))) {
		ndmpd_log(LOG_DEBUG, "nlist[%d] == NULL", idx);
	} else if (!ep->nm3_opath) {
		ndmpd_log(LOG_DEBUG, "nlist[%d].nm3_opath == NULL", idx);
	} else {
		ndmpd_log(LOG_DEBUG,
		    "ep[%d].nm3_opath \"%s\"", idx, ep->nm3_opath);
		rv = MOD_FILERECOVERD(params, ep->nm3_opath, stat);
	}

	return (rv);
}


/*
 * ndmpd_path_restored_v3
 *
 * Send the recovery status and the information for the restored
 * path.
 */
/*ARGSUSED*/
int
ndmpd_path_restored_v3(lbr_fhlog_call_backs_t *cbp, char *name,
    struct stat *st, u_longlong_t ll_idx)
{
	ndmpd_log(LOG_DEBUG, "ndmpd_path_restored_v3");
	int rv;
	ndmp_lbr_params_t *nlp;
	ndmpd_module_params_t *params;
	int idx = (int)ll_idx;

	if (!cbp) {
		ndmpd_log(LOG_DEBUG, "cbp is NULL");
		return (-1);
	}
	if (!name) {
		ndmpd_log(LOG_DEBUG, "name is NULL");
		return (-1);
	}

	ndmpd_log(LOG_DEBUG, "name: \"%s\", idx: %d", name, idx);

	nlp = ndmp_get_nlp(cbp->fh_cookie);
	if (!nlp) {
		ndmpd_log(LOG_DEBUG, "nlp is NULL");
		return (-1);
	}
	if (idx < 0 || idx >= nlp->nlp_nfiles) {
		ndmpd_log(LOG_DEBUG, "Invalid idx: %d", idx);
		return (-1);
	}
	params = nlp->nlp_params;
	if (!params || !params->mp_file_recovered_func)
		return (-1);

	if (nlp->nlp_lastidx == -1)
		nlp->nlp_lastidx = idx;

	rv = 0;

	/*
	 * Note: We should set the nm3_err here.
	 */
	if (nlp->nlp_lastidx != idx) {
		rv = ndmp_send_recovery_stat_v3(params, nlp, nlp->nlp_lastidx,
		    0);
		nlp->nlp_lastidx = idx;
	}

	return (rv);
}



/*
 * ndmpd_file_history_init
 *
 * Initialize file history variables.
 * Note that the entry and name buffers are not allocated here.
 * Since it is not know if the backup module will be sending file history
 * data or what kind of data (path or dir/node), the entry and name
 * buffers are not allocated until the first call to one of the file history
 * entry functions is made. This way resources are only allocated as
 * needed.
 *
 * Parameters:
 *   session (input) - session pointer.
 *
 * Returns:
 *   void
 */
void
ndmpd_file_history_init(ndmpd_session_t *session)
{
	session->ns_fh.fh_path_entries = 0;
	session->ns_fh.fh_dir_entries = 0;
	session->ns_fh.fh_node_entries = 0;
	session->ns_fh.fh_path_name_buf = 0;
	session->ns_fh.fh_dir_name_buf = 0;
	session->ns_fh.fh_path_index = 0;
	session->ns_fh.fh_dir_index = 0;
	session->ns_fh.fh_node_index = 0;
	session->ns_fh.fh_path_name_buf_index = 0;
	session->ns_fh.fh_dir_name_buf_index = 0;

	/*
	 * V3.
	 */
	session->ns_fh_v3.fh_files = 0;
	session->ns_fh_v3.fh_dirs = 0;
	session->ns_fh_v3.fh_nodes = 0;
	session->ns_fh_v3.fh_file_names = 0;
	session->ns_fh_v3.fh_dir_names = 0;
	session->ns_fh_v3.fh_file_stats = 0;
	session->ns_fh_v3.fh_node_stats = 0;
	session->ns_fh_v3.fh_file_name_buf = 0;
	session->ns_fh_v3.fh_dir_name_buf = 0;
	session->ns_fh_v3.fh_file_index = 0;
	session->ns_fh_v3.fh_dir_index = 0;
	session->ns_fh_v3.fh_node_index = 0;
	session->ns_fh_v3.fh_file_name_buf_index = 0;
	session->ns_fh_v3.fh_dir_name_buf_index = 0;
}


/*
 * ndmpd_file_history_cleanup_v3
 *
 * Send (or discard) any buffered file history entries.
 *
 * Parameters:
 *   session  (input) - session pointer.
 *   send_flag (input) - if TRUE  buffered entries are sent.
 *		      if FALSE buffered entries are discarded.
 *
 * Returns:
 *   void
 */
static void
ndmpd_file_history_cleanup_v3(ndmpd_session_t *session, bool_t send_flag)
{
	if (send_flag == TRUE) {
		(void) ndmpd_api_file_history_file_v3(session, 0, 0, 0);
		(void) ndmpd_api_file_history_dir_v3(session, 0, 0, 0);
		(void) ndmpd_api_file_history_node_v3(session, 0, 0, 0);
	}

	if (session->ns_fh_v3.fh_files != 0) {
		free(session->ns_fh_v3.fh_files);
		session->ns_fh_v3.fh_files = 0;
	}
	if (session->ns_fh_v3.fh_dirs != 0) {
		free(session->ns_fh_v3.fh_dirs);
		session->ns_fh_v3.fh_dirs = 0;
	}
	if (session->ns_fh_v3.fh_nodes != 0) {
		free(session->ns_fh_v3.fh_nodes);
		session->ns_fh_v3.fh_nodes = 0;
	}
	if (session->ns_fh_v3.fh_file_names != 0) {
		free(session->ns_fh_v3.fh_file_names);
		session->ns_fh_v3.fh_file_names = 0;
	}
	if (session->ns_fh_v3.fh_dir_names != 0) {
		free(session->ns_fh_v3.fh_dir_names);
		session->ns_fh_v3.fh_dir_names = 0;
	}
	if (session->ns_fh_v3.fh_file_stats != 0) {
		free(session->ns_fh_v3.fh_file_stats);
		session->ns_fh_v3.fh_file_stats = 0;
	}
	if (session->ns_fh_v3.fh_node_stats != 0) {
		free(session->ns_fh_v3.fh_node_stats);
		session->ns_fh_v3.fh_node_stats = 0;
	}
	if (session->ns_fh_v3.fh_file_name_buf != 0) {
		free(session->ns_fh_v3.fh_file_name_buf);
		session->ns_fh_v3.fh_file_name_buf = 0;
	}
	if (session->ns_fh_v3.fh_dir_name_buf != 0) {
		free(session->ns_fh_v3.fh_dir_name_buf);
		session->ns_fh_v3.fh_dir_name_buf = 0;
	}

	session->ns_fh_v3.fh_file_index = 0;
	session->ns_fh_v3.fh_dir_index = 0;
	session->ns_fh_v3.fh_node_index = 0;
	session->ns_fh_v3.fh_file_name_buf_index = 0;
	session->ns_fh_v3.fh_dir_name_buf_index = 0;
}


/*
 * ndmpd_file_history_cleanup
 *
 * Send any pending posts and clean up
 */
void
ndmpd_file_history_cleanup(ndmpd_session_t *session, bool_t send_flag)
{
	switch (session->ns_protocol_version) {
	case 1:
	case 2:
	case 3:
	case 4:
		ndmpd_file_history_cleanup_v3(session, send_flag);
		break;
	default:
		ndmpd_log(LOG_DEBUG, "Unknown version %d",
		    session->ns_protocol_version);
	}
}

/*
 * get_params
 *
 * Callbacks from LBR.
 */
static ndmpd_module_params_t *
get_params(void *cookie)
{
	ndmp_lbr_params_t *nlp;

	if ((nlp = ndmp_get_nlp(cookie)) == NULL)
		return (NULL);

	return (nlp->nlp_params);
}


