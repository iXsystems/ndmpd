#include <sys/errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>


#include <tlm.h>
#include <tlm_buffers.h>
#include <tlm_util.h>

#include <sys/mtio.h>
#include <pthread.h>



#include <ndmpd_func.h>

longlong_t llmin(longlong_t, longlong_t);
unsigned int min(unsigned int, unsigned int);
unsigned int max(unsigned int, unsigned int);
int oct_atoi(char *p);

int tlm_log_fhnode(tlm_job_stats_t *,
    char *,
    char *,
    struct stat *,
    u_longlong_t);

int tlm_log_fhdir(tlm_job_stats_t *,
    char *,
    struct stat *,
    struct fs_fhandle *);

int tlm_log_fhpath_name(tlm_job_stats_t *,
    char *,
    struct stat *,
    u_longlong_t);



void tlm_log_list(char *,
    char **);

int tlm_ioctl(int, int, void *);

void tlm_build_header_checksum(tlm_tar_hdr_t *);
int tlm_vfy_tar_checksum(tlm_tar_hdr_t *);
int tlm_entry_restored(tlm_job_stats_t *, char *, int);

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


/*
 * Tar archiving ops vector
 */
tm_ops_t tm_tar_ops = {
	"tar",
	tar_putfile,
	tar_putdir,
	NULL,
	tar_getfile,
	tar_getdir,
	NULL
};


/*
 * get the next tape buffer from the drive's pool of buffers
 */
/*ARGSUSED*/
char *
tlm_get_write_buffer(long want, long *actual_size,
    tlm_buffers_t *buffers, int zero)
{


	int	buf = buffers->tbs_buffer_in;


	tlm_buffer_t *buffer = &buffers->tbs_buffer[buf];
	int	align_size = RECORDSIZE - 1;
	char	*rec;



	/*
	 * make sure the allocation is in chunks of 512 bytes
	 */
	want += align_size;
	want &= ~align_size;

	*actual_size = buffer->tb_buffer_size - buffer->tb_buffer_spot;
	// if there are still space, just give it out

	if (*actual_size <= 0) {
		/*
		 * no room, send this one
		 * and wait for a free one
		 */
		if (!buffer->tb_full) {

			/*
			 * we are now ready to send a full buffer
			 * instead of trying to get a new buffer
			 *
			 * do not send if we failed to get a buffer
			 * on the previous call
			 */
			buffer->tb_full = TRUE;

			/*
			 * tell the writer that a buffer is available
			 */
			tlm_buffer_release_in_buf(buffers);

			buffer = tlm_buffer_advance_in_idx(buffers);

		}

		buffer = tlm_buffer_in_buf(buffers, NULL);

		if (buffer->tb_full) {
			/*
			 * wait for the writer to free up a buffer
			 */
			tlm_buffer_out_buf_timed_wait(buffers, 500);
		}

		buffer = tlm_buffer_in_buf(buffers, NULL);
		if (buffer->tb_full) {
			/*
			 * the next buffer is still full
			 * of data from previous activity
			 *
			 * nothing has changed.
			 */
			return (0);
		}

		buffer->tb_buffer_spot = 0;
		*actual_size = buffer->tb_buffer_size - buffer->tb_buffer_spot;
	}

	*actual_size = min(want, *actual_size);
	rec = &buffer->tb_buffer_data[buffer->tb_buffer_spot];

	buffer->tb_buffer_spot += *actual_size;
	buffers->tbs_offset += *actual_size;


	if (zero) {
		(void) memset(rec, 0, *actual_size);
	}
	return (rec);
}

/*
 * get a read record from the tape buffer,
 * and read a tape block if necessary
 */
/*ARGSUSED*/
char *
tlm_get_read_buffer(int want, int *error,
    tlm_buffers_t *buffers, int *actual_size)
{
	tlm_buffer_t *buffer;
	int	align_size = RECORDSIZE - 1;
	int	buf;
	int	current_size;
	char	*rec;

	buf = buffers->tbs_buffer_out;
	buffer = &buffers->tbs_buffer[buf];

	/*
	 * make sure the allocation is in chunks of 512 bytes
	 */
	want += align_size;
	want &= ~align_size;

	current_size = buffer->tb_buffer_size - buffer->tb_buffer_spot;
	if (buffer->tb_full && current_size <= 0) {
		/*
		 * no more data, release this
		 * one and go get another
		 */

		/*
		 * tell the reader that a buffer is available
		 */
		buffer->tb_full = FALSE;
		tlm_buffer_release_out_buf(buffers);

		buffer = tlm_buffer_advance_out_idx(buffers);
		current_size = buffer->tb_buffer_size - buffer->tb_buffer_spot;
	}

	if (!buffer->tb_full) {
		/*
		 * next buffer is not full yet.
		 * wait for the reader.
		 */
		tlm_buffer_in_buf_timed_wait(buffers, 500);

		buffer = tlm_buffer_out_buf(buffers, NULL);
		if (!buffer->tb_full) {
			/*
			 * we do not have anything from the tape yet
			 */
			return (0);
		}

		current_size = buffer->tb_buffer_size - buffer->tb_buffer_spot;
	}

	/* Make sure we got something */
	if (current_size <= 0)
		return (0);

	current_size = min(want, current_size);
	rec = &buffer->tb_buffer_data[buffer->tb_buffer_spot];
	buffer->tb_buffer_spot += current_size;
	*actual_size = current_size;

	/*
	 * the error flag is only sent back one time,
	 * since the flag refers to a previous read
	 * attempt, not the data in this buffer.
	 */
	*error = buffer->tb_errno;

	return (rec);
}


/*
 * unread a previously read buffer back to the tape buffer
 */
void
tlm_unget_read_buffer(tlm_buffers_t *buffers, int size)
{
	tlm_buffer_t *buffer;
	int	align_size = RECORDSIZE - 1;
	int	buf;
	int	current_size;

	buf = buffers->tbs_buffer_out;
	buffer = &buffers->tbs_buffer[buf];

	/*
	 * make sure the allocation is in chunks of 512 bytes
	 */
	size += align_size;
	size &= ~align_size;

	current_size = min(size, buffer->tb_buffer_spot);
	buffer->tb_buffer_spot -= current_size;
}


/*
 * unwrite a previously written buffer
 */
void
tlm_unget_write_buffer(tlm_buffers_t *buffers, int size)
{
	tlm_buffer_t *buffer;
	int	align_size = RECORDSIZE - 1;
	int	buf;
	int	current_size;

	buf = buffers->tbs_buffer_in;
	buffer = &buffers->tbs_buffer[buf];

	/*
	 * make sure the allocation is in chunks of 512 bytes
	 */
	size += align_size;
	size &= ~align_size;

	current_size = min(size, buffer->tb_buffer_spot);
	buffer->tb_buffer_spot -= current_size;
}


/*
 * build a checksum for a TAR header record
 */
void
tlm_build_header_checksum(tlm_tar_hdr_t *r)
{
	int	i;
	unsigned int	sum = 0;
	char *c = (char *)r;

	(void) memcpy(r->th_chksum, CHKBLANKS, strlen(CHKBLANKS));
	for (i = 0; i < RECORDSIZE; i++) {
		sum += c[i] & 0xFF;
	}
	(void) snprintf(r->th_chksum, sizeof (r->th_chksum), "%6o", sum);
}

/*
 * verify the tar header checksum
 */
int
tlm_vfy_tar_checksum(tlm_tar_hdr_t *tar_hdr)
{
	unsigned int	chksum = oct_atoi(tar_hdr->th_chksum);
	u_char	*p = (u_char *)tar_hdr;
	unsigned int	sum = 0;	/* initial value of checksum */
	int	i;		/* loop counter */

	/*
	 * compute the checksum
	 */
	for (i = 0; i < RECORDSIZE; i++) {
		sum += p[i] & 0xFF;
	}

	if (sum == 0) {
		return (0);
	}

	/*
	 * subtract out the label's checksum values
	 * this lets us undo the old checksum "in-
	 * place", no need to swap blanks in and out
	 */
	for (i = 0; i < 8; i++) {
		sum -= 0xFF & tar_hdr->th_chksum[i];
	}

	/*
	 * replace the old checksum field with blanks
	 */
	sum += ' ' * 8;

	if (sum != chksum)
		ndmpd_log(LOG_DEBUG,
		    "should be %d, is %d", chksum, sum);

	return ((sum == chksum) ? 1 : -1);
}


/*
 * create the IPC area between the reader and writer
 */
tlm_cmd_t *
tlm_create_reader_writer_ipc(bool_t write, long data_transfer_size)
{
	tlm_cmd_t *cmd;
	ndmpd_log(LOG_DEBUG, "tlm_create_reader_writer_ipc");
	cmd = ndmp_malloc(sizeof (tlm_cmd_t));
	if (cmd == NULL)
		return (NULL);

	cmd->tc_reader = TLM_BACKUP_RUN;
	cmd->tc_writer = TLM_BACKUP_RUN;
	cmd->tc_ref = 1;

	cmd->tc_buffers = tlm_allocate_buffers(write, data_transfer_size);
	if (cmd->tc_buffers == NULL) {
		free(cmd);
		return (NULL);
	}

	(void) mutex_init(&cmd->tc_mtx, 0, NULL);
	(void) cond_init(&cmd->tc_cv, 0, NULL);

	return (cmd);
}

/*
 * release(destroy) the IPC between the reader and writer
 */
void
tlm_release_reader_writer_ipc(tlm_cmd_t *cmd)
{

	if (--cmd->tc_ref <= 0) {
		(void) mutex_lock(&cmd->tc_mtx);
		tlm_release_buffers(cmd->tc_buffers);
		(void) cond_destroy(&cmd->tc_cv);
		(void) mutex_unlock(&cmd->tc_mtx);
		(void) mutex_destroy(&cmd->tc_mtx);
		free(cmd);
	}
}


/*
 * NDMP support begins here.
 */

/*
 * Initialize the file history callback functions
 */
lbr_fhlog_call_backs_t *
lbrlog_callbacks_init(void *cookie, path_hist_func_t log_pname_func,
    dir_hist_func_t log_dir_func, node_hist_func_t log_node_func)
{
	lbr_fhlog_call_backs_t *p;

	p = ndmp_malloc(sizeof (lbr_fhlog_call_backs_t));
	if (p == NULL)
		return (NULL);

	p->fh_cookie = cookie;
	p->fh_logpname = (func_t)log_pname_func;
	p->fh_log_dir = (func_t)log_dir_func;
	p->fh_log_node = (func_t)log_node_func;
	return (p);
}

/*
 * Cleanup the callbacks
 */
void
lbrlog_callbacks_done(lbr_fhlog_call_backs_t *p)
{
	if (p != NULL)
		(void) free((char *)p);
}

/*
 * Call back for file history directory info
 */
int
tlm_log_fhdir(tlm_job_stats_t *job_stats, char *dir, struct stat *stp,
    fs_fhandle_t *fhp)
{
	ndmpd_log(LOG_DEBUG, "tlm_log_fhdir");
	int rv;
	lbr_fhlog_call_backs_t *cbp; /* callbacks pointer */

	rv = 0;
	if (job_stats == NULL) {
		ndmpd_log(LOG_DEBUG, "log_fhdir: jstat is NULL");
	} else if (dir == NULL) {
		ndmpd_log(LOG_DEBUG, "log_fhdir: dir is NULL");
	} else if (stp == NULL) {
		ndmpd_log(LOG_DEBUG, "log_fhdir: stp is NULL");
	} else if ((cbp = (lbr_fhlog_call_backs_t *)job_stats->js_callbacks)
	    == NULL) {
		ndmpd_log(LOG_DEBUG, "log_fhdir: cbp is NULL");
	} else if (cbp->fh_log_dir == NULL) {
		ndmpd_log(LOG_DEBUG, "log_fhdir: callback is NULL");
	} else
		rv = (*cbp->fh_log_dir)(cbp, dir, stp, fhp);

	return (rv);
}

/*
 * Call back for file history node info
 */
int
tlm_log_fhnode(tlm_job_stats_t *job_stats, char *dir, char *file,
    struct stat *stp, u_longlong_t off)
{
	ndmpd_log(LOG_DEBUG, "tlm_log_fhnode");
	int rv;
	lbr_fhlog_call_backs_t *cbp; /* callbacks pointer */

	rv = 0;
	if (job_stats == NULL) {
		ndmpd_log(LOG_DEBUG, "log_fhnode: jstat is NULL");
	} else
		if (dir == NULL) {
		ndmpd_log(LOG_DEBUG, "log_fhnode: dir is NULL");
	} else if (file == NULL) {
		ndmpd_log(LOG_DEBUG, "log_fhnode: file is NULL");
	} else if (stp == NULL) {
		ndmpd_log(LOG_DEBUG, "log_fhnode: stp is NULL");
	} else if ((cbp = (lbr_fhlog_call_backs_t *)job_stats->js_callbacks)
	    == NULL) {
		ndmpd_log(LOG_DEBUG, "log_fhnode: cbp is NULL");
	} else if (cbp->fh_log_node == NULL) {
		ndmpd_log(LOG_DEBUG, "log_fhnode: callback is NULL");
	} else
		rv = (*cbp->fh_log_node)(cbp, dir, file, stp, off);

	return (rv);
}

/*
 * Call back for file history path info
 */
int
tlm_log_fhpath_name(tlm_job_stats_t *job_stats, char *pathname,
    struct stat *stp, u_longlong_t off)
{
	ndmpd_log(LOG_DEBUG, "tlm_log_fhpath_name");
	int rv;
	lbr_fhlog_call_backs_t *cbp; /* callbacks pointer */

	rv = 0;
	if (!job_stats) {
		ndmpd_log(LOG_DEBUG, "log_fhpath_name: jstat is NULL");
	} else if (!pathname) {
		ndmpd_log(LOG_DEBUG, "log_fhpath_name: pathname is NULL");
	} else if (!stp) {
		ndmpd_log(LOG_DEBUG, "log_fhpath_name: stp is NULL");
	} else if ((cbp = (lbr_fhlog_call_backs_t *)job_stats->js_callbacks)
	    == 0) {
		ndmpd_log(LOG_DEBUG, "log_fhpath_name: cbp is NULL");
	} else if (!cbp->fh_logpname) {
		ndmpd_log(LOG_DEBUG, "log_fhpath_name: callback is NULL");
	} else
		rv = (*cbp->fh_logpname)(cbp, pathname, stp, off);

	return (rv);
}


/*
 * Log call back to report the entry recovery
 */
int
tlm_entry_restored(tlm_job_stats_t *job_stats, char *name, int pos)
{
	ndmpd_log(LOG_DEBUG, "tlm_entry_restored");
	lbr_fhlog_call_backs_t *cbp; /* callbacks pointer */

	ndmpd_log(LOG_DEBUG, "name: \"%s\", pos: %d", name, pos);

	if (job_stats == NULL) {
		ndmpd_log(LOG_DEBUG, "entry_restored: jstat is NULL");
		return (0);
	}
	cbp = (lbr_fhlog_call_backs_t *)job_stats->js_callbacks;
	if (cbp == NULL) {
		ndmpd_log(LOG_DEBUG, "entry_restored is NULL");
		return (0);
	}
	return (*cbp->fh_logpname)(cbp, name, 0, (longlong_t)pos);
}
/*
 * NDMP support ends here.
 */

/*
 * Function: tlm_cat_path
 * Concatenates two path names
 * or directory name and file name
 * into a buffer passed by the caller. A slash
 * is inserted if required. Buffer is assumed
 * to hold PATH_MAX characters.
 *
 * Parameters:
 *	char *buf	- buffer to write new dir/name string
 *	char *dir	- directory name
 *	char *name	- file name
 *
 * Returns:
 *	TRUE		- No errors. buf contains the dir/name string
 *	FALSE		- Error. buf is not modified.
 */
bool_t
tlm_cat_path(char *buf, char *dir, char *name)
{
	char *fmt;
	int dirlen = strlen(dir);
	int filelen = strlen(name);
	ndmpd_log(LOG_DEBUG,"tlm_cat_path dir=%s name=%s",dir,name );
	if ((dirlen + filelen + 1) >= PATH_MAX) {
		return (FALSE);
	}

	if (*dir == '\0' || *name == '\0' || dir[dirlen - 1] == '/' ||
	    *name == '/') {
		fmt = "%s%s";
	} else {
		fmt = "%s/%s";
	}

	/* check for ".../" and "/...." */
	if ((dirlen > 0) && (dir[dirlen - 1] == '/') && (*name == '/'))
		name += strspn(name, "/");

	/* LINTED variable format */
	(void) snprintf(buf, TLM_MAX_PATH_NAME, fmt, dir, name);

	return (TRUE);
}



/*
 * Release an array of pointers and the pointers themselves.
 */
void
tlm_release_list(char **lpp)
{
	char **save;


	if ((save = lpp) == 0)
		return;

	while (*lpp)
		free(*lpp++);

	free(save);
}

/*
 * Print the list of array of strings in the backup log
 */
void
tlm_log_list(char *title, char **lpp)
{
	int i;

	if (!lpp)
		return;

	ndmpd_log(LOG_DEBUG, "%s:", title);

	for (i = 0; *lpp; lpp++, i++)
		ndmpd_log(LOG_DEBUG, "%d: [%s]", i, *lpp);
}



/*
 * see if we should exclude this file.
 */
bool_t
tlm_is_excluded(char *dir, char *name, char **excles)
{
	int	i;
	int lastCh=0;

	if (!dir || !name || !excles)
		return (FALSE);

	for (i = 0; excles[i] != 0; i++) {
		ndmpd_log(LOG_DEBUG, "compare dir...\n",dir, excles[i]+2);
		if(strncmp(excles[i],"d_",2)==0){
			ndmpd_log(LOG_DEBUG, "compare dir=%s with pattern=%s\n",dir, excles[i]+2);
			if (match(excles[i]+2, dir)){
				ndmpd_log(LOG_DEBUG, "directory matched, skip this folder");
				return (TRUE);
			}
		}else{
			ndmpd_log(LOG_DEBUG, "comparing file...\n");
			ndmpd_log(LOG_DEBUG, "compare file=%s with pattern=%s\n",name, excles[i]+2);
			if (match(excles[i]+2, name)){
				ndmpd_log(LOG_DEBUG, "file matched, skip this file");
				return (TRUE);
			}
		}
	}
	ndmpd_log(LOG_DEBUG, "no match\n");
	return (FALSE);
}



/*
 * Get the data offset of inside the buffer
 */
longlong_t
tlm_get_data_offset(tlm_cmd_t *lcmds)
{
	if (!lcmds)
		return (0LL);

	return (lcmds->tc_buffers->tbs_offset);
}

/*
 * Enable the barcode capability on the library
 */
void
tlm_enable_barcode(int l)
{
	ndmpd_log(LOG_DEBUG, "tlm_enable_barcode");
}


/*
 * IOCTL wrapper with retries
 */
int
tlm_ioctl(int fd, int cmd, void *data)
{
	int retries = 0;

	ndmpd_log(LOG_DEBUG, "tlm_ioctl fd %d cmd %d", fd, cmd);
	if (fd == 0 || data == NULL)
		return (EINVAL);

	do {
		if (ioctl(fd, cmd, data) == 0)
			break;

		if (errno != EIO && errno != 0) {
			ndmpd_log(LOG_ERR,
			    "Failed to send command to device: %m.");
			ndmpd_log(LOG_DEBUG, "IOCTL error %d", errno);
			return (errno);
		}
		(void) sleep(1);
	} while (retries++ < MAXIORETRY);

	return (0);
}

/*
 * Min/max functions
 */
unsigned
min(a, b)
	unsigned a, b;
{
	return (a < b ? a : b);
}

unsigned
max(a, b)
	unsigned a, b;
{
	return (a > b ? a : b);
}

longlong_t
llmin(longlong_t a, longlong_t b)
{
	return (a < b ? a : b);
}
