/*
 * Copyright 2009 Sun Microsystems, Inc.  
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

#ifndef _NDMPD_FHISTORY_H
#define	_NDMPD_FHISTORY_H

/* defined in ndmpd_fhistory.c */
int ndmpd_api_file_history_dir_v3(void *cookie, char *name, u_long node,
			u_long parent);
int ndmpd_api_file_history_node_v3(void *cookie, u_long node,
			struct stat *file_stat, u_longlong_t fh_info);
int ndmpd_api_file_history_file_v3(void *cookie, char *name,
			struct stat *file_stat, u_longlong_t fh_info);

void ndmpd_file_history_init(ndmpd_session_t *session);
void ndmpd_file_history_cleanup(ndmpd_session_t *session, bool_t send_flag);

int ndmpd_fhpath_v3_cb(lbr_fhlog_call_backs_t *cbp, char *path,
	struct stat *stp,u_longlong_t off);
int ndmpd_fhdir_v3_cb(lbr_fhlog_call_backs_t *cbp, char *dir, struct stat *stp);
int ndmpd_fhnode_v3_cb(lbr_fhlog_call_backs_t *cbp, char *dir, char *file,
	struct stat *stp, u_longlong_t off);
int ndmpd_path_restored_v3(lbr_fhlog_call_backs_t *cbp, char *name,
	struct stat *st, u_longlong_t ll_idx);
char *get_bk_path_v3(ndmpd_module_params_t *params);

#endif /* _NDMPD_FHISTORY_H */
