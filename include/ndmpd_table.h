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

#ifndef _NDMPD_TABLE_H_
#define	_NDMPD_TABLE_H_

#include <ndmpd.h>

/* inet_ntoa */
#include <arpa/inet.h>

#include <rpc/types.h>

#define	AUTH_REQUIRED		TRUE
#define	AUTH_NOT_REQUIRED	FALSE
/*
 * NDMP request handler functions.
 *	Following define function handler. this will really do the work on the host
 *
 */

/* Get information */
ndmp_msg_handler_func_t ndmpd_config_get_host_info_v3;

ndmp_msg_handler_func_t ndmpd_config_get_butype_info_v3;
ndmp_msg_handler_func_t ndmpd_config_get_butype_info_v4;

ndmp_msg_handler_func_t ndmpd_config_get_connection_type_v3;
ndmp_msg_handler_func_t ndmpd_config_get_auth_attr_v3;
ndmp_msg_handler_func_t ndmpd_config_get_fs_info_v3;
ndmp_msg_handler_func_t ndmpd_config_get_tape_info_v3;
ndmp_msg_handler_func_t ndmpd_config_get_scsi_info_v3;
ndmp_msg_handler_func_t ndmpd_config_get_server_info_v3;

ndmp_msg_handler_func_t ndmpd_config_get_ext_list_v4;
ndmp_msg_handler_func_t ndmpd_config_set_ext_list_v4;

/*
 * we don't have ndmpd_data_get_env_v3, v4 can handle it.
 */
ndmp_msg_handler_func_t ndmpd_data_get_env_v3;
ndmp_msg_handler_func_t ndmpd_data_get_env_v4;

ndmp_msg_handler_func_t ndmpd_data_get_state_v3;
ndmp_msg_handler_func_t ndmpd_data_get_state_v4;

ndmp_msg_handler_func_t ndmpd_data_connect_v3;
ndmp_msg_handler_func_t ndmpd_data_connect_v4;

ndmp_msg_handler_func_t ndmpd_data_listen_v3;
ndmp_msg_handler_func_t ndmpd_data_listen_v4;

ndmp_msg_handler_func_t ndmpd_data_stop_v3;
ndmp_msg_handler_func_t ndmpd_data_abort_v3;
ndmp_msg_handler_func_t ndmpd_data_start_recover_v3;
ndmp_msg_handler_func_t ndmpd_data_start_backup_v3;

ndmp_msg_handler_func_t ndmpd_data_start_recover_filehist_v4;


/* Connect */
ndmp_msg_handler_func_t ndmpd_connect_open_v3;
ndmp_msg_handler_func_t ndmpd_connect_client_auth_v3;
ndmp_msg_handler_func_t ndmpd_connect_server_auth_v3;
ndmp_msg_handler_func_t ndmpd_connect_close_v3;

#endif /* _NDMPD_TABLE_H_ */
