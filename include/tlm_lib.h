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

#ifndef	_TLM_LIB_H
#define	_TLM_LIB_H

char *tlm_get_write_buffer(long want, long *actual_size, tlm_buffers_t *buffers, int zero);
char *tlm_get_read_buffer(int want, int *error, tlm_buffers_t *buffers, int *actual_size);
void tlm_unget_read_buffer(tlm_buffers_t *buffers, int size);
void tlm_unget_write_buffer(tlm_buffers_t *buffers, int size);
void tlm_build_header_checksum(tlm_tar_hdr_t *r);
int tlm_vfy_tar_checksum(tlm_tar_hdr_t *tar_hdr);
tlm_cmd_t *tlm_create_reader_writer_ipc(bool_t write, long data_transfer_size);
void tlm_release_reader_writer_ipc(tlm_cmd_t *cmd);
lbr_fhlog_call_backs_t * lbrlog_callbacks_init(void *cookie, 
		path_hist_func_t log_pname_func, dir_hist_func_t log_dir_func,
		node_hist_func_t log_node_func);
void lbrlog_callbacks_done(lbr_fhlog_call_backs_t *p);
int tlm_log_fhdir(tlm_job_stats_t *job_stats, char *dir, struct stat *stp, fs_fhandle_t *fhp);
int tlm_log_fhnode(tlm_job_stats_t *job_stats, char *dir, char *file,
		struct stat *stp, u_longlong_t off);
int tlm_log_fhpath_name(tlm_job_stats_t *job_stats, char *pathname,
		struct stat *stp, u_longlong_t off);
int tlm_entry_restored(tlm_job_stats_t *job_stats, char *name, int pos);
bool_t tlm_cat_path(char *buf, char *dir, char *name);
void tlm_release_list(char **lpp);
void tlm_log_list(char *title, char **lpp);

char *tlm_remove_checkpoint(char *name, char *unchkp_name);
bool_t tlm_is_excluded(char *dir, char *name, char **excl_files);
longlong_t tlm_get_data_offset(tlm_cmd_t *lcmds);
void tlm_enable_barcode(int l);
int tlm_ioctl(int fd, int cmd, void *data);

bool_t fs_is_chkpntvol(char *path);
bool_t fs_is_chkpnt_enabled(char *path);
bool_t	fs_is_rdonly(char *path);
unsigned min(unsigned a, unsigned b);
unsigned max(unsigned a, unsigned b);
longlong_t llmin(longlong_t a, longlong_t b);

#endif	/* _TLM_LIB_H */
