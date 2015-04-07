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

/*

	Defines the function that will be used by function in handlers.

 */

#ifndef _NDMPD_CALLBACKS_H
#define	_NDMPD_CALLBACKS_H

#include <ndmpd.h>

/*
 * NDMP daemon callback functions. used in ndmpd_module_params structure which will handle the data operations.
 * Called by backup/recover modules.
 */
 /*	Session Parameter */
typedef char*	ndmpd_get_env_func_t(void *, char *);
typedef int 	ndmpd_add_env_func_t(void *, char *, char *);
typedef void*	ndmpd_get_name_func_t(void *, u_long);
typedef int 	ndmpd_dispatch_func_t(void *, bool_t);
typedef void 	ndmpd_done_func_t(void *, int);
typedef int 	ndmpd_log_func_t(void *, char *, ...);

typedef void ndmpd_file_handler_func_t(void *, int, u_long);

typedef int ndmpd_log_func_v3_t(void *, ndmp_log_type, u_long,	char *, ...);
typedef int ndmpd_add_file_handler_func_t(void *, void *, int, u_long,	ndmpd_file_handler_func_t *);
typedef int ndmpd_remove_file_handler_func_t(void *, int);
typedef int ndmpd_write_func_t(void *, char *, u_long);
typedef int ndmpd_file_history_path_func_t(void *, char *, struct stat *,	u_longlong_t);
typedef int ndmpd_file_history_dir_func_t(void *, char *, u_long,	u_long);
typedef int ndmpd_file_history_node_func_t(void *, u_long, struct stat *,	u_longlong_t);
typedef int ndmpd_seek_func_t(void *, u_longlong_t, u_longlong_t);
typedef int ndmpd_read_func_t(void *, char *, u_long);
typedef int ndmpd_file_recovered_func_t(void *, char *, int);




void		ndmpd_api_done_v3(void *cookie, int err);
int		ndmpd_api_log_v3(void *cookie, ndmp_log_type type, u_long msg_id, char *format, ...);
int			ndmpd_api_write_v3(void *client_data, char *data, u_long length);
int			ndmpd_api_read_v3(void *client_data, char *data, u_long length);
void*		ndmpd_api_get_name_v3(void *cookie, u_long name_index);
int 		ndmpd_api_file_recovered_v3(void *cookie, char *name, int error);
int 		ndmpd_api_seek_v3(void *cookie, u_longlong_t offset, u_longlong_t length);
int		ndmpd_api_log_v4(void *cookie, ndmp_log_type type, u_long msg_id, char *format, ...);
int			ndmpd_api_file_recovered_v4(void *cookie, char *name, int error);
ndmp_pval *	ndmpd_api_find_env(void *cookie, char *name);
char *		ndmpd_api_get_env(void *cookie, char *name);
int			ndmpd_api_add_env(void *cookie, char *name, char *value);
int			ndmpd_api_set_env(void *cookie, char *name, char *value);
void *		ndmpd_api_get_name(void *cookie, u_long name_index);
int			ndmpd_api_dispatch(void *cookie, bool_t block);
int			ndmpd_api_add_file_handler(void *daemon_cookie, void *cookie, int fd, u_long mode, ndmpd_file_handler_func_t *func);
int			ndmpd_api_remove_file_handler(void *cookie, int fd);



#endif /* _NDMPD_CALLBACKS_H */

