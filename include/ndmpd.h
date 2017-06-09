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

#ifndef _NDMPD_H_
#define	_NDMPD_H_

#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h> /* memset */
#include <unistd.h> /* close */
#include <stdio.h>

/* free() */
#include <stdlib.h>

#include <sys/types.h>
/* thread */
#include <pthread.h>
/* ndmp xdr define */
#include <ndmp.h>

#include <syslog.h>

/* struct stat */
#include <sys/stat.h>
/* LIST_ENTRY and all LIST operation */
#include <sys/queue.h>

#define	MAX_RECORD_SIZE (126*512)
#define	REMOTE_RECORD_SIZE (60*KB)
#define	SCSI_MAX_NAME 32
#define	MD5_CHALLENGE_SIZE 64
#define	MD5_PASS_LIMIT 32
#define	MAX_BACKUP_JOB_NAME 32	/* max size of a job's name */

/* File handler classes */
#define	HC_CLIENT	1
#define	HC_MOVER	2
#define	HC_MODULE	4
#define	HC_ALL		0xffffffff

#define	NDMP_PROC_ERR	-1
#define	NDMP_PROC_MSG	1
#define	NDMP_PROC_REP	0
#define	NDMP_PROC_REP_ERR	2

#define	INT_MAXCMD	12

#define KB	1024

// FIXME: check on this value later.
#define	BUFFER_SIZE	32768

#ifdef EMC_MODEL
/* pretend we are a EMC product for Symantec BackupExec */
	#define	VENDOR_NAME	"EMC2"
	#define	PRODUCT_NAME	"CELERRA"
	#define OS_TYPE		"DartOS"
	#define OS_VERSION	"EMC Celerra File Server.T.6.0.55abc1997"
	#define SOFTWARE_REV 	"T.6.0.55.3"
#else
	#define	VENDOR_NAME	"FreeBSD"
	#define	PRODUCT_NAME	"FreeBSD"
	#define OS_TYPE         "BSD"
	#define OS_VERSION      "FreeBSD"
	#define SOFTWARE_REV    "XXX"
#endif

/*
 * Patchable socket buffer sizes in kilobytes.
 * ssb: send buffer size.
 * rsb: receive buffer size.
 */
#define ndmp_sbs  60
#define ndmp_rbs  60

/*
 * NLP flags.
 */
#define	NLPF_SNAP	(1 << 0)
#define	NLPF_FH		(1 << 1)
#define	NLPF_DIRECT	(1 << 2)
#define	NLPF_UPDATE	(1 << 3)
#define	NLPF_DUMP	(1 << 4)
#define	NLPF_TAR	(1 << 5)
#define	NLPF_ABORTED	(1 << 6)
//#define	NLPF_TOKENBK		(1 << 8)
//#define	NLPF_LBRBK		(1 << 9)
#define	NLPF_LEVELBK	(1 << 10)
#define	NLPF_IGNCTIME	(1 << 11)
#define	NLPF_INCLMTIME	(1 << 12)
#define	NLPF_RECURSIVE	(1 << 13)

/*
 * Macros on NLP flags.
 */
#define	NLP_ISSET(n, f)	(((n)->nlp_flags & (f)) != 0)
#define	NLP_SET(n, f)	(n)->nlp_flags |= (f)
#define	NLP_UNSET(n, f)	(n)->nlp_flags &= ~(f)


#define	NLP_ISSNAP(n)		NLP_ISSET(n, NLPF_SNAP)
#define	NLP_SHOULD_UPDATE(n)	NLP_ISSET(n, NLPF_UPDATE)
#define	NLP_ISDUMP(n)		NLP_ISSET(n, NLPF_DUMP)
#define	NLP_ISTAR(n)		NLP_ISSET(n, NLPF_TAR)
#define	NLP_IGNCTIME(n)		NLP_ISSET(n, NLPF_IGNCTIME)
#define	NLP_INCLMTIME(n)	NLP_ISSET(n, NLPF_INCLMTIME)

/*
 * Calculate array length based on its size and size of
 * its elements.
 */
#define	ARRAY_LEN(a, t)	(sizeof (a) / sizeof (t))

/*
 * Default maximum permitted sequence number for the token-based backup.
 */
#define	NDMP_MAX_TOKSEQ	9

/*
 * Hard-limit for the sequence number in the token-based backup.
 * It's one less than the ASCII value of 'A'.  The 'A' letter
 * can be used as level in the lbr-type backups.
 */
#define	NDMP_TOKSEQ_HLIMIT ('A' - 1)


/*
 * Soft-limit for the sequence number in the token-based backup.
 */
#define	NDMP_TOKSEQ_SLIMIT (NDMP_TOKSEQ_HLIMIT - 5)


/*
 * Root inode number of dump format in V2.
 */
#define	ROOT_INODE 2

/*
 * NDMP backup image signature.
 */
#define	NDMPUTF8MAGIC "NDMPUTF8MAGIC"

/*
 * Supported BU types
 */
#define	NDMP_DUMP_TYPE	"dump"
#define	NDMP_TAR_TYPE	"tar"

/* All 1's binary maximum mover window */
#define	MAX_WINDOW_SIZE	0xffffffffffffffffULL

#define	NDMP_FREE(cp) 	{ free((char *)(cp)); (cp) = NULL; }

#define	NDMP_YORN(f)	((f) ? 'Y' : 'N')
#define	NDMP_TORF(f)	((f) ? "TRUE" : "FALSE")
#define	NDMP_SVAL(cp)	((cp) ? (cp) : "NULL")

#define	NDMP_SETENV(env, nm, val) \
	{ \
		env->name = nm; \
		env->value = val; \
		env++; \
	}

#define	NDMP_CL_ADDR_LEN 	24
#define	NDMP_TCP_ADDR_SIZE	32
#define	NDMP_TAPE_DEV_NAME	256

/*	dispatch message, mapping function with it's version	*/
#define	XDR_AND_SIZE(func) (bool_t(*)(XDR*, ...))xdr_##func, sizeof (func)

/*
 * List of files/directories to be excluded from backup list.
 */
#define	EXCL_PROC	"/proc"
#define	EXCL_TMP	"/tmp"

typedef long long	longlong_t;
typedef unsigned long long u_longlong_t;

typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;
typedef pthread_rwlock_t rwlock_t;

/*
 *	POSXI to SOLARIS
 *	typedef PTHREAD_MUTEX_INITIALIZER	DEFAULTMUTEX
 *	typedef PTHREAD_COND_INITIALIZER	DEFAULTCV
 *	typedef PTHREAD_RWLOCK_INITIALIZER	DEFAULTRWLOCK
 */
#define	mutex_init(l,f,a)	pthread_mutex_init(l,NULL)
#define	mutex_destroy(l)	pthread_mutex_destroy(l)
#define	mutex_lock(l)		pthread_mutex_lock(l)
#define	mutex_trylock(l)	pthread_mutex_trylock(l)
#define	mutex_unlock(l)		pthread_mutex_unlock(l)
#define	rwlock_init(l,f,a)	pthread_rwlock_init(l,NULL)
#define	rwlock_destroy(l)	pthread_rwlock_destroy(l)
#define	rw_rdlock(l)		pthread_rwlock_rdlock(l)
#define	rw_wrlock(l)		pthread_rwlock_wrlock(l)
#define	rw_tryrdlock(l)		pthread_rwlock_tryrdlock(l)
#define	rw_trywrlock(l)		pthread_rwlock_trywrlock(l)
#define	rw_unlock(l)		pthread_rwlock_unlock(l)
#define	cond_init(l,f,a)	pthread_cond_init(l,NULL)
#define	cond_destroy(l)		pthread_cond_destroy(l)
#define	cond_wait(l,m)		pthread_cond_wait(l,m)
#define	cond_signal(l)		pthread_cond_signal(l)
#define	cond_broadcast(l)	pthread_cond_broadcast(l)


typedef void *(*funct_t)(void *);	/* function pointer */

/* Connection data structure. */
typedef struct msg_info {
	/* ndmp_header defined in ndmp.x	*/
	ndmp_header mi_hdr;
	struct ndmp_msg_handler *mi_handler;
	void *mi_body;
} msg_info_t;

typedef struct ndmp_connection {
	int conn_sock;
	XDR conn_xdrs;
	u_long conn_my_sequence;
	bool_t conn_authorized;
	bool_t conn_eof;
	msg_info_t conn_msginfo; /* received request or reply message */
	u_short conn_version;
	void *conn_client_data;
	pthread_mutex_t conn_lock;
} ndmp_connection_t;


typedef void (*ndmp_con_handler_func_t) (struct ndmp_connection *);

/* function pointer */
typedef void ndmp_msg_handler_func_t(struct ndmp_connection *, void *);

/* find the corresponding interface handle. */
typedef struct ndmp_msg_handler {
	ndmp_msg_handler_func_t *mh_func;
	bool_t(*mh_xdr_request) (XDR *xdrs, ...);
	int mh_sizeof_request;
	bool_t(*mh_xdr_reply) (XDR *xdrs, ...);
	int mh_sizeof_reply;
} ndmp_msg_handler_t;

/* structure of handlers, check on the ndmpd_dispatch file */
typedef struct ndmp_handler {
	int hd_cnt;
	struct hd_messages {
		ndmp_message hm_message;
		bool_t hm_auth_required;
		ndmp_msg_handler_t hm_msg_v[3];
	} hd_msgs[INT_MAXCMD];
} ndmp_handler_t;

typedef struct ndmp_chkpnt_vol {
	char cv_vol_name[64];
	unsigned int cv_count;
	void *cv_next;
} ndmp_chkpnt_vol_t;

#define	NDMPD_SELECT_MODE_READ		1
#define	NDMPD_SELECT_MODE_WRITE		2
#define	NDMPD_SELECT_MODE_EXCEPTION	4


/* extern functions */
extern int ndmp_connect_list_add(ndmp_connection_t *connection, int *id);
extern int ndmp_connect_list_del(ndmp_connection_t *connection);

/* define a print log function */
void ndmpd_log(int level, const char *fmt,...);

/* functions prototype */
void * ndmpd_worker(void *ptarg);
int ndmp_run(u_long port, ndmp_con_handler_func_t con_handler_func);
void connection_handler(ndmp_connection_t *connection);
bool_t file_exists(const char * filename);
int startNDMPD(void);
static char * getline_ndmpd(FILE *fp, char *line, int llen);

#endif /* _NDMPD_H_ */
