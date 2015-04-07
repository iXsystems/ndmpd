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

#ifndef	_TLM_PROTO_H
#define	_TLM_PROTO_H

#include <tlm.h>
#include <tlm_buffers.h>

#include <dirent.h>
#include <pthread.h>


#define	MAXIORETRY	20

typedef void log_func_t(u_long, char *, ...);

extern longlong_t llmin(longlong_t, longlong_t);
extern unsigned int min(unsigned int, unsigned int);
extern unsigned int max(unsigned int, unsigned int);
extern int oct_atoi(char *p);

extern int tlm_log_fhnode(tlm_job_stats_t *,
    char *,
    char *,
    struct stat *,
    u_longlong_t);

extern int tlm_log_fhdir(tlm_job_stats_t *,
    char *,
    struct stat *,
    struct fs_fhandle *);

extern int tlm_log_fhpath_name(tlm_job_stats_t *,
    char *,
    struct stat *,
    u_longlong_t);


extern void tlm_log_list(char *,
    char **);

extern int tlm_ioctl(int, int, void *);

extern int tlm_library_count(void);

extern bool_t fs_is_rdonly(char *);
extern bool_t fs_is_chkpntvol();
extern int chkpnt_backup_successful();
extern int chkpnt_backup_prepare();
extern int chkpnt_creationtime_bypattern();

extern log_func_t log_debug;
extern log_func_t log_error;
extern bool_t match(char *, char *);

extern void tlm_build_header_checksum(tlm_tar_hdr_t *);
extern int tlm_vfy_tar_checksum(tlm_tar_hdr_t *);
extern int tlm_entry_restored(tlm_job_stats_t *, char *, int);
extern char *strupr(char *);
extern char *parse(char **, char *);
extern int sysattr_rdonly(char *);
extern int sysattr_rw(char *);

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
#endif	/* _TLM_PROTO_H */
