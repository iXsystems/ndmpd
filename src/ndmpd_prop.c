

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
	char		*name;
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

ndmpd_cfg_param_t ndmpd_cfg_table[] =
{
	{"listen-nic",					""			},
	{"serve-nic",					""			},
	{"dump-pathnode",				"true" 		},
	{"tar-pathnode",				"true" 		},
	{"fh-inode",					"true" 		},
	{"ignore-ctime",				"false"		},
	{"include-lmtime",				"false"		},
	{"version",						"4"			},
	{"restore-fullpath",			"false"		},
#ifdef QNAP_TS
	{"debug-path",					"./log"	},
	{"plugin-path",					"./log"	},
#else
	{"debug-path",					"/var/log/ndmp"	},
	{"plugin-path",					"/var/log/ndmp"	},
#endif
	{"socket-css",					"65" 		},
	{"socket-crs",					"80" 		},
	{"mover-recordsize",			"60" 		},
	{"restore-wildcard-enable",		"false" 	},
	{"cram-md5-username",			""			},
	{"cram-md5-password",			""	 		},
	{"cleartext-username",			""			},
	{"cleartext-password",			""			},
	{"tcp-port",					"10000" 	},
	{"backup-quarantine",			"false" 	},
	{"restore-quarantine",			"false" 	},
	{"overwrite-quarantine",		"false" 	},
};


void print_prop(){
	ndmpd_cfg_id_t id;
	ndmpd_cfg_param_t *cfg;
	for (id = 0; id < NDMP_MAXALL; id++) {
		cfg = &ndmpd_cfg_table[id];
		printf("ndmpd_prop- key=%30s	value=%20s\n",cfg->name,cfg->value);
	}
}


/*
 * Loads all the NDMP configuration parameters and sets up the
 * config table.
 */

void setup(char *line){
	int ki,vi,idx,iskey;
	char key[64];
	char value[64+1];
	ndmpd_cfg_id_t id;
	ndmpd_cfg_param_t *cfg;

	if(strlen(line)==0 || line[0]=='#' || line[0]=='\n')
		return ;

	for(ki=0,vi=0,idx=0,iskey=1;idx<strlen(line);idx++){
		if(line[idx]<33 || line[idx]>126)
			continue;
		if(line[idx]=='=' && iskey){
			key[ki++]='\0';
			iskey=0;
		}else{
			if(iskey)
				key[ki++]=tolower(line[idx]);
			else
				value[vi++]=line[idx];
		}
	}

	value[vi]='\0';
	// we only support maximum 64 characters.
	if(vi>64)
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
ndmpd_load_prop(char *filename)
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
	int i=0;

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

	if (env && *env != 0) {
		return (env);
	} else {
		return (dflt);
	}
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

