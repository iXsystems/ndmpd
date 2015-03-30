
#ifndef	_TLM_H_
#define	_TLM_H_

#include <rpc/types.h>

#include <limits.h>

//#include <sys/acl.h>


#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/queue.h>

#define	IS_SET(f, m)	(((f) & (m)) != 0)

#define	TLM_MAX_BACKUP_JOB_NAME	32	/* max size of a job's name */
//#define	TLM_TAPE_BUFFERS	10	/* number of rotating buffers */
#define	TLM_TAPE_BUFFERS	1	/* number of rotating buffers */
#define	TLM_LINE_SIZE		128	/* size of text messages */


#define	TLM_BACKUP_RUN		0x00000001
#define	TLM_RESTORE_RUN		0x00000002
#define	TLM_STOP		0x00000009	/* graceful stop, means successfully done. */
#define	TLM_ABORT		0x99999999	/* abandon the run */

#define	TLM_EXTRA_SPACE		64
#define	TLM_MAX_PATH_NAME	(PATH_MAX + TLM_EXTRA_SPACE)

#define	ENTRYTYPELEN	14
#define	PERMS		4
#define	ID_STR_MAX	20
#define	APPENDED_ID_MAX	(ID_STR_MAX + 1)
#define	ACL_ENTRY_SIZE	(ENTRYTYPELEN + ID_STR_MAX + PERMS + APPENDED_ID_MAX)

/*	this is defined in sys/acl.h	*/
#define	MAX_ACL_ENTRIES		(1024)	/* max entries of each type */
#define	TLM_MAX_ACL_TXT	MAX_ACL_ENTRIES * ACL_ENTRY_SIZE


#ifdef QNAP_TS
	#define XATTR_SIZE_MAX 65536    /* size of an extended attribute value (64k) */
#endif

/*	max tlm_ioctl retry count*/
#define	MAXIORETRY	20


/* operation flags */
#define	TLM_OP_CHOOSE_ARCHIVE	0x00000001	/* look for archive bit */

#define	OCTAL7CHAR	07777777

typedef	int (*func_t)();

typedef struct fs_fhandle {
	int fh_fid;
	char *fh_fpath;
} fs_fhandle_t;


#define	DEFAULT_SLINK_MAX_XFER	(64*1024)

typedef struct	tlm_info {
	int			ti_init_done;	/* initialization done ? */
	int			ti_library_count; /* number of libraries */
	struct tlm_library	*ti_library;	/* first in chain */
	struct tlm_chain_link	*ti_job_stats;  /* chain of job statistics */
} tlm_info_t;

typedef struct	tlm_chain_link {
	struct tlm_chain_link	*tc_next;	/* next blob of statistics */
	struct tlm_chain_link	*tc_prev;	/* previous blob in the chain */
	int	tc_ref_count;			/* number of routines */
	void	*tc_data;			/* the data blob */
} tlm_chain_link_t;

#define	TLM_NO_ERRORS			0x00000000
#define	TLM_ERROR_BUSY			0x00000001
#define	TLM_ERROR_INTERNAL		0x00000002
#define	TLM_ERROR_NO_ROBOTS		0x00000003
#define	TLM_TIMEOUT			0x00000004
#define	TLM_ERROR_RANGE			0x00000005
#define	TLM_EMPTY			0x00000006
#define	TLM_DRIVE_NOT_ASSIGNED		0x00000007
#define	TLM_NO_TAPE_NAME		0x00000008
#define	TLM_NO_BACKUP_DIR		0x00000009
#define	TLM_NO_BACKUP_HARDWARE		0x0000000a
#define	TLM_NO_SOURCE_FILE		0x0000000b
#define	TLM_NO_FREE_TAPES		0x0000000c
#define	TLM_EOT				0x0000000d
#define	TLM_SERIAL_NOT_FOUND		0x0000000e
#define	TLM_SMALL_READ			0x0000000f
#define	TLM_NO_RESTORE_FILE		0x00000010
#define	TLM_EOF				0x00000011
#define	TLM_NO_DIRECTORY		0x00000012
#define	TLM_NO_MEMORY			0x00000013
#define	TLM_WRITE_ERROR			0x00000014
#define	TLM_NO_SCRATCH_SPACE		0x00000015
#define	TLM_INVALID			0x00000016
#define	TLM_MOVE			0x00000017
#define	TLM_SKIP			0x00000018
#define	TLM_OPEN_ERR			0x00000019


#define	TLM_MAX_TAPE_DRIVES	16
#define	TLM_NAME_SIZE		100
#define	TLM_MAX_TAR_IMAGE	017777777770
#define	NAME_MAX		255

#define	TLM_MAGIC		"ustar  "


#define	RECORDSIZE	512
#define	NAMSIZ	100

typedef struct	tlm_tar_hdr {
	char	th_name[TLM_NAME_SIZE];
	char	th_mode[8];
	char	th_uid[8];
	char	th_gid[8];
	char	th_size[12];
	char	th_mtime[12];
	char	th_chksum[8];
	char	th_linkflag;
	char	th_linkname[TLM_NAME_SIZE];
	char	th_magic[8];
	char	th_uname[32];
	char	th_gname[32];
	union {
		struct {
			char	th_devmajor[8];
			char	th_devminor[8];
		} th_dev;
		char	th_hlink_ino[12];
	} th_shared;
} tlm_tar_hdr_t;



/*
 * The linkflag defines the type of file
 */
#define	LF_OLDNORMAL	'\0'		/* Normal disk file, Unix compat */
#define	LF_NORMAL	'0'		/* Normal disk file */
#define	LF_LINK		'1'		/* Link to previously dumped file */
#define	LF_SYMLINK	'2'		/* Symbolic link */
#define	LF_CHR		'3'		/* Character special file */
#define	LF_BLK		'4'		/* Block special file */
#define	LF_DIR		'5'		/* Directory */
#define	LF_FIFO		'6'		/* FIFO special file */
#define	LF_CONTIG	'7'		/* Contiguous file */
/* Further link types may be defined later. */

#define	LF_DUMPDIR	'D'
					/*
					 * This is a dir entry that contains
					 * the names of files that were in
					 * the dir at the time the dump
					 * was made
					 */
#define	LF_HUMONGUS	'H'
					/*
					 * Identifies the NEXT file on the tape
					 * as a HUGE file
					 */
#define	LF_LONGLINK	'K'
					/*
					 * Identifies the NEXT file on the tape
					 * as having a long linkname
					 */
#define	LF_LONGNAME	'L'
					/*
					 * Identifies the NEXT file on the tape
					 * as having a long name.
					 */
#define	LF_MULTIVOL	'M'
					/*
					 * This is the continuation
					 * of a file that began on another
					 * volume
					 */

#define	LF_VOLHDR	'V'		/* This file is a tape/volume header */
					/* Ignore it on extraction */

#define	LF_ACL		'A'		/* Access Control List */

#define	LF_XATTR	'E'		/* Extended attribute */

#define	KILOBYTE	1024

#define	UFSD_ACL	(1)


/*
 * ACL support structure
 *
 * For TS, we will also save extended attributes as a part of ACL.
 *
 */
typedef struct sec_attr {
	char attr_type;
	int 	attr_len;
//#ifdef QNAP_TS
//	// the acl in xattr will be save in attr_info too. we will by pass this.
//	// After TS have rich ACL, everything will be fine.
//	int		xattr_len;
//#endif
	char 	*attr_info;

} sec_attr_t;

typedef struct	tlm_acls {
	int	acl_checkpointed	: 1,	/* are checkpoints active ? */
		acl_clear_archive	: 1,	/* clear archive bit ? */
		acl_overwrite		: 1,	/* always overwrite ? */
		acl_update		: 1,	/* only update ? */
		acl_non_trivial		: 1;	/* real ACLs? */
		/*
		 * The following fields are here to allow
		 * the backup reader to open a file one time
		 * and keep the information for ACL, ATTRs,
		 * and reading the file.
		 */
	sec_attr_t acl_info;

	char acl_root_dir[NAME_MAX]; /* name of root filesystem */
	fs_fhandle_t acl_dir_fh;		/* parent dir's info */
	fs_fhandle_t acl_fil_fh;		/* file's info */
	struct stat acl_attr;			/* file system attributes */
	char uname[32];
	char gname[32];
} tlm_acls_t;


/*
 * Tape manager's data archiving ops vector
 *
 * This vector represents the granular operations for
 * performing backup/restore. Each backend should provide
 * such a vector interface in order to be invoked by NDMP
 * server.
 * The reserved callbacks are kept for different backup
 * types which are volume-based rather than file-based
 * e.g. zfs send.
 */
typedef struct tm_ops {
	char *tm_name;
	int (*tm_putfile)();
	int (*tm_putdir)();
	int (*tm_putvol)();	/* Reserved */
	int (*tm_getfile)();
	int (*tm_getdir)();
	int (*tm_getvol)();	/* Reserved */
} tm_ops_t;
//
///* The checksum field is filled with this while the checksum is computed. */
/*	for checksum header	*/
#define	CHKBLANKS	"        "	/* 8 blanks, no null */
//
#define	LONGNAME_PREFIX	"././_LoNg_NaMe_"
/*
 * Node in struct hardlink_q
 *
 * inode: the inode of the hardlink
 * path: the name of the hardlink, used during restore
 * offset: tape offset of the data records for the hardlink, used during backup
 * is_tmp: indicate whether the file was created temporarily for restoring
 * other links during a non-DAR partial restore
 */
struct hardlink_node {
	unsigned long inode;
	char *path;
	unsigned long long offset;
	int is_tmp;
	SLIST_ENTRY(hardlink_node) next_hardlink;
};

/*
 * Hardlinks that have been backed up or restored.
 *
 * During backup, each node represents a file whose
 *   (1) inode has multiple links
 *   (2) data has been backed up
 *
 * When we run into a file with multiple links during backup,
 * we first check the list to see whether a file with the same inode
 * has been backed up.  If yes, we backup an empty record, while
 * making the file history of this file contain the data offset
 * of the offset of the file that has been backed up.  If no,
 * we backup this file, and add an entry to the list.
 *
 * During restore, each node represents an LF_LINK type record whose
 * data has been restored (v.s. a hard link has been created).
 *
 * During restore, when we run into a record of LF_LINK type, we
 * first check the queue to see whether a file with the same inode
 * has been restored.  If yes, we create a hardlink to it.
 * If no, we restore the data, and add an entry to the list.
 */
/*	we will just create a fake one, we don't need to use this function	*/
struct hardlink_q {
	struct hardlink_node *slh_first;
};

/* Utility functions from handling hardlink */
extern struct hardlink_q *hardlink_q_init();
extern void hardlink_q_cleanup(struct hardlink_q *qhead);
extern int hardlink_q_get(struct hardlink_q *qhead, unsigned long inode,
    unsigned long long *offset, char **path);
extern int hardlink_q_add(struct hardlink_q *qhead, unsigned long inode,
    unsigned long long offset, char *path, int is_tmp);








/*
 * To prune a directory when traversing it, this return
 * value should be returned by the callback function in
 * level-order and pre-order traversing.
 *
 * In level-order processing, this return value stops
 * reading the rest of the directory and calling the callback
 * function for them.  Traversing will continue with the next
 * directory of the same level.  The children of the current
 * directory will be pruned too.  For example on this ,
 *
 */
#define	FST_SKIP	1


#define	SKIP_ENTRY	2


/*
 * Directives for traversing file system.
 *
 * FST_STOP_ONERR: Stop travergins when stat fails on an entry.
 * FST_STOP_ONLONG: Stop on detecting long path.
 * FST_VERBOSE: Verbose running.
 */
#define	FST_STOP_ONERR		0x00000001
#define	FST_STOP_ONLONG		0x00000002
#define	FST_VERBOSE		0x80000000


typedef void (*ft_log_t)();


/*
 * The arguments of traversing file system contains:
 *     path: The physical path to be traversed.
 *
 *     lpath	The logical path to be passed to the callback
 *         	function as path.
 *         	If this is set to NULL, the default value will be
 *         	the 'path'.
 *
 *         	For example, traversing '/v1.chkpnt/backup/home' as
 *         	physical path can have a logical path of '/v1/home'.
 *
 *     flags	Show how the traversing should be done.
 *         	Values of this field are of FST_ constants.
 *
 *     callbk	The callback function pointer.  The callback
 *         	function is called like this:
 *         	(*ft_callbk)(
 *         		void *ft_arg,
 *         		struct fst_node *path,
 *         		struct fst_node *entry)
 *
 *     arg	The 'void *' argument to be passed to the call
 *		back function.
 *
 *     logfp	The log function pointer.  This function
 *         	is called to log the messages.
 *         	Default is logf().
 */
typedef struct fs_traverse {
	char *ft_path;
//	char *ft_lpath;
	unsigned int ft_flags;
	int (*ft_callbk)();
	void *ft_arg;
	ft_log_t ft_logfp;
} fs_traverse_t;


/*
 * Traversing Nodes.  For each path and node upon entry this
 * structure is passed to the callback function.
 */
typedef struct fst_node {
	char *tn_path;
	fs_fhandle_t *tn_fh;
	struct stat *tn_st;
} fst_node_t;

typedef struct path_list {
	char *pl_path;
	struct path_list *pl_next;
} path_list_t;















#endif	/* !_TLM_H_ */
