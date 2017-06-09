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

#ifndef _TLM_UTIL_H_
#define	_TLM_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <tlm.h>
#include <tlm_buffers.h>
#include <ndmpd.h>
#include <cstack.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <ctype.h>

#include <dirent.h>
#include <rpc/types.h>
#include <limits.h>

cstack_t *cstack_new(void);
void cstack_delete(cstack_t *stk);
int cstack_push(cstack_t *stk, void *data, int len);
int cstack_pop(cstack_t *stk, void **data, int *len);
int cstack_top(cstack_t *stk, void **data, int *len);
bool_t match(char *patn, char *str);
int match_ci(char *patn, char *str);
char *parse(char **line, char *seps);
int oct_atoi(char *p);
char *strupr(char *s);
char *trim_whitespace(char *buf);
char *trim_name(char *nm);
char *get_volname(char *path);
bool_t fs_volexist(char *path);
int tlm_tarhdr_size(void);
struct full_dir_info *dup_dir_info(struct full_dir_info *old_dir_info);
struct full_dir_info *tlm_new_dir_info(struct  fs_fhandle *fhp, char *dir, char *nm);
int sysattr_rdonly(char *name);
int sysattr_rw(char *name);
int traverse_level(fs_traverse_t *ftp, bool_t );
bool_t tlm_is_too_long(int, char *, char *);

#ifdef __cplusplus
}
#endif

#endif /* _TLM_UTIL_H_ */
