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

#ifndef _NDMPD_TAR_V3_
#define _NDMPD_TAR_V3_

#include <ndmpd_util.h>
#include <ctype.h>
/*
// * IS_LBR_BKTYPE shows if the backup type is one of these
// * 'F' of 'f': 'Full' backup type.
// * 'A' of 'a': 'Archive' backup type.
// * 'I' of 'i': 'Incremental' backup type.
// * 'D' of 'd': 'Differntial' backup type.
// */
#define	IS_LBR_BKTYPE(t)	(((t) && strchr("FAID", toupper(t))) ? 1 : 0)


extern bool_t ndmp_ignore_ctime;
extern bool_t ndmp_include_lmtime;



/*	Defined	*/
ndmp_error	ndmp_restore_get_params_v3(ndmpd_session_t *session, ndmpd_module_params_t *params);
ndmp_error	ndmp_backup_get_params_v3(ndmpd_session_t *session, ndmpd_module_params_t *params);
int	ndmp_send_recovery_stat_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp, int idx, int stat);


void setWriteBufDone(tlm_buffers_t *bufs);
void setReadBufDone(tlm_buffers_t *bufs);


#endif /* _NDMPD_TAR_V3_ */
