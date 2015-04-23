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

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/acl.h>
#include <utime.h>
#include <unistd.h>
#include <pthread.h>
#include <tlm.h>
#include <tlm_buffers.h>
#include <tlm_lib.h>


#include <pwd.h>
#include <grp.h>
#include <ndmpd_prop.h>

#include <cstack.h>

#include <ndmpd_func.h>
#include <ndmpd_tar_v3.h>

#include "tlm_proto.h"

#include <assert.h>


#define	PM_EXACT_OR_CHILD(m)	((m) == PM_EXACT || (m) == PM_CHILD)

typedef bool_t name_match_fp_t(char *s, char *t);

static void set_acl(char *name,
    tlm_acls_t *acls);
static long restore_file(int *fp,
    char *real_name,
    long size,
    longlong_t huge_size,
    tlm_acls_t *,
    bool_t want_this_file,
    tlm_cmd_t *,
    tlm_job_stats_t *);

static int get_long_name(int lib,
    int drv,
    long recsize,
    char *name,
    long *buf_spot,
    tlm_cmd_t *local_commands);
static int get_humongus_file_header(int lib,
    int	drv,
    long recsize,
    longlong_t *size,
    char *name,
    tlm_cmd_t *);
static int create_directory(char *dir,
    tlm_job_stats_t *);
static int create_hard_link(char *name,
    char *link,
    tlm_acls_t *,
    tlm_job_stats_t *);
static int create_sym_link(char *dst,
    char *target,
    tlm_acls_t *,
    tlm_job_stats_t *);
static int create_fifo(char *name,
    tlm_acls_t *);
static long load_acl_info(int lib,
    int	drv,
    long size,
    tlm_acls_t *,
    long *acl_spot,
    tlm_cmd_t *);
static char *get_read_buffer(int want,
    int	*error,
    int	*actual_size,
    tlm_cmd_t *);
static bool_t wildcard_enabled(void);
static bool_t is_file_wanted(char *name,
    char **sels,
    char **exls,
    int	flags,
    int	*mchtype,
    int	*pos);
static char *catnames(const struct rs_name_maker *rnp,
    char *buf,
    int	pos,
    char *path);

static char *rs_new_name(const struct rs_name_maker *rnp,
    char *real_name,
    int pos,
    char *path);

typedef struct stack_ent {
	char *se_name;
	tlm_acls_t se_acls;
} stack_ent_t;


/*
 * dtree_push
 */
int
dtree_push(cstack_t *stp, char *nmp, tlm_acls_t *acls)
{
	int len;
	stack_ent_t *sp;

	sp = ndmp_malloc(sizeof (stack_ent_t));
	if (!sp || !nmp || !acls) {
		free(sp);
		return (-1);
	}

	len = strlen(nmp) + 1;
	sp->se_name = ndmp_malloc(len);
	if (!sp->se_name) {
		free(sp);
		return (-1);
	}

	(void) strlcpy(sp->se_name, nmp, len);
	(void) memcpy(&sp->se_acls, acls, sizeof (*acls));
	(void) memset(acls, 0, sizeof (tlm_acls_t));

	return (cstack_push(stp, (void *)sp, sizeof (*sp)));
}

/*
 * dtree_pop
 */
int
dtree_pop(cstack_t *stp)
{
	int err;
	stack_ent_t *sp;

	err = cstack_pop(stp, (void **)&sp, (void *)NULL);
	if (err)
		return (-1);

	set_acl(sp->se_name, &sp->se_acls);

	free(sp->se_name);
	free(sp);
	return (err);
}


/*
 * dtree_peek
 */
char *
dtree_peek(cstack_t *stp)
{
	int err;
	stack_ent_t *sp;

	err = cstack_top(stp, (void **)&sp, (void *)NULL);
	if (err)
		return (NULL);

	return (sp->se_name);
}

/*
 * NBU and EBS may not send us the correct file list containing hardlinks
 * during a DAR restore, e.g. they appear always send the first name
 * associated with an inode, even if other link names were
 * selected for the restore.  As a workaround, we use the file name entry
 * in sels[] (ignore the name in the tar header) as restore target.
 */
static char *
rs_darhl_new_name(const struct rs_name_maker *rnp, char *name, char **sels, int *pos,
    char *longname)
{
	int x;

	for (x = 0; sels[x] != NULL; x++) {
		if (strcmp(sels[x], " ")) {
			*pos = x;
			(void) strlcpy(longname, sels[x], TLM_MAX_PATH_NAME);
			ndmpd_log(LOG_DEBUG,
			    "to replace hardlink name [%s], pos [%d]",
			    longname, *pos);

			return (rs_new_name(rnp, name, *pos, longname));
		}
	}

	return (NULL);
}

/*
 * Main DIR restore function for tar
 */
int
tar_getdir(tlm_commands_t *commands,
    tlm_cmd_t *local_commands,
    tlm_job_stats_t *job_stats,
    const struct rs_name_maker *rnp,
    int	lib,
    int	drv,
    char **sels, /* what to get off the tape */
    char **exls, /* what to leave behind */
    int	flags,
    int	DAR, struct hardlink_q *hardlink_q)
{

	ndmpd_log(LOG_DEBUG, "++++++++tar_getdir++++++++");


	int	fp = 0;		/* file being restored ... */
				/*  ...need to preserve across volume changes */
	tlm_acls_t *acls;	/* file access info */

	char longname[TLM_MAX_PATH_NAME];
	char longlink[TLM_MAX_PATH_NAME];
	char hugename[TLM_MAX_PATH_NAME];
	char parentlnk[TLM_MAX_PATH_NAME];
	char name[TLM_MAX_PATH_NAME];


	longlong_t huge_size = 0;	/* size of a HUGE file */
	long	acl_spot;		/* any ACL info on the next volume */
	long	file_size;		/* size of file to restore */
	long	size_left = 0;		/* need this after volume change */
	int	last_action = 0;	/* what we are doing at EOT */
	bool_t multi_volume = FALSE;	/* is this a multi-volume switch ? */
	int	chk_rv;			/* scratch area */

	int	mchtype, pos;
					/*
					 * if an exact match is found for
					 * restore and its position in the
					 * selections list
					 */
	bool_t break_flg;		/* exit the while loop */
	int	rv;
	long nm_end, lnk_end;
	char	*nmp;
	cstack_t *stp;
	char 	*bkpath;

	/*
	 * The directory where temporary files may be created during a partial
	 * non-DAR restore of hardlinks.  It is intended to be initialized by
	 * an environment variable that can be set by user.
	 *
	 * It is not initialized for now.   We keep it here for future use.
	 */
	char *tmplink_dir = NULL;
	int dar_recovered = 0;

	/*
	 * startup
	 */

	acls = ndmp_malloc(sizeof (tlm_acls_t));
	stp = cstack_new();
	if (longname == NULL || longlink == NULL || hugename == NULL ||
	    name == NULL || acls == NULL || stp == NULL || parentlnk == NULL) {
		cstack_delete(stp);
		free(acls);
		return (-TLM_NO_SCRATCH_SPACE);
	}

	acl_spot = 0;
	*hugename = '\0';
	*parentlnk = '\0';
	nm_end = 0;
	*longname = '\0';
	lnk_end = 0;
	*longlink = '\0';
	(void) memset(acls, 0, sizeof (tlm_acls_t));
	if (IS_SET(flags, RSFLG_OVR_ALWAYS)) {
		acls->acl_overwrite = TRUE;
		ndmpd_log(LOG_DEBUG, "RSFLG_OVR_ALWAYS");
	} else if (IS_SET(flags, RSFLG_OVR_UPDATE)) {
		acls->acl_update = TRUE;
		ndmpd_log(LOG_DEBUG, "RSFLG_OVR_UPDATE");
	}

	/*
	 * work
	 */
	rv = 0;
	break_flg = FALSE;


	tlm_tar_hdr_t fake_tar_hdr;
	char	*file_name;
	char	*link_name;
	int	erc;
	int	actual_size;
	bool_t want_this_file;
	int	want = sizeof (tlm_tar_hdr_t);
	tlm_tar_hdr_t *tar_hdr;
	/* The inode of an LF_LINK type. */
	unsigned long hardlink_inode = 0;
	/*
	 * Indicate whether a file with the same inode has been
	 * restored.
	 */
	int hardlink_done = 0;
	/* The path of the restored hardlink file */
	char *hardlink_target = NULL;
	int is_hardlink = 0;

	/*
	 * Whether a temporary file should be created for restoring
	 * hardlink.
	 */
	int hardlink_tmp_file = 0;
	char *hardlink_tmp_name = ".tmphlrsnondar";

	while (commands->tcs_writer != TLM_ABORT &&
	    local_commands->tc_writer != TLM_STOP) {
		(void)pthread_yield();
		hardlink_inode = 0;
		hardlink_done = 0;
		is_hardlink = 0;
		hardlink_tmp_file = 0;

		/* used to make up hardlink_tmp_name */
		static int hardlink_tmp_idx = 0;

		if (break_flg) {
			ndmpd_log(LOG_DEBUG,
			    "Exiting writer thread drive %d", drv);
			break;
		}

		if (multi_volume) {
			ndmpd_log(LOG_DEBUG, "multi_volume %c %ld", last_action, size_left);

			/*
			 * the previous volume is out of data
			 * and is back in the rack, a new tape
			 * is loaded and ready to read.
			 *
			 * We need to pick up where we left off.
			 */
			(void) memset(&fake_tar_hdr, 0, sizeof (fake_tar_hdr));
			file_size = size_left;
			tar_hdr = &fake_tar_hdr;
			tar_hdr->th_linkflag = last_action;

			multi_volume = FALSE;
			last_action = 0;
		} else {
			tar_hdr = (tlm_tar_hdr_t *)get_read_buffer(want,
			    &erc, &actual_size, local_commands);

			if (tar_hdr == NULL) {
				rv = -1;
				continue;
			}

			/*
			 * we can ignore read errors here because
			 *   1) they are logged by Restore Reader
			 *   2) we are not doing anything important here
			 *	just looking for the next work record.
			 */
			if (actual_size < want) {
				/*
				 * EOF hits here
				 *
				 * wait for another buffer to come along
				 * or until the Reader thread tells us
				 * that no more tapes will be loaded ...
				 * time to stop.
				 */
				continue;
			}

			/*
			 * check for "we are lost"
			 */
			chk_rv = tlm_vfy_tar_checksum(tar_hdr);

			// we don't have to care about the end of tar mark. We just wait for the socket to be closed.
			if(chk_rv!=1){
				continue;
			}
			/*
			 * When files are spanned to the next tape, the
			 * information of the acls must not be over-written
			 * by the information of the LF_MULTIVOL and LF_VOLHDR
			 * header, whose information is irrelevant to the file.
			 * The information of the original header must be
			 * kept in the 'acl'.
			 */
			if (tar_hdr->th_linkflag != LF_MULTIVOL &&
					tar_hdr->th_linkflag != LF_VOLHDR) {
					if (tar_hdr->th_linkflag != LF_HUMONGUS) {
						acls->acl_attr.st_mode =
							oct_atoi(tar_hdr->th_mode);
						acls->acl_attr.st_size =
							oct_atoi(tar_hdr->th_size);
						acls->acl_attr.st_uid =
							oct_atoi(tar_hdr->th_uid);
						acls->acl_attr.st_gid =
							oct_atoi(tar_hdr->th_gid);
						acls->acl_attr.st_mtime =
							oct_atoi(tar_hdr->th_mtime);
						(void) strlcpy(acls->uname,
							tar_hdr->th_uname,
							sizeof (acls->uname));
						(void) strlcpy(acls->gname,
							tar_hdr->th_gname,
							sizeof (acls->gname));
					}
					file_size = oct_atoi(tar_hdr->th_size);
					acl_spot = 0;
					last_action = tar_hdr->th_linkflag;
				}
			}

		ndmpd_log(LOG_DEBUG, "n [%s] f [%c] s %ld m %o u %d g %d t %ld",
		    tar_hdr->th_name, tar_hdr->th_linkflag,
		    acls->acl_attr.st_size, acls->acl_attr.st_mode,
		    acls->acl_attr.st_uid, acls->acl_attr.st_gid,
		    acls->acl_attr.st_mtime);

		ndmpd_log(LOG_DEBUG, "tar_hdr->th_linkflag=%c",tar_hdr->th_linkflag);

		switch (tar_hdr->th_linkflag) {
		case LF_MULTIVOL:
			multi_volume = TRUE;
			break;
		case LF_LINK:
			is_hardlink = 1;
			hardlink_inode =
			    oct_atoi(tar_hdr->th_shared.th_hlink_ino);

			/*
			 * Check if we have restored a link with the same inode
			 * If the inode is 0, we have to restore it as a
			 * regular file.
			 */
			if (hardlink_inode) {
				hardlink_done = !hardlink_q_get(hardlink_q,
				    hardlink_inode, 0, &hardlink_target);
			}

			if (hardlink_done) {
				ndmpd_log(LOG_DEBUG,
				    "found hardlink, inode = %lu, target = [%s]",
				    hardlink_inode,
				    hardlink_target? hardlink_target : "--");

				/* create a hardlink to hardlink_target */
				file_name = (*longname == 0) ?
				    tar_hdr->th_name : longname;
				if (!is_file_wanted(file_name, sels, exls,
				    flags, &mchtype, &pos)) {
					nmp = NULL;
					/*
					 * This means that DMA did not send us
					 * the correct fh_info for the file
					 * in restore list.  We use the file
					 * name entry in sels[] (ignore the
					 * name in the tar header) as restore
					 * target.
					 */
					if (DAR) {
						nmp = rs_darhl_new_name(rnp,
						    name, sels, &pos,
						    file_name);
					}
				} else {
					nmp = rs_new_name(rnp, name, pos,
					    file_name);
					if (!nmp) {
						ndmpd_log(LOG_DEBUG,
						    "can't make name for %s",
						    longname);
					}
				}

				if (nmp) {
					if (hardlink_target) {
						erc = create_hard_link(
						    hardlink_target, nmp,
						    acls, job_stats);
						if (erc == 0) {
							(void)
							    tlm_entry_restored(
							    job_stats,
							    file_name, pos);
							ndmpd_log(LOG_DEBUG,
							    "restored %s -> %s",
							    nmp,
							    hardlink_target);
						}
					} else {
						ndmpd_log(LOG_DEBUG,
						    "no target for hardlink %s",
						    nmp);
					}

					name[0] = 0;
				}

				nm_end = 0;
				longname[0] = 0;
				lnk_end = 0;
				longlink[0] = 0;

				break;
			}
			/* otherwise fall through, restore like a normal file */
			/*FALLTHROUGH*/
		case LF_OLDNORMAL:
			/*
			 * check for TAR's end-of-tape method
			 * of zero filled records.
			 */
			if (tar_hdr->th_name[0] == 0) {
				break;
			}
			/*
			 * otherwise fall through,
			 * this is an old style normal file header
			 */
			/*FALLTHROUGH*/
		case LF_NORMAL:
		case LF_CONTIG:
			job_stats->js_files_so_far++;
			if (*hugename != 0) {
				(void) strlcpy(longname, hugename,
				    TLM_MAX_PATH_NAME);
			} else if (*longname == 0) {
				if (tar_hdr->th_name[0] != '/') {
					/*
					 * check for old tar format, it
					 * does not have a leading "/"
					 */
					longname[0] = '/';
					longname[1] = 0;
					(void) strlcat(longname,
					    tar_hdr->th_name,
					    TLM_MAX_PATH_NAME);
				} else {
					(void) strlcpy(longname,
					    tar_hdr->th_name,
					    TLM_MAX_PATH_NAME);
				}
			}

			want_this_file = is_file_wanted(longname, sels, exls,
			    flags, &mchtype, &pos);

			ndmpd_log(LOG_DEBUG, "longname = %s",longname);
			if (!want_this_file) {
				ndmpd_log(LOG_DEBUG, "do not want this file");
				nmp = NULL;
			} else {
				nmp = rs_new_name(rnp, name, pos, longname);
				if (!nmp)
					want_this_file = FALSE;
			}
			ndmpd_log(LOG_DEBUG, "nmp = %s",nmp);
			if (nmp){
				(void) strlcpy(parentlnk, nmp, sizeof(parentlnk));
			}

			/*
			 * For a hardlink, even if it's not asked to be
			 * restored, we restore it to a temporary location,
			 * in case other links to the same file need to be
			 * restored later.
			 *
			 * The temp files are created in tmplink_dir, with
			 * names like ".tmphlrsnondar*".  They are cleaned up
			 * at the completion of a restore.  However, if a
			 * restore were interrupted, e.g. by a system reboot,
			 * they would have to be cleaned up manually in order
			 * for the disk space to be freed.
			 *
			 * If tmplink_dir is NULL, no temperorary files are
			 * created during a restore.  This may result in some
			 * hardlinks not being restored during a partial
			 * restore.
			 */
			if (is_hardlink && !DAR && !want_this_file && !nmp) {
				if (tmplink_dir) {
					(void) snprintf(name, TLM_MAX_PATH_NAME,
					    "%s/%s_%d", tmplink_dir,
					    hardlink_tmp_name,
					    hardlink_tmp_idx);
					nmp = name;

					hardlink_tmp_idx++;
					hardlink_tmp_file = 1;
					want_this_file = TRUE;
					ndmpd_log(LOG_DEBUG,
					    "To restore temp hardlink file %s.",
					    nmp);
				} else {
					ndmpd_log(LOG_DEBUG,
					    "No tmplink_dir specified.");
				}
			}

			size_left = restore_file(&fp, nmp, file_size,
			    huge_size, acls, want_this_file, local_commands,
			    job_stats);

			/*
			 * In the case of non-DAR, we have to record the first
			 * link for an inode that has multiple links. That's
			 * the only link with data records actually backed up.
			 * In this way, when we run into the other links, they
			 * will be treated as links, and we won't go to look
			 * for the data records to restore.  This is not a
			 * problem for DAR, where DMA tells the tape where
			 * to locate the data records.
			 */
			if (is_hardlink && !DAR) {
				if (hardlink_q_add(hardlink_q, hardlink_inode,
				    0, nmp, hardlink_tmp_file))
					ndmpd_log(LOG_DEBUG,
					    "failed to add (%lu, %s) to HL q",
					    hardlink_inode, nmp);
			}

			/* remove / reverse the temporary stuff */
			if (hardlink_tmp_file) {
				nmp = NULL;
				want_this_file = FALSE;
			}

			/*
			 * Check if it is time to set the attribute
			 * of the restored directory
			 */
			while (nmp && ((bkpath = dtree_peek(stp)) != NULL)) {
				if (strstr(nmp, bkpath))
					break;

				(void) dtree_pop(stp);
			}

			ndmpd_log(LOG_DEBUG, "sizeleft %s %ld, %lld", longname,
			    size_left, huge_size);

			if (size_left == -TLM_STOP) {
				break_flg = TRUE;
				rv = -1;
				commands->tcs_reader = TLM_ABORT;
				ndmpd_log(LOG_DEBUG, "restoring [%s] failed",
				    longname);
				break;
			}

			if (want_this_file) {
				job_stats->js_bytes_total += file_size;
				job_stats->js_files_total++;
			}

			huge_size -= file_size;
			if (huge_size < 0) {
				huge_size = 0;
			}
			if (size_left == 0 && huge_size == 0) {
				if (PM_EXACT_OR_CHILD(mchtype)) {
					(void) tlm_entry_restored(job_stats,
					    longname, pos);

					/*
					 * Add an entry to hardlink_q to record
					 * this hardlink.
					 */
					if (is_hardlink) {
						ndmpd_log(LOG_DEBUG,
						    "Restored hardlink file %s",
						    nmp);

						if (DAR) {
							(void) hardlink_q_add(
							    hardlink_q,
							    hardlink_inode, 0,
							    nmp, 0);
						}
					}
				}

				nm_end = 0;
				longname[0] = 0;
				lnk_end = 0;
				longlink[0] = 0;
				hugename[0] = 0;
				name[0] = 0;
			}
			break;
		case LF_XATTR:
			/*
			 * we are using the NFSv4 ACL, we don't need extended attributes.
			 *
			 * For TS, we embedded the xattr in ACL since the only reason we need xattr is
			 * for ACL.
			 *
			 * */

			break;
		case LF_SYMLINK:
			file_name = (*longname == 0) ? tar_hdr->th_name :
			    longname;
			link_name = (*longlink == 0) ?
			    tar_hdr->th_linkname : longlink;
			ndmpd_log(LOG_DEBUG, "file_name[%s]", file_name);
			ndmpd_log(LOG_DEBUG, "link_name[%s]", link_name);
			if (is_file_wanted(file_name, sels, exls, flags,
			    &mchtype, &pos)) {
				nmp = rs_new_name(rnp, name, pos, file_name);
				if (nmp) {
					erc = create_sym_link(nmp, link_name,
					    acls, job_stats);
					if (erc == 0 &&
					    PM_EXACT_OR_CHILD(mchtype))
						(void) tlm_entry_restored(
						    job_stats, file_name, pos);
					name[0] = 0;
				}
			}
			nm_end = 0;
			longname[0] = 0;
			lnk_end = 0;
			longlink[0] = 0;
			break;
		case LF_DIR:
			file_name = *longname == 0 ? tar_hdr->th_name :
			    longname;
			if (is_file_wanted(file_name, sels, exls, flags,
			    &mchtype, &pos)) {
				nmp = rs_new_name(rnp, name, pos, file_name);
				if (nmp && mchtype != PM_PARENT) {

					(void) strlcpy(parentlnk, nmp, sizeof(parentlnk));

					erc = create_directory(nmp, job_stats);
					if (erc == 0 &&
					    PM_EXACT_OR_CHILD(mchtype))
						(void) tlm_entry_restored(
						    job_stats, file_name, pos);
					/*
					 * Check if it is time to set
					 * the attribute of the restored
					 * directory
					 */
					while ((bkpath = dtree_peek(stp))
					    != NULL) {
						if (strstr(nmp, bkpath))
							break;
						(void) dtree_pop(stp);
					}

					(void) dtree_push(stp, nmp, acls);
					name[0] = 0;
				}
			}
			nm_end = 0;
			longname[0] = 0;
			lnk_end = 0;
			longlink[0] = 0;
			break;
		case LF_FIFO:
			file_name = *longname == 0 ? tar_hdr->th_name :
			    longname;
			if (is_file_wanted(file_name, sels, exls, flags,
			    &mchtype, &pos)) {
				nmp = rs_new_name(rnp, name, pos, file_name);
				if (nmp) {
					erc = create_fifo(nmp, acls);
					if (erc == 0 &&
					    PM_EXACT_OR_CHILD(mchtype))
						(void) tlm_entry_restored(
						    job_stats, file_name, pos);
					name[0] = 0;
				}
			}
			nm_end = 0;
			longname[0] = 0;
			lnk_end = 0;
			longlink[0] = 0;
			break;
		case LF_LONGLINK:
			file_size = min(file_size, TLM_MAX_PATH_NAME - lnk_end);
			file_size = max(0, file_size);
			size_left = get_long_name(lib, drv, file_size, longlink,
			    &lnk_end, local_commands);

			if (size_left != 0)
				ndmpd_log(LOG_DEBUG,
				    "fsize %ld sleft %ld lnkend %ld", file_size, size_left, lnk_end);
			break;
		case LF_LONGNAME:
			file_size = min(file_size, TLM_MAX_PATH_NAME - nm_end);
			file_size = max(0, file_size);
			size_left = get_long_name(lib, drv, file_size, longname,
			    &nm_end, local_commands);

			if (size_left != 0)
				ndmpd_log(LOG_DEBUG, "fsize %ld sleft %ld nmend %ld", file_size, size_left, nm_end);
			break;
		case LF_ACL:
			size_left = load_acl_info(lib, drv, file_size, acls,
			    &acl_spot, local_commands);
			break;
		case LF_VOLHDR:
			break;
		case LF_HUMONGUS:
			(void) memset(hugename, 0, TLM_MAX_PATH_NAME);
			(void) get_humongus_file_header(lib, drv, file_size,
			    &huge_size, hugename, local_commands);
			break;
		default:
			break;

		}

	}

	/*	gracefully stopped. */
	if(local_commands->tc_writer == TLM_STOP)
		rv=0;

	/*
	 * tear down
	 */
	if (fp != 0) {
		(void) close(fp);
	}
	while (dtree_pop(stp) != -1)
		;
	cstack_delete(stp);

	free(acls);

	ndmpd_log(LOG_DEBUG, "--------tar_getdir--------");

	return (rv);
}

/*
 * Main file restore function for tar (should run as a thread)
 */
int
tar_getfile(tlm_backup_restore_arg_t *argp)
{
	ndmpd_log(LOG_DEBUG, "++++++++tar_getfile++++++++");

	tlm_job_stats_t	*job_stats;
	char	**sels;		/* list of files desired */
	char	**exls;		/* list of files not wanted */
	char	dir[TLM_MAX_PATH_NAME];		/* where to restore the files */
	char	job[TLM_MAX_BACKUP_JOB_NAME+1];
				/* the restore job name */
	int	erc;		/* error return codes */
	int	flags;
	struct	rs_name_maker rn;
	tlm_commands_t *commands;
	tlm_cmd_t *lcmd;
	char *list = NULL;

	commands = argp->ba_commands;
	lcmd = argp->ba_cmd;

	flags = 0;


	(void) strlcpy(job, argp->ba_job, TLM_MAX_BACKUP_JOB_NAME+1);
	(void) strlcpy(dir, argp->ba_dir, TLM_MAX_PATH_NAME);

	flags |= RSFLG_OVR_ALWAYS;
	flags |= RSFLG_IGNORE_CASE;

	/*
	 * do not test for "dir" having no string, since that
	 * is a legal condition.  Restore to origional location
	 * will not have a restore directory.
	 */
	if (*job == '\0') {
		ndmpd_log(LOG_DEBUG, "No job defined");
		lcmd->tc_reader = TLM_STOP;
		(void) pthread_barrier_wait(&argp->ba_barrier);
		return (-1);
	}

	sels = argp->ba_sels;
	if (sels == NULL) {
		lcmd->tc_reader = TLM_STOP;
		(void) pthread_barrier_wait(&argp->ba_barrier);
		return (-1);
	}
	exls = &list;

	tlm_log_list("selections", sels);
	tlm_log_list("exclusions", exls);

	if (wildcard_enabled())
		flags |= RSFLG_MATCH_WCARD;

	lcmd->tc_ref++;
	commands->tcs_writer_count++;

	/*
	 * let the launcher continue
	 */
	(void) pthread_barrier_wait(&argp->ba_barrier);

	job_stats = tlm_ref_job_stats(job);

	rn.rn_fp = catnames;
	rn.rn_nlp = dir;

	/*
	 * work
	 */
	ndmpd_log(LOG_DEBUG, "start restore job %s", job);
	erc = tar_getdir(commands, lcmd, job_stats, &rn, 1, 1,
	    sels, exls, flags, 0, NULL);


	/*
	 * teardown
	 */

	ndmpd_log(LOG_DEBUG, "end restore job %s", job);
	tlm_un_ref_job_stats(job);
	tlm_release_list(sels);
	tlm_release_list(exls);

	commands->tcs_writer_count--;
	lcmd->tc_reader = TLM_STOP;
	tlm_release_reader_writer_ipc(lcmd);

	ndmpd_log(LOG_DEBUG, "--------tar_getfile--------");

	return (erc);
}

/*
 * Creates the directories all the way down to the
 * end if they dont exist
 */
int
make_dirs(char *dir)
{
	char c;
	char *cp, *end;
	struct stat st;

	cp = dir;
	cp += strspn(cp, "/");
	end = dir + strlen(dir);
	do {
		if (*cp == '\0' || *cp == '/') {
			c = *cp;
			*cp = '\0';
			if (lstat(dir, &st) < 0)
				if (mkdir(dir, 0777) < 0) {
					ndmpd_log(LOG_DEBUG, "Error %d"
					    " creating directory %s",
					    errno, dir);
					*cp = c;
					return (-1);
				}

			*cp = c;
		}
	} while (++cp <= end);

	return (0);
}

/*
 * Creates the directories leading to the given path
 */
int
mkbasedir(char *path)
{
	int rv;
	char *cp;
	struct stat st;

	if (!path || !*path) {
		ndmpd_log(LOG_DEBUG, "Invalid argument");
		return (-1);
	}

	cp = strrchr(path, '/');
	if (cp)
		*cp = '\0';
	rv = lstat(path, &st);
	if (rv < 0)	/* need new directories */
		rv = make_dirs(path);
	if (cp)
		*cp = '/';

	return (rv);
}


/*
 * read the file off the tape back onto disk
 */
static long
restore_file(int *fp,
    char *real_name,
    long size,
    longlong_t huge_size,
    tlm_acls_t *acls,
    bool_t want_this_file,
    tlm_cmd_t *local_commands,
    tlm_job_stats_t *job_stats)
{

	ndmpd_log(LOG_DEBUG, "restore_file");
	struct stat	attr;

	if (!real_name) {
		if (want_this_file) {
			ndmpd_log(LOG_DEBUG, "No file name but wanted!");
			want_this_file = FALSE;
		}
	} else
		ndmpd_log(LOG_DEBUG, "new file[%s]", real_name);

	/*
	 * OK, some FM is creeping in here ...
	 * int *fp is used to keep the
	 * backup file channel open through
	 * the interruption of EOT and
	 * processing the headers of the
	 * next tape.  So, if *fp is zero
	 * then no file is open yet and all
	 * is normal.  If *fp has a number
	 * then we are returning after an
	 * EOT break.
	 *
	 * *fp is now also open for HUGE files
	 * that are put back in sections.
	 */

	if (*fp == 0 && want_this_file) {
		int	erc_stat;

		if (mkbasedir(real_name) < 0)
			job_stats->js_errors++;

		erc_stat = stat(real_name, (struct stat *)&attr);
		if (erc_stat < 0) {
			ndmpd_log(LOG_DEBUG, "erc_stat < 0");
			/*EMPTY*/
			/* new file */
		} else if (acls->acl_overwrite) {
			ndmpd_log(LOG_DEBUG, "acls->acl_overwrite");
			/*EMPTY*/
			/* take this file no matter what */
		} else if (acls->acl_update) {
			if (attr.st_mtime < acls->acl_attr.st_mtime) {
				ndmpd_log(LOG_DEBUG, "acls->acl_update attr.st_mtime < acls->acl_attr.st_mtime ");
				/*EMPTY*/
				/* tape is newer */
			} else {
				ndmpd_log(LOG_DEBUG, "acls->acl_update else");
				/* disk file is newer */
				want_this_file = FALSE;
			}
		} else {
			ndmpd_log(LOG_DEBUG, "erc_stat < 0 else");
			/*
			 * no overwrite, no update,
			 * do not ever replace old files.
			 */
			want_this_file = TRUE;
		}
		ndmpd_log(LOG_DEBUG, "want_this_file:%s yes/no:%d",real_name,want_this_file);
		if (want_this_file) {
			ndmpd_log(LOG_DEBUG, "creating:%s",real_name);

			*fp = open(real_name, O_CREAT | O_WRONLY,
			    S_IRUSR | S_IWUSR);
			if (*fp == -1) {
				ndmpd_log(LOG_ERR,
				    "Could not open %s for restore.",
				    real_name);
				ndmpd_log(LOG_DEBUG,
				    "fp=%d err=%d ", *fp, errno);
				job_stats->js_errors++;
				want_this_file = FALSE;
				/*
				 * we cannot return here,
				 * the file is still on
				 * the tape and must be
				 * skipped over.
				 */
			}
		}
		(void) strlcpy(local_commands->tc_file_name, real_name,
		    TLM_MAX_PATH_NAME);
	}

	/*
	 * this is the size left in the next segment
	 */
	huge_size -= size;

	/*
	 * work
	 */
	int	actual_size;
	int	error;
	char	*rec;
	int	write_size;

	while (size > 0 && local_commands->tc_writer == TLM_RESTORE_RUN) {

		(void)pthread_yield();
		/*
		 * Use bytes_in_file field to tell reader the amount
		 * of data still need to be read for this file.
		 */
		job_stats->js_bytes_in_file = size;

		error = 0;
		rec = get_read_buffer(size, &error, &actual_size,
		    local_commands);
		if (actual_size <= 0) {
			ndmpd_log(LOG_DEBUG,
			    "RESTORE WRITER> error %d, actual_size %d",
			    error, actual_size);

			/* no more data for this file for now */
			job_stats->js_bytes_in_file = 0;

			setReadBufDone(local_commands->tc_buffers);

			return (size);
		} else if (error) {
			ndmpd_log(LOG_DEBUG, "Error %d in file [%s]",
			    error, local_commands->tc_file_name);
			break;
		} else {
			write_size = min(size, actual_size);
			if (want_this_file) {
				write_size = write(*fp, rec, write_size);
			}

			size -= write_size;
		}
	}
	setReadBufDone(local_commands->tc_buffers);

	/* no more data for this file for now */
	job_stats->js_bytes_in_file = 0;



	/*
	 * teardown
	 */
	if (*fp != 0 && huge_size <= 0) {
		(void) close(*fp);
		*fp = 0;
		set_acl(real_name, acls);
	}
	return (0);
}




/*
 * Match the name with the list
 */
static int
exact_find(char *name, char **list)
{
	bool_t found;
	int i;
	char *cp;

	found = FALSE;
	for (i = 0; *list != NULL; list++, i++) {
		cp = *list + strspn(*list, "/");
		if (match(cp, name)) {
			found = TRUE;
			ndmpd_log(LOG_DEBUG, "exact_find> found[%s]", cp);
			break;
		}
	}

	return (found);
}

/*
 * On error, return FALSE and prevent restoring(probably) unwanted data.
 */
static int
is_parent(char *parent, char *child, int flags)
{
	char tmp[TLM_MAX_PATH_NAME];
	bool_t rv;

	if (IS_SET(flags, RSFLG_MATCH_WCARD)) {
		if (!tlm_cat_path(tmp, parent, "*")) {
			ndmpd_log(LOG_DEBUG,
			    "is_parent> path too long [%s]", parent);
			rv = FALSE;
		} else
			rv = (match(tmp, child) != 0) ? TRUE : FALSE;
	} else {
		if (!tlm_cat_path(tmp, parent, "/")) {
			ndmpd_log(LOG_DEBUG,
			    "is_parent> path too long [%s]", parent);
			rv = FALSE;
		} else
			rv = (strncmp(tmp, child, strlen(tmp)) == 0) ?
			    TRUE : FALSE;
	}

	return (rv);
}

/*
 * Used to match the filename inside the list
 */
static bool_t
strexactcmp(char *s, char *t)
{
	return ((strcmp(s, t) == 0) ? TRUE : FALSE);
}

/*
 * Check if the file is needed to be restored
 */
static bool_t
is_file_wanted(char *name,
    char **sels,
    char **exls,
    int flags,
    int *mchtype,
    int *pos)
{
	char *p_sel;
	static char uc_name[TLM_MAX_PATH_NAME];
	static char retry[TLM_MAX_PATH_NAME];
	char *namep;
	bool_t found;
	int i;
	name_match_fp_t *cmp_fp;

	if (name == NULL || sels == NULL || exls == NULL)
		return (FALSE);

	found = FALSE;
	if (mchtype != NULL)
		*mchtype = PM_NONE;
	if (pos != NULL)
		*pos = 0;

	/*
	 * For empty selection, restore everything
	 */
	if (*sels == NULL || **sels == '\0') {
		ndmpd_log(LOG_DEBUG, "is_file_wanted: Restore all");
		return (TRUE);
	}


	if (IS_SET(flags, RSFLG_MATCH_WCARD))
		cmp_fp = match;
	else
		cmp_fp = strexactcmp;

	namep = name + strspn(name, "/");
	if (IS_SET(flags, RSFLG_IGNORE_CASE)) {
		(void) strlcpy(uc_name, namep, TLM_MAX_PATH_NAME);
		(void) strupr(uc_name);
		namep = uc_name;
	}
	ndmpd_log(LOG_DEBUG, "is_file_wanted> flg: 0x%x name: [%s]",
	    flags, name);

	for (i = 0; *sels != NULL; sels++, i++) {
		p_sel = *sels + strspn(*sels, "/");

		/*
		 * Try exact match.
		 */
		if ((*cmp_fp)(p_sel, namep)) {
			ndmpd_log(LOG_DEBUG, "match1> pos: %d [%s][%s]",
			    i, p_sel, name);
			found = TRUE;
			if (mchtype != NULL)
				*mchtype = PM_EXACT;
			break;
		}
		/*
		 * Try "entry/" and the current selection.  The
		 * current selection may be something like "<something>/".
		 */
		(void) tlm_cat_path(retry, namep, "/");
		if ((*cmp_fp)(p_sel, retry)) {
			ndmpd_log(LOG_DEBUG, "match2> pos %d [%s][%s]",
			    i, p_sel, name);
			found = TRUE;
			if (mchtype != NULL)
				*mchtype = PM_EXACT;
			break;
		}
		/*
		 * If the following check returns true it means that the
		 * 'name' is an entry below the 'p_sel' hierarchy.
		 */
		if (is_parent(p_sel, namep, flags)) {
			ndmpd_log(LOG_DEBUG, "parent1> pos %d [%s][%s]",
			    i, p_sel, name);
			found = TRUE;
			if (mchtype != NULL)
				*mchtype = PM_CHILD;
			break;
		}

		/*
		 * There is a special case for parent directories of a
		 * selection.  If 'p_sel' is something like "*d1", the
		 * middle directories of the final entry can't be determined
		 * until the final entry matches with 'p_sel'.  At that
		 * time the middle directories of the entry have been passed
		 * and they can't be restored.
		 */
		if (is_parent(namep, p_sel, flags)) {
			ndmpd_log(LOG_DEBUG, "parent2> pos %d [%s][%s]",
			    i, p_sel, name);
			found = TRUE;
			if (mchtype != NULL)
				*mchtype = PM_PARENT;
			break;
		}
	}

	/* Check for exclusions.  */
	if (found && exact_find(namep, exls)) {
		if (mchtype != NULL)
			*mchtype = PM_NONE;
		found = FALSE;
	}
	if (found && pos != NULL)
		*pos = i;

	return (found);
}

/*
 * Read the specified amount data into the buffer.  Detects EOT or EOF
 * during read.
 *
 * Returns the number of bytes actually read.  On error returns -1.
 */
static int
input_mem(int l, int d, tlm_cmd_t *lcmds, char *mem, int len)
{
	int err = 0;
	int toread, actual_size, rec_size;
	char *rec;

	if (l <= 0 || d <= 0 || !lcmds || !mem) {
		ndmpd_log(LOG_DEBUG, "Invalid argument");
		return (-1);
	}

	toread = len;

	while (toread > 0) {
		rec = get_read_buffer(toread, &err, &actual_size, lcmds);
		if (actual_size <= 0) {
			ndmpd_log(LOG_DEBUG, "err %d act_size %d detected",
			    err, actual_size);
			break;
		} else if (err) {

			ndmpd_log(LOG_DEBUG, "error %d reading data", err);

			setReadBufDone(lcmds->tc_buffers);
			return (-1);
		}
		rec_size = min(actual_size, toread);
		(void) memcpy(mem, rec, rec_size);
		mem += rec_size;
		toread -= rec_size;
	}

	setReadBufDone(lcmds->tc_buffers);

	return (len - toread);
}

/*
 * pick up the name and size of a HUGE file
 */
static	int
get_humongus_file_header(int lib,
    int	drv,
    long recsize,
    longlong_t *size,
    char *name,
    tlm_cmd_t *local_commands)
{
	ndmpd_log(LOG_DEBUG, "get_humongus_file_header");

	char *p_record, *value;
	int rv;

	ndmpd_log(LOG_DEBUG, "HUGE Record found: %ld", recsize);

	rv = 0;
	if (recsize == 0) {
		/*
		 * The humongus_file_header was written in a
		 * RECORDSIZE block and the header.size field of this
		 * record was 0 before this fix.  For backward compatiblity
		 * read only one RECORDSIZE-size block if the header.size
		 * field is 0.  Otherwise the header.size field should show
		 * the length of the data of this header.
		 */
		ndmpd_log(LOG_DEBUG, "Old HUGE record found");
		recsize = RECORDSIZE;
	}

	if (input_mem(lib, drv, local_commands, name, recsize) != recsize) {
		rv = -1;
		*size = 0;
		*name = '\0';
		ndmpd_log(LOG_DEBUG, "Error reading a HUGE file name");
	} else {
		ndmpd_log(LOG_DEBUG, "HUGE [%s]", name);

		p_record = name;
		value = parse(&p_record, " ");
		*size = atoll(value);
		/*
		 * Note: Since the backed up names are not longer than
		 * NAME_MAX and the buffer passed to us is
		 * TLM_MAX_PATH_NAME, it should be safe to use strlcpy
		 * without check on the buffer size.
		 */
		(void) strlcpy(name, p_record, TLM_MAX_PATH_NAME);
	}

	ndmpd_log(LOG_DEBUG, "HUGE Record %lld [%s]", *size, name);

	return (rv);
}

/*
 * pick up the long name from the special tape file
 */
static int
get_long_name(int lib,
    int drv,
    long recsize,
    char *name,
    long *buf_spot,
    tlm_cmd_t *local_commands)
{
	int nread;

	ndmpd_log(LOG_DEBUG, "LONGNAME Record found rs %ld bs %ld", recsize,
	    *buf_spot);

	if (*buf_spot < 0)
		*buf_spot = 0;

	nread = input_mem(lib, drv, local_commands, name + *buf_spot,
	    recsize);
	if (nread < 0) {
		nread = recsize; /* return 0 as size left */
		name[*buf_spot] = '\0';
		ndmpd_log(LOG_ERR, "Error %d reading a long file name %s.",
		    nread, name);
	} else {
		*buf_spot += nread;
		name[*buf_spot] = '\0';
		ndmpd_log(LOG_DEBUG, "LONGNAME [%s]", name);
	}

	return (recsize - nread);
}

/*
 * create a new directory
 */
static	int
create_directory(char *dir, tlm_job_stats_t *job_stats)
{
	struct stat attr;
	char	*p;
	char	temp;
	int	erc;

	/*
	 * Make sure all directories in this path exist, create them if
	 * needed.
	 */
	ndmpd_log(LOG_DEBUG, "new dir[%s]", dir);

	erc = 0;
	p = &dir[1];
	do {
		temp = *p;
		if (temp == '/' || temp == 0) {
			*p = 0;
			if (stat(dir, &attr) < 0) {
				erc = mkdir(dir, 0777);
				if (erc < 0) {
					job_stats->js_errors++;
					ndmpd_log(LOG_DEBUG,
					    "Could not create directory %s",
					    dir);
					break;
				}
			}
			*p = temp;
		}
		p++;
	} while (temp != 0);

	return (erc);
}

/*
 * create a new hardlink
 */
static int
create_hard_link(char *name_old, char *name_new,
    tlm_acls_t *acls, tlm_job_stats_t *job_stats)
{
	int erc;

	if (mkbasedir(name_new)) {
		ndmpd_log(LOG_DEBUG, "faile to make base dir for [%s]",
		    name_new);

		return (-1);
	}

	erc = link(name_old, name_new);
	if (erc) {
		job_stats->js_errors++;
		ndmpd_log(LOG_DEBUG, "error %d (errno %d) hardlink [%s] to [%s]",
		    erc, errno, name_new, name_old);
	} else {
		set_acl(name_new, acls);
	}
	return (erc);
}

/*
 * create a new symlink
 */
/*ARGSUSED*/
static int
create_sym_link(char *dst, char *target, tlm_acls_t *acls,
    tlm_job_stats_t *job_satats)
{
	int erc;

	if (mkbasedir(dst) < 0)
		return (-1);

	erc = symlink(target, dst);
	if (erc) {
		job_satats->js_errors++;
		ndmpd_log(LOG_DEBUG, "error %d (errno %d) softlink [%s] to [%s]",
		    erc, errno, dst, target);
	} else {
		set_acl(dst, acls);
	}

	return (erc);
}

/*
 * create a new FIFO
 */
static int
create_fifo(char *name, tlm_acls_t *acls)
{
	(void) mknod(name, 0777 + S_IFIFO, 0);
	set_acl(name, acls);
	return (0);
}

/*
 * read in the ACLs for the file in next entry.
 */
static long
load_acl_info(int lib,
    int drv,
    long file_size,
    tlm_acls_t *acls,
    long *acl_spot,
    tlm_cmd_t *local_commands)
{
	char *bp;
	int nread;
	ndmpd_log(LOG_DEBUG, "load_acl_info");
	/*
	 * If the ACL is spanned on tapes, then the acl_spot should NOT be
	 * 0 on next calls to this function to read the rest of the ACL
	 * on next tapes.
	 */
	if (*acl_spot == 0) {
		(void) memset(acls, 0, sizeof (tlm_acls_t));
	}


	bp = ((char *)&acls->acl_info) + *acl_spot;
	nread = input_mem(lib, drv, local_commands, (void *)bp, file_size);
	if (nread < 0) {
		*acl_spot = 0;
		(void) memset(acls, 0, sizeof (tlm_acls_t));
		ndmpd_log(LOG_DEBUG, "Error reading ACL data");
		return (0);
	}


	int acl_all_len = 0;
	int acl_len = 0;
	int xattr_len = 0;


	acl_len = acls->acl_info.attr_len;
	ndmpd_log(LOG_DEBUG, "ACL : acl_len=%d",acl_len);

//#ifdef QNAP_TS
//	// handle the ACL in xattr.
//	xattr_len = acls->acl_info.xattr_len;
//	ndmpd_log(LOG_DEBUG, "ACL : xattr_len=%d",xattr_len);
//
//#endif

	acl_all_len = acl_len+xattr_len;

	// we can not use dynamic allocate here, it will overwrite some address that we still need.
	char tmp_acl[acl_all_len];

	ndmpd_log(LOG_DEBUG, "ACL :offset=%ld all len=%d file_size=%ld acl_info size=%ld",sizeof(acls->acl_info), acl_all_len, file_size, sizeof(acls->acl_info));

	memcpy(tmp_acl,(char *)&acls->acl_info+sizeof(acls->acl_info),acl_all_len);

	acls->acl_info.attr_info = (char*)calloc(acl_all_len, sizeof(char));

	memcpy(acls->acl_info.attr_info, tmp_acl,acl_all_len);

	*acl_spot += nread;
	acls->acl_non_trivial = TRUE;

	return (file_size - nread);
}



/*
 * Set the standard attributes of the file
 */
static void
set_attr(char *name, tlm_acls_t *acls)
{
	ndmpd_log(LOG_DEBUG, "set_attr");
	struct utimbuf tbuf;
	bool_t priv_all = FALSE;
	struct stat *st;
	uid_t uid;
	gid_t gid;
	struct passwd *pwd;
	struct group *grp;


	if (!name || !acls)
		return;


	st = &acls->acl_attr;
	ndmpd_log(LOG_DEBUG, "set_attr: %s uid %d gid %d uname %s gname %s "
	    "mode %o", name, st->st_uid, st->st_gid, acls->uname, acls->gname,
	    st->st_mode);


	uid = st->st_uid;
	if ((pwd = getpwnam(acls->uname)) != NULL) {
		ndmpd_log(LOG_DEBUG, "set_attr: new uid %d old %d",
		    pwd->pw_uid, uid);
		uid = pwd->pw_uid;
	}



	gid = st->st_gid;
	if ((grp = getgrnam(acls->gname)) != NULL) {
		ndmpd_log(LOG_DEBUG, "set_attr: new gid %d old %d",
		    grp->gr_gid, gid);
		gid = grp->gr_gid;
	}

	if (lchown(name, uid, gid)){
		ndmpd_log(LOG_ERR,
		    "Could not set uid or/and gid for file %s.", name);

	}

	if (chmod(name, st->st_mode)){
		ndmpd_log(LOG_ERR,
		    "Could not set correct file permission for file %s.", name);

	}

	tbuf.modtime = st->st_mtime;
	tbuf.actime = st->st_atime;
	(void) utime(name, &tbuf);
}

/*
 * Set the ACL info for the file
 */
static void
set_acl(char *name, tlm_acls_t *acls)
{
	ndmpd_log(LOG_DEBUG, "set_acl");
	int erc;

	acl_t acl = NULL;

	if (name)
		ndmpd_log(LOG_DEBUG, "set_acl: %s", name);

	if (acls != 0) {

		set_attr(name, acls);

		ndmpd_log(LOG_DEBUG, "set_acl: acls->acl_non_trivial =%d",acls->acl_non_trivial);

		if (!acls->acl_non_trivial) {
			(void) memset(acls, 0, sizeof (tlm_acls_t));
			ndmpd_log(LOG_DEBUG, "set_acl: skipping trivial");
			return;
		}

		char *acl_txt = acls->acl_info.attr_info;
		if(acl_txt==NULL)
			return ; // ACL not support in this volume.

		ndmpd_log(LOG_DEBUG, "set_acl with text:\n%s\n",acl_txt);

		acl = acl_from_text(acl_txt);

		if (acl!=NULL) {
#ifdef QNAP_TS
			erc = acl_set_file(name, ACL_TYPE_ACCESS, acl);
#else
			erc = acl_set_file(name, ACL_TYPE_NFS4, acl);
#endif
			if (erc < 0) {
				ndmpd_log(LOG_DEBUG, "RESTORE> acl_set errno %d!!!", errno);
				// if the volume does not support ACL, this will fail.
			}
			acl_free(acl);


		}else{
			ndmpd_log(LOG_DEBUG,"RESTORE> acl_from_text error on file:%s",name);
			fprintf(stderr, "RESTORE> acl_from_text error on file:%s",name);
		}

//#ifdef QNAP_TS
//		char *aclname="security.NTACL";
//		if(acls->acl_info.xattr_len>0){
//			char *acl_xattr=acls->acl_info.attr_info+acls->acl_info.attr_len;
//			setxattr(name, aclname, acl_xattr, acls->acl_info.xattr_len, 0);
//		}
//#endif

		free(acls->acl_info.attr_info);

		(void) memset(acls, 0, sizeof (tlm_acls_t));

	}
}

/*
 * a wrapper to tlm_get_read_buffer so that
 * we can cleanly detect ABORT commands
 * without involving the TLM library with
 * our problems.
 */
static char *
get_read_buffer(int want,
    int	*error,
    int	*actual_size,
    tlm_cmd_t *local_commands)
{

	setReadBufDone(local_commands->tc_buffers);

	char	*rec;
	while (local_commands->tc_writer == TLM_RESTORE_RUN) {

		rec = tlm_get_read_buffer(want, error,
		    local_commands->tc_buffers, actual_size);
		if (rec != 0) {
			return (rec);
		}
	}


	setReadBufDone(local_commands->tc_buffers);
	/*
	 * the job is ending, give Writer a buffer that will never be read ...
	 * it does not matter anyhow, we are aborting.
	 */
	*actual_size = RECORDSIZE;
	return (NULL);
}

/*
 * Enable wildcard for restore options
 */
static bool_t
wildcard_enabled(void)
{
	char *cp;

	cp = ndmpd_get_prop_default(NDMP_RESTORE_WILDCARD_ENABLE, "n");
	return ((toupper(*cp) == 'Y') ? TRUE : FALSE);
}


/*
 * Concatenate two names
 */
/*ARGSUSED*/
static char *
catnames(const struct rs_name_maker *rnp, char *buf, int pos, char *path)
{
	char *rv;
	ndmpd_log(LOG_DEBUG, "catnames");
	rv = NULL;
	if (!buf) {
		ndmpd_log(LOG_DEBUG, "buf is NULL");
	} else if (!path) {
		ndmpd_log(LOG_DEBUG, "path is NULL");
	} else if (!rnp->rn_nlp) {
		ndmpd_log(LOG_DEBUG, "rn_nlp is NULL [%s]", path);
	} else if (!tlm_cat_path(buf, rnp->rn_nlp, path)) {
		ndmpd_log(LOG_DEBUG, "Path too long [%s][%s]",
		    rnp->rn_nlp, path);
	} else
		rv = buf;

	return (rv);
}


/*
 * Create a new name path for restore
 */
static char *
rs_new_name(const struct rs_name_maker *rnp, char *buf, int pos, char *path)
{
	if (!rnp || !rnp->rn_fp){
		assert(0);
		return (NULL);
	}

	return (*rnp->rn_fp)(rnp, buf, pos, path);
}
