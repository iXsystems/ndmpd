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

#ifndef _NDMPD_SNAPSHOT_H_
#define	_NDMPD_SNAPSHOT_H_

/*
 * SNAPSHOT
 * */

#include <ndmpd.h>
#include <ndmpd_func.h>
#include <mnt_variable.h>


#define	SNAPSHOT_PREFIX	".zfs"
#define	SNAPSHOT_DIR	".zfs/snapshot"

#define	VOLNAME_MAX_LENGTH	255



typedef struct chkpnt_param {
	char *chp_name;
	bool_t chp_found;
} chkpnt_param_t;

unsigned int	ndmp_add_chk_pnt_vol(char *vol_name);

//int	ndmpd_take_snapshot(char *vol_name, char *jname, char *snapshot_path);

//#ifdef QNAP_TS
//	int	ndmpd_delete_snapshot(int snapshot_id);
//#else
//	int	ndmpd_delete_snapshot(char *vol_name, char *jname);
//#endif
bool_t fs_is_rdonly(char *);
bool_t fs_is_chkpntvol();
int chkpnt_backup_successful();
int chkpnt_backup_prepare();

#ifdef QNAP_TS

#else
	int get_zfsvolname(char *, int, char *);
#endif

int chkpnt_creationtime_bypattern();

char 		*ndmpd_build_snapshot_name(char *name, char *sname, char *jname);

#endif /* _NDMPD_SNAPSHOT_H_ */


