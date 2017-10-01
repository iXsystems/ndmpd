/*
 * Copyright 2009 Sun Microsystems, Inc.  
 * Copyright 2017 Marcelo Araujo <araujo@FreeBSD.org>.
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

#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>

#include <tlm.h>
#include <tlm_buffers.h>
#include <tlm_lib.h>

#include <ndmpd_tar_v3.h>

#include <ndmpd_func.h>
#include <ndmpd_snapshot.h>

#include <pwd.h>
#include <grp.h>

#include <tlm_util.h>

#include "tlm_proto.h"


static char *get_write_buffer(long size,
    long *actual_size,
    bool_t zero,
    tlm_cmd_t *);
static int output_acl_header(sec_attr_t *,
    tlm_cmd_t *);
static int output_file_header(char *name,
    char *link,
    tlm_acls_t *,
    int section,
    tlm_cmd_t *);


/*
 * output_mem
 *
 * Gets a IO write buffer and copies memory to the that.
 */
static void
output_mem(tlm_cmd_t *local_commands, char *mem,
    int len)
{
	long actual_size, rec_size;
	char *rec;

	while (len > 0) {

		rec = get_write_buffer(len, &actual_size,
		    FALSE, local_commands);
		rec_size = min(actual_size, len);
		(void) memcpy(rec, mem, rec_size);
		// the buffer is filled.
		setWriteBufDone(local_commands->tc_buffers);

		mem += rec_size;
		len -= rec_size;
	}
}

/*
 * tlm_output_dir
 *
 * Put the directory information into the output buffers.
 */
int
tlm_output_dir(char *name, tlm_acls_t *tlm_acls,
    tlm_cmd_t *local_commands, tlm_job_stats_t *job_stats)
{
	u_longlong_t pos;

	/*
	 * Send the node or path history of the directory itself.
	 */
	pos = tlm_get_data_offset(local_commands);

	(void) tlm_log_fhnode(job_stats, name, "", &tlm_acls->acl_attr, pos);
	(void) tlm_log_fhpath_name(job_stats, name, &tlm_acls->acl_attr, pos);
	/* fhdir_cb is handled in ndmpd_tar3.c */

	(void) output_acl_header(&tlm_acls->acl_info,
	    local_commands);
	(void) output_file_header(name, "", tlm_acls, 0,
	    local_commands);

	return (0);
}

/*
 * tar_putdir
 *
 * Main dir backup function for tar
 */
int
tar_putdir(char *name, tlm_acls_t *tlm_acls,
    tlm_cmd_t *local_commands, tlm_job_stats_t *job_stats)
{
	int rv;

	rv = tlm_output_dir(name, tlm_acls, local_commands, job_stats);
	return (rv < 0 ? rv : 0);
}

/*
 * output_acl_header
 *
 * output the ACL header record and data
 */
static int
output_acl_header(sec_attr_t *acl_info,
    tlm_cmd_t *local_commands)
{

	long	actual_size;
	tlm_tar_hdr_t *tar_hdr;
	long	acl_size;

	if ((acl_info == NULL) ||
			// following should never happen, we always support ACL.(Just in case.)
			(acl_info->attr_info == NULL) ||
			(*acl_info->attr_info == '\0'))
		return (0);

	tar_hdr = (tlm_tar_hdr_t *)get_write_buffer(RECORDSIZE,
	    &actual_size, TRUE, local_commands);
	if (!tar_hdr)
		return (0);

	tar_hdr->th_linkflag = LF_ACL;
	acl_info->attr_type = UFSD_ACL;


	acl_size = sizeof (*acl_info)+acl_info->attr_len;

	(void) strlcpy(tar_hdr->th_name, "UFSACL", TLM_NAME_SIZE);
	(void) snprintf(tar_hdr->th_size, sizeof (tar_hdr->th_size), "%011lo ", acl_size);
	(void) snprintf(tar_hdr->th_mode, sizeof (tar_hdr->th_mode), "%06o ", 0444);
	(void) snprintf(tar_hdr->th_uid, sizeof (tar_hdr->th_uid), "%06o ", 0);
	(void) snprintf(tar_hdr->th_gid, sizeof (tar_hdr->th_gid), "%06o ", 0);
	(void) snprintf(tar_hdr->th_mtime, sizeof (tar_hdr->th_mtime), "%011o ", 0);
	(void) strlcpy(tar_hdr->th_magic, TLM_MAGIC, sizeof (tar_hdr->th_magic));

	tlm_build_header_checksum(tar_hdr);

	// header output done.
	setWriteBufDone(local_commands->tc_buffers);

	char *tmpbuf = (char*)malloc(acl_size);

	memcpy(tmpbuf,acl_info,sizeof (*acl_info));
	memcpy(tmpbuf+sizeof (*acl_info),acl_info->attr_info,acl_info->attr_len);

	(void) output_mem(local_commands, (void *)tmpbuf, acl_size);

	free(tmpbuf);
	free(acl_info->attr_info);


	return (0);
}

/*
 * output_humongus_header
 *
 * output a special header record for HUGE files
 * output is:	1) a TAR "HUGE" header redord
 * 		2) a "file" of size, name
 */
static int
output_humongus_header(char *fullname, longlong_t file_size,
    tlm_cmd_t *local_commands)
{
	char	*buf;
	int	len;
	long	actual_size;
	tlm_tar_hdr_t *tar_hdr;

	/*
	 * buf will contain: "%llu %s":
	 * - 20 is the maximum length of 'u_longlong' decimal notation.
	 * - The first '1' is for the ' ' between the "%llu" and the fullname.
	 * - The last '1' is for the null-terminator of fullname.
	 */
	len = 20 + 1 + strlen(fullname) + 1;

	if ((buf = ndmp_malloc(sizeof (char) * len)) == NULL)
		return (-1);



	tar_hdr = (tlm_tar_hdr_t *)get_write_buffer(RECORDSIZE,
	    &actual_size, TRUE, local_commands);
	if (!tar_hdr) {
		free(buf);
		return (0);
	}

	tar_hdr->th_linkflag = LF_HUMONGUS;
	(void) snprintf(tar_hdr->th_size, sizeof (tar_hdr->th_size), "%011o ",
	    len);
	tlm_build_header_checksum(tar_hdr);

	// header output done.
	setWriteBufDone(local_commands->tc_buffers);


	(void) snprintf(buf, len, "%lld %s", file_size, fullname);
	(void) output_mem(local_commands, buf, len);

	free(buf);
	return (0);
}






// FIXME: this is referenced in kernel mode.
#define		GID_NOBODY	65534
#define		UID_NOBODY	65534

/*
 * output_file_header
 *
 * output the TAR header record
 */
static int
output_file_header(char *name, char *link,
    tlm_acls_t *tlm_acls, int section, tlm_cmd_t *local_commands)
{
	static	longlong_t file_count = 0;
	struct stat *attr = &tlm_acls->acl_attr;
	tlm_tar_hdr_t *tar_hdr;
	long	actual_size;
	bool_t long_name = FALSE;
	bool_t long_link = FALSE;
	char	section_name[TLM_MAX_PATH_NAME];
	int	nmlen, lnklen;
	uid_t uid;
	gid_t gid;
	char *uname = "";
	char *gname = "";
	struct passwd *pwd;
	struct group *grp;


	/*
	 * if the file has to go out in sections,
	 * we must mung the name.
	 */
	if (section == 0) {
		(void) strlcpy(section_name, name, TLM_MAX_PATH_NAME);
	} else {
		(void) snprintf(section_name,
		    TLM_MAX_PATH_NAME, "%s.%03d", name, section);
	}

	if ((pwd = getpwuid(attr->st_uid)) != NULL)
		uname = pwd->pw_name;
	if ((grp = getgrgid(attr->st_gid)) != NULL)
		gname = grp->gr_name;

	if ((u_long)(uid = attr->st_uid) > (u_long)OCTAL7CHAR)
		uid = UID_NOBODY;
	if ((u_long)(gid = attr->st_gid) > (u_long)OCTAL7CHAR)
		gid = GID_NOBODY;


	if(strcmp(uname,"")==0)
		uid=UID_NOBODY;
	if(strcmp(gname,"")==0)
		gid=GID_NOBODY;


	nmlen = strlen(section_name);
	if (nmlen >= NAMSIZ) {
		/*
		 * file name is too big, it must go out
		 * in its own data file
		 */

		tar_hdr = (tlm_tar_hdr_t *)get_write_buffer(RECORDSIZE,
		    &actual_size, TRUE, local_commands);
		if (!tar_hdr) {
			return (0);
		}
		(void) snprintf(tar_hdr->th_name,
		    sizeof (tar_hdr->th_name),
		    "%s%08qd.fil",
		    LONGNAME_PREFIX,
		    file_count++);

		tar_hdr->th_linkflag = LF_LONGNAME;
		(void) snprintf(tar_hdr->th_size, sizeof (tar_hdr->th_size),
		    "%011o ", nmlen);
		(void) snprintf(tar_hdr->th_mode, sizeof (tar_hdr->th_mode),
		    "%06o ", attr->st_mode & 07777);
		(void) snprintf(tar_hdr->th_uid, sizeof (tar_hdr->th_uid),
		    "%06o ", uid);
		(void) snprintf(tar_hdr->th_gid, sizeof (tar_hdr->th_gid),
		    "%06o ", gid);
		(void) snprintf(tar_hdr->th_uname, sizeof (tar_hdr->th_uname),
		    "%.31s", uname);
		(void) snprintf(tar_hdr->th_gname, sizeof (tar_hdr->th_gname),
		    "%.31s", gname);
		(void) snprintf(tar_hdr->th_mtime, sizeof (tar_hdr->th_mtime),
		    "%011lo ", attr->st_mtime);
		(void) strlcpy(tar_hdr->th_magic, TLM_MAGIC,
		    sizeof (tar_hdr->th_magic));

		tlm_build_header_checksum(tar_hdr);
		// header output done.
		setWriteBufDone(local_commands->tc_buffers);

		(void) output_mem(local_commands,
		    (void *)section_name, nmlen);
		long_name = TRUE;
	}

	lnklen = strlen(link);
	if (lnklen >= NAMSIZ) {
		/*
		 * link name is too big, it must go out
		 * in its own data file
		 */

		tar_hdr = (tlm_tar_hdr_t *)get_write_buffer(RECORDSIZE,
		    &actual_size, TRUE, local_commands);
		if (!tar_hdr) {

			return (0);
		}
		(void) snprintf(tar_hdr->th_linkname,
		    sizeof (tar_hdr->th_name),
		    "%s%08qd.slk",
		    LONGNAME_PREFIX,
		    file_count++);

		tar_hdr->th_linkflag = LF_LONGLINK;
		(void) snprintf(tar_hdr->th_size, sizeof (tar_hdr->th_size),
		    "%011o ", lnklen);
		(void) snprintf(tar_hdr->th_mode, sizeof (tar_hdr->th_mode),
		    "%06o ", attr->st_mode & 07777);
		(void) snprintf(tar_hdr->th_uid, sizeof (tar_hdr->th_uid),
		    "%06o ", uid);
		(void) snprintf(tar_hdr->th_gid, sizeof (tar_hdr->th_gid),
		    "%06o ", gid);
		(void) snprintf(tar_hdr->th_uname, sizeof (tar_hdr->th_uname),
		    "%.31s", uname);
		(void) snprintf(tar_hdr->th_gname, sizeof (tar_hdr->th_gname),
		    "%.31s", gname);
		(void) snprintf(tar_hdr->th_mtime, sizeof (tar_hdr->th_mtime),
		    "%011lo ", attr->st_mtime);
		(void) strlcpy(tar_hdr->th_magic, TLM_MAGIC,
		    sizeof (tar_hdr->th_magic));

		tlm_build_header_checksum(tar_hdr);

		// header output done.
		setWriteBufDone(local_commands->tc_buffers);

		(void) output_mem(local_commands, (void *)link,
		    lnklen);
		long_link = TRUE;
	}

	tar_hdr = (tlm_tar_hdr_t *)get_write_buffer(RECORDSIZE,
	    &actual_size, TRUE, local_commands);
	if (!tar_hdr) {
		return (0);
	}
	if (long_name) {
		(void) snprintf(tar_hdr->th_name,
		    sizeof (tar_hdr->th_name),
		    "%s%08qd.fil",
		    LONGNAME_PREFIX,
		    file_count++);
	} else {
		(void) strlcpy(tar_hdr->th_name, section_name, TLM_NAME_SIZE);
	}

	ndmpd_log(LOG_DEBUG, "long_link: %s [%s]", long_link ? "TRUE" : "FALSE",
	    link);

	if (long_link) {
		(void) snprintf(tar_hdr->th_linkname,
		    sizeof (tar_hdr->th_name),
		    "%s%08qd.slk",
		    LONGNAME_PREFIX,
		    file_count++);
	} else {
		(void) strlcpy(tar_hdr->th_linkname, link, TLM_NAME_SIZE);
	}
	if (S_ISDIR(attr->st_mode)) {
		tar_hdr->th_linkflag = LF_DIR;
	} else if (S_ISFIFO(attr->st_mode)) {
		tar_hdr->th_linkflag = LF_FIFO;
	} else if (attr->st_nlink > 1) {
		/* mark file with hardlink LF_LINK */
		tar_hdr->th_linkflag = LF_LINK;
		(void) snprintf(tar_hdr->th_shared.th_hlink_ino,
		    sizeof (tar_hdr->th_shared.th_hlink_ino),
		    "%011o ", attr->st_ino);
	} else {
		tar_hdr->th_linkflag = *link == 0 ? LF_NORMAL : LF_SYMLINK;
		ndmpd_log(LOG_DEBUG, "linkflag: '%c'", tar_hdr->th_linkflag);
	}
	(void) snprintf(tar_hdr->th_size, sizeof (tar_hdr->th_size), "%011lo ",
	    (long)attr->st_size);
	(void) snprintf(tar_hdr->th_mode, sizeof (tar_hdr->th_mode), "%06o ",
	    attr->st_mode & 07777);
	(void) snprintf(tar_hdr->th_uid, sizeof (tar_hdr->th_uid), "%06o ",
	    uid);
	(void) snprintf(tar_hdr->th_gid, sizeof (tar_hdr->th_gid), "%06o ",
	    gid);
	(void) snprintf(tar_hdr->th_uname, sizeof (tar_hdr->th_uname), "%.31s",
	    uname);
	(void) snprintf(tar_hdr->th_gname, sizeof (tar_hdr->th_gname), "%.31s",
	    gname);
	(void) snprintf(tar_hdr->th_mtime, sizeof (tar_hdr->th_mtime), "%011lo ",
	    attr->st_mtime);
	(void) strlcpy(tar_hdr->th_magic, TLM_MAGIC,
	    sizeof (tar_hdr->th_magic));

	tlm_build_header_checksum(tar_hdr);

	// header output done.
	setWriteBufDone(local_commands->tc_buffers);


	if (long_name || long_link) {
		if (file_count > 99999990) {
			file_count = 0;
		}
	}
	return (0);
}


/*
 * tlm_readlink
 *
 * Read where the softlink points to.  Read the link in the checkpointed
 * path if the backup is being done on a checkpointed file system.
 */
static int
tlm_readlink(char *nm, char *snap, char *buf, int bufsize)
{
	int len;

	if ((len = readlink(snap, buf, bufsize)) >= 0) {
		/*
		 * realink(2) doesn't null terminate the link name.  We must
		 * do it here.
		 */
		buf[len] = '\0';
	} else {
		ndmpd_log(LOG_DEBUG, "Error %d reading softlink of [%s]",
		    errno, nm);
		buf[0] = '\0';

		/* Backup the link if the destination missing */
		if (errno == ENOENT)
			return (0);

	}

	return (len);
}

#include <assert.h>
/*
 * tlm_output_file
 *
 * Put this file into the output buffers.
 */
longlong_t
tlm_output_file(char *dir, char *name, char *chkdir,
    tlm_acls_t *tlm_acls, tlm_commands_t *commands, tlm_cmd_t *local_commands,
    tlm_job_stats_t *job_stats, struct hardlink_q *hardlink_q)
{

	ndmpd_log(LOG_DEBUG, "tlm_output_file********** output the data");

	char	fullname[TLM_MAX_PATH_NAME];		/* directory + name */
	char	snapname[TLM_MAX_PATH_NAME];		/* snapshot name */
	char	linkname[TLM_MAX_PATH_NAME];		/* where this file points */
	int	section = 0;		/* section of a huge file */
	int	fd;
	longlong_t real_size;		/* the origional file size */
	longlong_t file_size;		/* real size of this file */
	longlong_t seek_spot = 0;	/* location in the file */
					/* for Multi Volume record */
	u_longlong_t pos;
	char *fnamep;
	int ii=0;
	/* Indicate whether a file with the same inode has been backed up. */
	int hardlink_done = 0;

	/*
	 * If a file with the same inode has been backed up, hardlink_pos holds
	 * the tape offset of the data record.
	 */
	u_longlong_t hardlink_pos = 0;

	if (tlm_is_too_long(tlm_acls->acl_checkpointed, dir, name)) {
		ndmpd_log(LOG_DEBUG, "Path too long [%s][%s]", dir, name);
		return (-TLM_NO_SCRATCH_SPACE);
	}
	ndmpd_log(LOG_DEBUG, "tlm_output_file 0");

	if (!tlm_cat_path(fullname, dir, name) ||
	    !tlm_cat_path(snapname, chkdir, name)) {
		ndmpd_log(LOG_DEBUG, "Path too long.");
		real_size = -TLM_NO_SCRATCH_SPACE;
		goto err_out;
	}

	pos = tlm_get_data_offset(local_commands);


	if (S_ISLNK(tlm_acls->acl_attr.st_mode)) {
		file_size = tlm_readlink(fullname, snapname, linkname,
		    TLM_MAX_PATH_NAME-1);
		if (file_size < 0) {
			real_size = -ENOENT;
			goto err_out;
		}

		/*
		 * Since soft links can not be read(2), we should only
		 * backup the file header.
		 */
		(void) output_file_header(fullname,
		    linkname,
		    tlm_acls,
		    section,
		    local_commands);

		(void) tlm_log_fhnode(job_stats, dir, name,
		    &tlm_acls->acl_attr, pos);
		(void) tlm_log_fhpath_name(job_stats, fullname,
		    &tlm_acls->acl_attr, pos);

		return (0);
	}

//	fnamep = (tlm_acls->acl_checkpointed) ? snapname : fullname;
	fnamep = fullname;
	ndmpd_log(LOG_DEBUG, "fnamep=%s",fnamep);
	/*
	 * For hardlink, only read the data if no other link
	 * belonging to the same inode has been backed up.
	 */
	if (tlm_acls->acl_attr.st_nlink > 1) {
		hardlink_done = !hardlink_q_get(hardlink_q,
		    tlm_acls->acl_attr.st_ino, &hardlink_pos, NULL);
	}



	if (!hardlink_done) {
		fd = open(fnamep, O_RDONLY);
		if (fd == -1) {
			ndmpd_log(LOG_DEBUG,
			    "BACKUP> Can't open file [%s][%s] err(%d)",
			    fullname, fnamep, errno);
			real_size = -TLM_NO_SOURCE_FILE;
			goto err_out;
		}
	} else {
		ndmpd_log(LOG_DEBUG, "found hardlink, inode = %u, pos = %llu ",
		    tlm_acls->acl_attr.st_ino, hardlink_pos);

		fd = -1;
	}

	if (fd == -1)
		ndmpd_log(LOG_DEBUG,"BACKUP> Can't open file [%s][%s] err()",fullname, fnamep);

	linkname[0] = 0;

	real_size = tlm_acls->acl_attr.st_size;


	(void) output_acl_header(&tlm_acls->acl_info,
	    local_commands);

	/*
	 * section = 0: file is small enough for TAR
	 * section > 0: file goes out in TLM_MAX_TAR_IMAGE sized chunks
	 * 		and the file name gets munged
	 */
	file_size = real_size;
	if (file_size > TLM_MAX_TAR_IMAGE) {
		if (output_humongus_header(fullname, file_size,
		    local_commands) < 0) {
			(void) close(fd);
			real_size = -TLM_NO_SCRATCH_SPACE;
			goto err_out;
		}
		section = 1;
	} else {
		section = 0;
	}

	/*
	 * For hardlink, if other link belonging to the same inode
	 * has been backed up, only backup an empty record.
	 */

	if (hardlink_done)
		file_size = 0;

	/*
	 * work
	 */
	if (file_size == 0) {
		(void) output_file_header(fullname,
		    linkname,
		    tlm_acls,
		    section,
		    local_commands);
		/*
		 * this can fall right through since zero size files
		 * will be skipped by the WHILE loop anyway
		 */

	}

	int ccnt=0;
	while (file_size > 0) {

		longlong_t section_size = llmin(file_size,(longlong_t)TLM_MAX_TAR_IMAGE);


		tlm_acls->acl_attr.st_size = section_size;

		(void) output_file_header(fullname,
		    linkname,
		    tlm_acls,
		    section,
		    local_commands);

		while (section_size > 0) {
			char	*buf;
			long	actual_size;
			long	read_size;

			/*
			 * check for Abort commands
			 */
			if (commands->tcs_reader != TLM_BACKUP_RUN) {
				local_commands->tc_writer = TLM_ABORT;
				goto tear_down;
			}

			local_commands->tc_buffers->tbs_buffer[local_commands->tc_buffers->tbs_buffer_in].tb_file_size = section_size;
			local_commands->tc_buffers->tbs_buffer[local_commands->tc_buffers->tbs_buffer_in].tb_seek_spot = seek_spot;

			buf = get_write_buffer(section_size,
			    &actual_size, FALSE, local_commands);
			if (!buf){

				goto tear_down;
				assert(0);
			}

			/*
			 * check for Abort commands
			 */
			if (commands->tcs_reader != TLM_BACKUP_RUN) {
				local_commands->tc_writer = TLM_ABORT;
				goto tear_down;
			}


			read_size = min(section_size, actual_size);
			actual_size = read(fd, buf, read_size);

			if (actual_size == 0)
				break;

			if (actual_size == -1) {
				ndmpd_log(LOG_DEBUG, "problem(%d) reading file [%s][%s]", errno, fullname, snapname);
				goto tear_down;
				assert(0);
			}

			// read the data to output buffer.
			setWriteBufDone(local_commands->tc_buffers);

			seek_spot += actual_size;
			file_size -= actual_size;
			section_size -= actual_size;
		}
		section++;
	}
	// set the st_size to the actually one.
	tlm_acls->acl_attr.st_size = seek_spot;
	/*
	 * If data belonging to this hardlink has been backed up, add the link
	 * to hardlink queue.
	 */
	if (tlm_acls->acl_attr.st_nlink > 1 && !hardlink_done) {
		(void) hardlink_q_add(hardlink_q, tlm_acls->acl_attr.st_ino,
		    pos, NULL, 0);
		ndmpd_log(LOG_DEBUG,
		    "backed up hardlink file %s, inode = %u, pos = %llu ",
		    fullname, tlm_acls->acl_attr.st_ino, pos);
	}


	/*
	 * For hardlink, if other link belonging to the same inode has been
	 * backed up, no add_node entry should be sent for this link.
	 */
	if (hardlink_done) {
		ndmpd_log(LOG_DEBUG,
		    "backed up hardlink link %s, inode = %u, pos = %llu ",
		    fullname, tlm_acls->acl_attr.st_ino, hardlink_pos);
	} else {
		(void) tlm_log_fhnode(job_stats, dir, name,
		    &tlm_acls->acl_attr, pos);
	}


	(void) tlm_log_fhpath_name(job_stats, fullname, &tlm_acls->acl_attr,
	    pos);

tear_down:

	// flush the output
	setWriteBufDone(local_commands->tc_buffers);

	/*
	 * tell writer to abort.
	 * this is used to handle when the backup target host is disconnect.
	*/

	local_commands->tc_buffers->tbs_buffer[
	    local_commands->tc_buffers->tbs_buffer_in].tb_seek_spot = 0;

	(void) close(fd);

err_out:

	return (real_size);
}

/*
 * tar_putfile
 *
 * Main file backup function for tar
 */
int
tar_putfile(char *dir, char *name, char *chkdir,
    tlm_acls_t *tlm_acls, tlm_commands_t *commands,
    tlm_cmd_t *local_commands, tlm_job_stats_t *job_stats,
    struct hardlink_q *hardlink_q)
{
	int rv;

	rv = tlm_output_file(dir, name, chkdir, tlm_acls, commands,
	    local_commands, job_stats, hardlink_q);
	if (rv < 0)
		return (rv);


	return (rv < 0 ? rv : 0);
}

/*
 * get_write_buffer
 *
 * a wrapper to tlm_get_write_buffer so that
 * we can cleanly detect ABORT commands
 * without involving the TLM library with
 * our problems.
 */
static char *
get_write_buffer(long size, long *actual_size,
    bool_t zero, tlm_cmd_t *local_commands)
{

	// before get write buffer. need to flush the write buffer
	setWriteBufDone(local_commands->tc_buffers);
	while (local_commands->tc_reader == TLM_BACKUP_RUN) {

		char *rec = tlm_get_write_buffer(size, actual_size,
		    local_commands->tc_buffers, zero);
		if (rec != 0) {
			return (rec);
		}
	}
	// flush the write buffer.
	setWriteBufDone(local_commands->tc_buffers);

	return (NULL);
}

#define	NDMP_MORE_RECORDS	2

/*
 * write_tar_eof
 *
 * This function is initially written for NDMP support.  It appends
 * two tar headers to the tar file, and also N more empty buffers
 * to make sure that the two tar headers will be read as a part of
 * a mover record and don't get locked because of EOM on the mover
 * side.
 */
void
write_tar_eof(tlm_cmd_t *local_commands)
{
	int i;
	long actual_size;
	tlm_buffers_t *bufs;

	/*
	 * output 2 zero filled records,
	 * TAR wants this.
	 */


	/*
	 * NDMP: Clear the rest of the buffer and write two more buffers
	 * to the tape.
	 */
	bufs = local_commands->tc_buffers;



	bufs->tbs_buffer[bufs->tbs_buffer_in].tb_write_buf_filled = TRUE;
	bufs->tbs_buffer[bufs->tbs_buffer_in].tb_full = TRUE;


	tlm_buffer_release_in_buf(bufs);
}
