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

#include <tlm_util.h>


#include <ndmpd_func.h>

/*
 * Implementation of a list based stack class. The stack only holds
 * pointers/references to application objects. The objects are not
 * copied and the stack never attempts to dereference or access the
 * data objects. Applications should treat cstack_t references as
 * opaque handles.
 */

/*
 * cstack_new
 *
 * Allocate and initialize a new stack, which is just an empty cstack_t.
 * A pointer to the new stack is returned. This should be treated as an
 * opaque handle by the caller.
 */
cstack_t *
cstack_new(void)
{
	cstack_t *stk;

	if ((stk = ndmp_malloc(sizeof (cstack_t))) == NULL)
		return (NULL);

	return (stk);
}


/*
 * cstack_delete
 *
 * Deallocate the stack. This goes through the list freeing all of the
 * cstack nodes but not the data because we don't know how the data was
 * allocated. A stack really should be empty before it is deleted.
 */
void
cstack_delete(cstack_t *stk)
{
	cstack_t *tmp;

	if (stk == NULL) {
		ndmpd_log(LOG_DEBUG, "cstack_delete: invalid stack");
		return;
	}

	while ((tmp = stk->next) != NULL) {
		stk->next = tmp->next;
		free(tmp);
	}

	free(stk);
}


/*
 * cstack_push
 *
 * Push an element onto the stack. Allocate a new node and assign the
 * data and len values. We don't care what about the real values of
 * data or len and we never try to access them. The stack head will
 * point to the new node.
 *
 * Returns 0 on success. Otherwise returns -1 to indicate overflow.
 */
int
cstack_push(cstack_t *stk, void *data, int len)
{
	cstack_t *stk_node;

	if (stk == NULL) {
		ndmpd_log(LOG_DEBUG, "cstack_push: invalid stack");
		return (-1);
	}

	if ((stk_node = ndmp_malloc(sizeof (cstack_t))) == NULL)
		return (-1);

	stk_node->data = data;
	stk_node->len = len;
	stk_node->next = stk->next;
	stk->next = stk_node;

//	ndmpd_log(LOG_DEBUG, "cstack_push(0x%p): 0x%p", stk, stk_node);
	return (0);
}


/*
 * cstack_pop
 *
 * Pop an element off the stack. Set up the data and len references for
 * the caller, advance the stack head and free the popped stack node.
 *
 * Returns 0 on success. Otherwise returns -1 to indicate underflow.
 */
int
cstack_pop(cstack_t *stk, void **data, int *len)
{
	cstack_t *stk_node;

	if (stk == NULL) {
		ndmpd_log(LOG_DEBUG, "cstack_pop: invalid stack");
		return (-1);
	}

	if ((stk_node = stk->next) == NULL) {
		return (-1);
	}

	if (data)
		*data = stk_node->data;

	if (len)
		*len = stk_node->len;

	stk->next = stk_node->next;

	free(stk_node);
	return (0);
}

/*
 * cstack_top
 *
 * Returns the top data element on the stack without removing it.
 *
 * Returns 0 on success. Otherwise returns -1 to indicate underflow.
 */
int
cstack_top(cstack_t *stk, void **data, int *len)
{
	if (stk == NULL) {
		ndmpd_log(LOG_DEBUG, "cstack_pop: invalid stack");
		return (-1);
	}

	if (stk->next == NULL) {
		return (-1);
	}

	if (data)
		*data = stk->next->data;

	if (len)
		*len = stk->next->len;

	return (0);
}


/*
 * match
 *
 * Matching rules:
 *	c	Any non-special character matches itslef
 *	?	Match any character
 *	ab	character 'a' followed by character 'b'
 *	S	Any string of non-special characters
 *	AB	String 'A' followed by string 'B'
 *	*	Any String, including the empty string
 */
bool_t
match(char *patn, char *str)
{
	for (; ; ) {
		switch (*patn) {
		case 0:
			return (*str == 0);

		case '?':
			if (*str != 0) {
				str++;
				patn++;
				continue;
			} else {
				return (FALSE);
			}
			break;

		case '*':
			patn++;
			if (*patn == 0)
				return (TRUE);

			while (*str) {
				if (match(patn, str))
					return (TRUE);
				str++;
			}
			return (FALSE);

		default:
			if (*str != *patn)
				return (FALSE);
			str++;
			patn++;
			continue;
		}
	}
	return TRUE;
}

/*
 * Match recursive call
 */
int
match_ci(char *patn, char *str)
{
	/*
	 * "<" is a special pattern that matches only those names
	 * that do NOT have an extension. "." and ".." are ok.
	 */
	if (strcmp(patn, "<") == 0) {
		if ((strcmp(str, ".") == 0) || (strcmp(str, "..") == 0))
			return (TRUE);
		if (strchr(str, '.') == 0)
			return (TRUE);
		return (FALSE);
	}
	for (; ; ) {
		switch (*patn) {
		case 0:
			return (*str == 0);

		case '?':
			if (*str != 0) {
				str++;
				patn++;
				continue;
			} else {
				return (FALSE);
			}
			break;


		case '*':
			patn++;
			if (*patn == 0)
				return (TRUE);

			while (*str) {
				if (match_ci(patn, str))
					return (TRUE);
				str++;
			}
			return (FALSE);

		default:
			if (*str != *patn) {
				int	c1 = *str;
				int	c2 = *patn;

				c1 = tolower(c1);
				c2 = tolower(c2);
				if (c1 != c2)
					return (FALSE);
			}
			str++;
			patn++;
			continue;
		}
	}
	/* NOT REACHED */
	return (FALSE);
}

/*
 * Linear matching against a list utility function
 */
static bool_t
parse_match(char line, char *seps)
{
	char *sep = seps;

	while (*sep != 0) {
		/* compare this char with the seperator list */
		if (*sep == line)
			return (TRUE);
		sep++;
	}
	return (FALSE);
}

/*
 * Returns the next entry of the list after
 * each separator
 */
char *
parse(char **line, char *seps)
{
	char *start = *line;

	while (**line != 0) {
		*line = *line + 1;
		if (parse_match(**line, seps)) {
			/* hit a terminator, skip trailing terminators */
			while (parse_match(**line, seps)) {
				**line = 0;
				*line = *line + 1;
			}
			break;
		}
	}
	return (start);
}

/*
 * oct_atoi
 *
 * Convert an octal string to integer
 */
int
oct_atoi(char *p)
{
	int v = 0;
	int c;

	while (*p == ' ')
		p++;

	while ('0' <= (c = *p++) && c <= '7') {
		v <<= 3;
		v += c - '0';
	}

	return (v);
}

/*
 * strupr
 *
 * Convert a string to uppercase using the appropriate codepage. The
 * string is converted in place. A pointer to the string is returned.
 * There is an assumption here that uppercase and lowercase values
 * always result encode to the same length.
 */
char *
strupr(char *s)
{
	char c;
	unsigned char *p = (unsigned char *)s;

	while (*p) {
		c = toupper(*p);
		*p++ = c;
	}
	return (s);
}

/*
 * trim_whitespace
 *
 * Trim leading and trailing whitespace chars(as defined by isspace)
 * from a buffer. Example; if the input buffer contained "  text  ",
 * it will contain "text", when we return. We assume that the buffer
 * contains a null terminated string. A pointer to the buffer is
 * returned.
 */
char *
trim_whitespace(char *buf)
{
	char *p = buf;
	char *q = buf;

	if (buf == 0)
		return (0);

	while (*p && isspace(*p))
		++p;

	while ((*q = *p++) != 0)
		++q;

	if (q != buf) {
		while ((--q, isspace(*q)) != 0)
			*q = '\0';
	}

	return (buf);
}

/*
 * trim_name
 *
 * Trims the slash and dot slash from the beginning of the
 * path name.
 */
char *
trim_name(char *nm)
{
	while (*nm) {
		if (*nm == '/') {
			nm++;
			continue;
		}
		if (*nm == '.' && nm[1] == '/' && nm[2]) {
			nm += 2;
			continue;
		}
		break;
	}
	return (nm);
}

/*
 * get_volname
 *
 * Extract the volume name from the path
 */
char *
get_volname(char *path)
{
	char *cp, *save;
	int sp;

	if (!path)
		return (NULL);

	if (!(save = strdup(path)))
		return (NULL);

	sp = strspn(path, "/");
	if (*(path + sp) == '\0') {
		free(save);
		return (NULL);
	}

	if ((cp = strchr(save + sp, '/')))
		*cp = '\0';

	return (save);
}

/*
 * fs_volexist
 *
 * Check if the volume exists
 */
bool_t
fs_volexist(char *path)
{
	ndmpd_log(LOG_DEBUG, "fs_volexist");
	return (TRUE);
//	struct stat st;
//	char *p;
//
//	if ((p = get_volname(path)) == NULL)
//		return (FALSE);
//
//	if (stat(p, &st) != 0) {
//		free(p);
//		return (FALSE);
//	}
//
//	free(p);
//	return (TRUE);
}

/*
 * tlm_tarhdr_size
 *
 * Returns the size of the TLM_TAR_HDR structure.
 */
int
tlm_tarhdr_size(void)
{
	return (sizeof (tlm_tar_hdr_t));
}

/*
 * dup_dir_info
 *
 * Make and return a copy of the directory info.
 */
struct full_dir_info *
dup_dir_info(struct full_dir_info *old_dir_info)
{
	struct	full_dir_info *new_dir_info;
	new_dir_info = ndmp_malloc(sizeof (struct full_dir_info));

	if (new_dir_info) {
		bcopy(old_dir_info, new_dir_info,
		    sizeof (struct full_dir_info));
	}
	return (new_dir_info);
}

/*
 * tlm_new_dir_info
 *
 * Create a new structure, set fh field to what is specified and the path
 * to the concatenation of directory and the component
 */
struct full_dir_info *
tlm_new_dir_info(struct  fs_fhandle *fhp, char *dir, char *nm)
{

	ndmpd_log(LOG_DEBUG, ".............tlm_new_dir_info...............");
	struct full_dir_info *fdip;
//
//	if (!(fdip = ndmp_malloc(sizeof (struct full_dir_info))))
//		return (NULL);
//
//	(void) memcpy(&fdip->fd_dir_fh, fhp, sizeof (fs_fhandle_t));
//	if (!tlm_cat_path(fdip->fd_dir_name, dir, nm)) {
//		free(fdip);
//		ndmpd_log(LOG_DEBUG, "TAPE BACKUP Find> path too long [%s][%s]",
//		    dir, nm);
//		return (NULL);
//	}
	return (fdip);
}

/*
 * sysattr_rdonly
 *
 * Check if the attribute file is one of the readonly system
 * attributes.
 */
int
sysattr_rdonly(char *name)
{
	ndmpd_log(LOG_DEBUG, ".............sysattr_rdonly............... name=%s",name);
	return 0;
//	return (name && strcmp(name, SYSATTR_RDONLY) == 0);
}

/*
 * sysattr_rw
 *
 * Check if the attribute file is one of the read/write system
 * attributes.
 */
int
sysattr_rw(char *name)
{
	ndmpd_log(LOG_DEBUG, ".............sysattr_rw............... name=%s",name);
		return 0;
}

/*
 * Check if the path is too long
 */
bool_t
tlm_is_too_long(int checkpointed, char *dir, char *nm)
{
	int nlen, tot;

	tot = 0;
	if (dir)
		tot += strlen(dir);
//	if (checkpointed)
//		tot += strlen(SNAPSHOT_DIR) + 1;
	if (nm) {
		if ((nlen = strlen(nm)) > 0)
			tot += nlen + 1;
	}
	return ((tot >= PATH_MAX) ? TRUE : FALSE);
}

/****************** Traversal on the node. ***********************/

/*
* LEVEL-ORDER:
 * This is a special case of pre-order.  In this method,
 * all the non-directory entries of a directory are processed
 * and then come the directory entries.  Level-order traversing
 * of the above hierarchy will be like this:
 *
 * AAA
 * AAA, 10
 * AAA, 11
 * AAA, 12
 * AAA, 13
 * AAA, BBB
 * AAA/BBB, 1
 * AAA/BBB, 2
 * AAA/BBB, 3
 * AAA, CCC
 * AAA/CCC, 9
 * AAA/CCC, EEE
 * AAA/CCC/EEE, 6
 * AAA/CCC/EEE, 5
 * AAA/CCC/EEE, 8
 * AAA/CCC/EEE, 4
 * AAA/CCC/EEE, 7
 * AAA, XXX
 * AAA, ZZZ
 *
 * The rules of pre-order for the return value of callback
 * function applies for level-order.
 */


#define	CALLBACK(pp, ep)	\
	(*(ftp)->ft_callbk)((ftp)->ft_arg, pp, ep)

extern int FORCE_STOP_TRAVEL;

int traverse_level(fs_traverse_t *ftp, bool_t stopOnError)
{
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
	int done=0;
	fs_traverse_t *tmpftp;
	fs_traverse_t *tmpfs;
	int len;
	char* path=(char*)calloc(PATH_MAX+1, 1);
	//char path[PATH_MAX+1];

	fs_fhandle_t fh;
	struct fst_node pn, en;
	int rv;
	int itr;

	int error;

	if(FORCE_STOP_TRAVEL){
		free(path);
		return -1;
	}

	cstack_t *stack = cstack_new();

    if((dp = opendir(ftp->ft_path)) == NULL) {
        fprintf(stderr,"cannot open directory: %s\n", ftp->ft_path);
        return -1;
    }

	(void) memset(&fh, 0, sizeof (fh));

    lstat(ftp->ft_path,&statbuf);


    // trim the tailing '/'
    len = strlen(ftp->ft_path);
    for(itr=len-1;itr>=0;itr--)
    	if(ftp->ft_path[itr]=='/')
    		ftp->ft_path[itr]='\0';
    	else
    		break;

	en.tn_path = NULL;
	en.tn_fh = NULL;
	en.tn_st = NULL;
	pn.tn_path = strdup(ftp->ft_path);
	pn.tn_fh = &fh;
	pn.tn_st = &statbuf;

	error = 0;
    while((entry = readdir(dp)) != NULL) {
    	(void)pthread_yield();
    	if(FORCE_STOP_TRAVEL)
    		break;
    	if(stopOnError && error)
    		break;

        sprintf(path, "%s/%s",ftp->ft_path,entry->d_name);

        lstat(path,&statbuf);

        if(strcmp("..",entry->d_name) == 0)
        	continue;


        if(strcmp(".",entry->d_name) == 0){
           	(void) memset(&fh, 0, sizeof (fh));

           		fh.fh_fid = statbuf.st_ino;
           		fh.fh_fpath = strdup(ftp->ft_path);
           		ndmpd_log(LOG_DEBUG, "doing callback in traverse_level %s with '.'",fh.fh_fpath);

           		en.tn_path = strdup(".");
				en.tn_fh = &fh;
				en.tn_st = &statbuf;


				rv = CALLBACK(&pn, &en);
				if(rv!=0)
					error=1;

				free(fh.fh_fpath);
				free(en.tn_path);


        	continue;
        }

		if(S_ISDIR(statbuf.st_mode)) {
				tmpfs = (fs_traverse_t*)malloc(sizeof(fs_traverse_t));

				tmpfs->ft_path=strdup(path);
//				tmpfs->ft_lpath=strdup(path);

				tmpfs->ft_flags = ftp->ft_flags;
				tmpfs->ft_callbk = ftp->ft_callbk;
				tmpfs->ft_arg = ftp->ft_arg;

                cstack_push(stack,tmpfs,0);
        }else{

        	/*	this is a file, do the callback*/

        	(void) memset(&fh, 0, sizeof (fh));

        	fh.fh_fid = statbuf.st_ino;
        	fh.fh_fpath = strdup(path);

			en.tn_path = strdup(entry->d_name);
			en.tn_fh = &fh;
			en.tn_st = &statbuf;

			ndmpd_log(LOG_DEBUG, "doing callback, parent name=%s, entry=%s",pn.tn_path, en.tn_path);
			rv = CALLBACK(&pn, &en);
			if(rv!=0)
				error=1;

			free(fh.fh_fpath);
			free(en.tn_path);
        }

    }

    closedir(dp);
    free(pn.tn_path);
    free(path);

	while(cstack_pop(stack, (void **)&tmpftp, 0)>=0){
		(void)pthread_yield();
		traverse_level(tmpftp, stopOnError);
		free(tmpftp->ft_path);
//		free(tmpftp->ft_lpath);
		free(tmpftp);
	}


	cstack_delete(stack);


    return 0;
}
