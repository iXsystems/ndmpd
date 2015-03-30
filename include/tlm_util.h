
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

//#include "tlm_proto.h"

#include <dirent.h>
#include <rpc/types.h>
#include <limits.h>



cstack_t 		*cstack_new(void);

void			cstack_delete(cstack_t *stk);

int				cstack_push(cstack_t *stk, void *data, int len);

int				cstack_pop(cstack_t *stk, void **data, int *len);

int				cstack_top(cstack_t *stk, void **data, int *len);

bool_t			match(char *patn, char *str);

int				match_ci(char *patn, char *str);

static bool_t	parse_match(char line, char *seps);

char *			parse(char **line, char *seps);

int				oct_atoi(char *p);

char 			*strupr(char *s);

char 			*trim_whitespace(char *buf);

char		 	*trim_name(char *nm);

char 			*get_volname(char *path);


bool_t			fs_volexist(char *path);

int				tlm_tarhdr_size(void);

struct full_dir_info 	*dup_dir_info(struct full_dir_info *old_dir_info);

struct full_dir_info 	*tlm_new_dir_info(struct  fs_fhandle *fhp, char *dir, char *nm);

int				sysattr_rdonly(char *name);

int				sysattr_rw(char *name);

int				traverse_level(fs_traverse_t *ftp, bool_t );

bool_t tlm_is_too_long(int, char *, char *);










#ifdef __cplusplus
}
#endif


#endif /* _TLM_UTIL_H_ */
