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
 * NDMP configuration management
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>

#include "ndmpd_prop.h"
#include "ndmpd.h"

typedef struct ndmpd_cfg_param {
	const char		*name;
	char		value[64+1];
} ndmpd_cfg_param_t;

/*
 * programmable argument.
 *
 * 1. username
 * 2. password
 * 3. restore-fullpath
 * 4. serve-ip
 *
 * */
static
ndmpd_cfg_param_t ndmpd_cfg_table[] =
{
	{"listen-nic", ""},
	{"serve-nic", ""},
	{"dump-pathnode", "true"},
	{"tar-pathnode", "true"},
	{"fh-inode", "true"},
	{"ignore-ctime", "false"},
	{"include-lmtime", "false"},
	{"version", "4"},
	{"restore-fullpath", "false"},
	{"debug-path", "/var/log/ndmp"},
	{"plugin-path", "/var/log/ndmp"},
	{"socket-css", "65"},
	{"socket-crs", "80"},
	{"mover-recordsize", "60"},
	{"restore-wildcard-enable", "false"},
	{"cram-md5-username", ""},
	{"cram-md5-password", ""},
	{"cleartext-username", ""},
	{"cleartext-password", ""},
	{"tcp-port", "10000"},
	{"backup-quarantine", "false"},
	{"restore-quarantine",	"false"},
	{"overwrite-quarantine", "false"},
};

void print_prop(){
	ndmpd_cfg_id_t id;
	ndmpd_cfg_param_t *cfg;
	for (id = 0; id < NDMP_MAXALL; id++) {
		cfg = &ndmpd_cfg_table[id];
		printf("ndmpd_prop- key=%30s value=%20s\n",cfg->name,cfg->value);
	}
}

/*
 * Loads all the NDMP configuration parameters and sets up the
 * config table.
 */
void setup(char *line){
	int ki,vi,iskey;
	unsigned long idx;
	char key[64];
	char value[64+1];
	ndmpd_cfg_id_t id;
	ndmpd_cfg_param_t *cfg;

	if (strlen(line)==0 || line[0]=='#' || line[0]=='\n')
		return ;

	for (ki=0, vi=0, idx=0, iskey=1; idx < strlen(line); idx++){
		if (line[idx]<33 || line[idx]>126)
			continue;
		if (line[idx]=='=' && iskey) {
			key[ki++]='\0';
			iskey=0;
		} else {
			if (iskey)
				key[ki++]=tolower(line[idx]);
			else
				value[vi++]=line[idx];
		}
	}

	value[vi]='\0';
	// we only support maximum 64 characters.
	if (vi > 64)
		return ;

	for (id = 0; id < NDMP_MAXALL; id++) {
		cfg = &ndmpd_cfg_table[id];
		if (!strncmp(cfg->name,key,strlen(key))) {
			(void) strncpy(cfg->value, value,64+1);
			break;
		}
	}
}

int
ndmpd_load_prop(const char *filename)
{
	FILE *fp=NULL;
	char str[128];

	fp=fopen(filename, "r");
	if(!fp){
		printf("Open configuration file fail.\n");
		return 1;
	}

	memset(str,0,128);
	while(fgets(str,128,fp)){
		setup(str);
		memset(str,0,128);
	}
	fclose(fp);

	return (0);
}

/*
 * Returns value of the specified config param.
 * The return value is a string pointer to the locally
 * allocated memory if the config param is defined
 * otherwise it would be NULL.
 */
char *
ndmpd_get_prop(ndmpd_cfg_id_t id)
{
	char *env_val;

	if (id < NDMP_MAXALL) {
		env_val = ndmpd_cfg_table[id].value;
		return (env_val);
	}

	return (0);
}

/*
 * Similar to ndmpd_get_prop except it will return dflt value
 * if env is not set.
 */
char *
ndmpd_get_prop_default(ndmpd_cfg_id_t id, char *dflt)
{
	char *env;

	env = ndmpd_get_prop(id);

	if (env && *env != 0)
		return (env);
	else
		return (dflt);
}

/*
 * Returns the value of a yes/no config param.
 * Returns 1 is config is set to "yes", otherwise 0.
 */
int
ndmpd_get_prop_yorn(ndmpd_cfg_id_t id)
{
	char *val;

	val = ndmpd_get_prop(id);
	if (val) {
		if (strcasecmp(val, "true") == 0)
			return (1);
	}

	return (0);
}

