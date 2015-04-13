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

#include <ndmpd_tar_v3.h>

#include <ndmp.h>
#include <ndmpd.h>
#include <ndmpd_session.h>
#include <ndmpd_util.h>
#include <ndmpd_func.h>
#include <ndmpd_fhistory.h>
#include <ndmpd_snapshot.h>
#include <handler.h>

#include <tlm_buffers.h>
#include <tlm.h>
#include <tlm_util.h>

#include <tlm_lib.h>

/* open	*/
#include  <fcntl.h>
/* close */
#include <unistd.h>

#include <signal.h>
#include <sys/acl.h>
#include <assert.h>

/* getmntinfo */
#include <sys/mount.h>

extern int ndmp_force_bk_dirs;
extern bool_t ndmp_ignore_ctime;
extern bool_t ndmp_include_lmtime;
extern tm_ops_t tm_tar_ops;
extern int ndmpd_put_dumptime(char *path, int level, time_t ddate);
extern int ndmpd_get_dumptime(char *path, int *level, time_t *ddate);

/*
 * Maximum length of the string-representation of u_longlong_t type.
 */
#define	QUAD_DECIMAL_LEN	20

/* IS 'Y' OR "T' */
#define	IS_YORT(c)	(strchr("YT", toupper(c)))

/*
 * If path is defined.
 */
#define	ISDEFINED(cp)	((cp) && *(cp))
#define	SHOULD_LBRBK(bpp)	(!((bpp)->bp_opr & TLM_OP_CHOOSE_ARCHIVE))

/*
 * Component boundary means end of path or on a '/'.  At this
 * point both paths should be on component boundary.
 */
#define	COMPBNDRY(p)	(!*(p) || (*p) == '/')

typedef struct bk_param_v3 {
	ndmpd_session_t *bp_session;
	ndmp_lbr_params_t *bp_nlp;
	tlm_job_stats_t *bp_js;
	tlm_cmd_t *bp_lcmd;
	tlm_commands_t *bp_cmds;
	tlm_acls_t *bp_tlmacl;
	int bp_opr;
	char *bp_tmp;
	char *bp_chkpnm;
	char **bp_excls;
	char *bp_unchkpnm;
} bk_param_v3_t;

/*
 * Multiple destination restore mode
 */
#define	MULTIPLE_DEST_DIRS 128

int multiple_dest_restore = 0;

/*
 * split_env
 *
 * Splits the string into list of sections separated by the
 * sep character.
 *
 * Parameters:
 *   envp (input) - the environment variable that should be broken
 *   sep (input) - the separator character
 *
 * Returns:
 *   Array of character pointers: On success.  The array is allocated
 *	as well as all its entries.  They all should be freed by the
 *	caller.
 *   NULL: on error
 */
static char **
split_env(char *envp, char sep)
{
	char *bp, *cp, *ep;
	char *save;
	char **cpp;
	int n;

	if (!envp)
		return (NULL);

	while (isspace(*envp))
		envp++;

	if (!*envp)
		return (NULL);

	bp = save = strdup(envp);
	if (!bp)
		return (NULL);

	/*
	 * Since the env variable is not empty, it contains at least one
	 * component
	 */
	n = 1;
	while ((cp = strchr(bp, sep))) {
		if (cp > save && *(cp-1) != '\\')
			n++;

		bp = cp + 1;
	}

	n++; /* for the terminating NULL pointer */
	cpp = ndmp_malloc(sizeof (char *) * n);
	if (!cpp) {
		free(save);
		return (NULL);
	}

	(void) memset(cpp, 0, n * sizeof (char *));
	n = 0;
	cp = bp = ep = save;
	while (*cp)
		if (*cp == sep) {
			*ep = '\0';
			if (strlen(bp) > 0) {
				cpp[n] = strdup(bp);
				if (!cpp[n++]) {
					tlm_release_list(cpp);
					cpp = NULL;
					break;
				}
			}
			ep = bp = ++cp;
		} else if (*cp == '\\') {
			++cp;
			if (*cp == 'n') {	/* "\n" */
				*ep++ = '\n';
				cp++;
			} else if (*cp == 't') {	/* "\t" */
				*ep++ = '\t';
				cp++;
			} else
				*ep++ = *cp++;
		} else
			*ep++ = *cp++;

	*ep = '\0';
	if (cpp) {
		if (strlen(bp) > 0) {
			cpp[n] = strdup(bp);
			if (!cpp[n++]) {
				tlm_release_list(cpp);
				cpp = NULL;
			} else
				cpp[n] = NULL;
		}

		if (n == 0 && cpp != NULL) {
			tlm_release_list(cpp);
			cpp = NULL;
		}
	}

	free(save);
	return (cpp);
}

/*
 * prl
 *
 * Print the array of character pointers passed to it.  This is
 * used for debugging purpose.
 *
 * Parameters:
 *   lpp (input) - pointer to the array of strings
 *
 * Returns:
 *   void
 */
static void
prl(char **lpp)
{
	if (!lpp) {
		ndmpd_log(LOG_DEBUG, "empty");
		return;
	}
	while (*lpp)
		ndmpd_log(LOG_DEBUG, "\"%s\"", *lpp++);
}

/*
 * inlist
 *
 * Looks through all the strings of the array to see if the ent
 * matches any of the strings.  The strings are patterns.
 *
 * Parameters:
 *   lpp (input) - pointer to the array of strings
 *   ent (input) - the entry to be matched
 *
 * Returns:
 *   TRUE: if there is a match
 *   FALSE: invalid argument or no match
 */
static bool_t
inlist(char **lpp, char *ent)
{
	if (!lpp || !ent) {
		ndmpd_log(LOG_DEBUG, "empty list");
		return (FALSE);
	}

	while (*lpp) {
		/*
		 * Fixing the sync_sort NDMPV3 problem, it sends the inclusion
		 * like "./" which we should skip the "./"
		 */
		char *pattern = *lpp;
		if (strncmp(pattern, "./", 2) == 0)
			pattern += 2;

		ndmpd_log(LOG_DEBUG, "pattern %s, ent %s", pattern, ent);

		if (match(pattern, ent)) {
			ndmpd_log(LOG_DEBUG, "match(%s,%s)", pattern, ent);
			return (TRUE);
		}
		lpp++;
	}

	ndmpd_log(LOG_DEBUG, "no match");
	return (FALSE);
}

/*
 * ininc
 *
 * Checks if the entry is in the list.  This is used for inclusion
 * list.  If the inclusion list is empty, TRUE should be returned
 * showing that everything should be included by default.
 *
 * Parameters:
 *   lpp (input) - pointer to the array of strings
 *   ent (input) - the entry to be matched
 *
 * Returns:
 *   TRUE: if there is a match or the list is empty
 *   FALSE: no match
 */
static bool_t
ininc(char **lpp, char *ent)
{
	if (!lpp || !ent || !*ent)
		return (TRUE);

	return (inlist(lpp, ent));
}

/*
 * setupsels
 *
 * Set up the selection list for Local B/R functions.  A new array of
 * "char *" is created and the pointers point to the original paths of
 * the Nlist.
 *
 * Parameters:
 *   session (input) - pointer to the session
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *   index(input) - If not zero is the DAR entry position
 *
 * Returns:
 *   list pointer: on success
 *   NULL: on error
 */
/*ARGSUSED*/
char **
setupsels(ndmpd_session_t *session, ndmpd_module_params_t *params,
    ndmp_lbr_params_t *nlp, int index)
{
	char **lpp, **save;
	int i, n;
	int len;
	int start, end;
	mem_ndmp_name_v3_t *ep;

	n = session->ns_data.dd_nlist_len;

	save = lpp = ndmp_malloc(sizeof (char *) * (n + 1));
	if (!lpp) {
		MOD_LOGV3(params, NDMP_LOG_ERROR, "Insufficient memory.\n");
		return (NULL);
	}

	if (index) { /* DAR, just one entry */
		/*
		 * We have to setup a list of strings that will not match any
		 * file. One DAR entry will be added in the right position later
		 * in this function.
		 * When the match is called from tar_getdir the
		 * location of the selection that matches the entry is
		 * important
		 */
		for (i = 0; i < n; ++i)
			*(lpp+i) = " ";
		n = 1;
		start = index-1;
		end = start+1;
		lpp += start; /* Next selection entry will be in lpp[start] */
	} else {
		start = 0;
		end = n;
	}

	for (i = start; i < end; i++) {
		ep = (mem_ndmp_name_v3_t *)MOD_GETNAME(params, i);
		if (!ep)
			continue;

		/*
		 * Check for clients that send original path as "."(like
		 * CA products). In this situation opath is something like
		 * "/v1/." and we should change it to "/v1/"
		 */
		len = strlen(ep->nm3_opath);
		if (len > 1 && ep->nm3_opath[len-2] == '/' &&
		    ep->nm3_opath[len-1] == '.') {
			ep->nm3_opath[len-1] = '\0';
			ndmpd_log(LOG_DEBUG,
			    "nm3_opath changed from %s. to %s",
			    ep->nm3_opath, ep->nm3_opath);
		}
		*lpp++ = ep->nm3_opath;
	}

	/* list termination indicator is a null pointer */
	*lpp = NULL;

	return (save);
}

/*
 * mkrsp
 *
 * Make Restore Path.
 * It gets a path, a selection (with which the path has matched) a new
 * name and makes a new name for the path.
 * All the components of the path and the selection are skipped as long
 * as they are the same.  If either of the path or selection are not on
 * a component boundary, the match was reported falsefully and no new name
 * is generated(Except the situation in which both path and selection
 * end with trailing '/' and selection is the prefix of the path).
 * Otherwise, the remaining of the path is appended to the
 * new name.  The result is saved in the buffer passed.
 *
 * Parameters:
 *   bp (output) - pointer to the result buffer
 *   pp (input) - pointer to the path
 *   sp (input) - pointer to the selection
 *   np (input) - pointer to the new name
 *
 * Returns:
 *   pointer to the bp: on success
 *   NULL: otherwise
 */
char *
mkrsp(char *bp, char *pp, char *sp, char *np)
{
	if (!bp || !pp)
		return (NULL);

	ndmpd_log(LOG_DEBUG, "mkrsp pp=%s,sp=%s,np=%s",pp,sp,np);

	pp += strspn(pp, "/");
	if (sp) {
		sp += strspn(sp, "/");

		/* skip as much as match */
		while (*sp && *pp && *sp == *pp) {
			sp++;
			pp++;
		}

		if (!COMPBNDRY(pp) || !COMPBNDRY(sp))
			/* An exception to the boundary rule */
			/* (!(!*sp && (*(pp - 1)) == '/')) */
			if (*sp || (*(pp - 1)) != '/')
				return (NULL);

		/* if pp shorter than sp, it should not be restored */
		if (!*pp && *sp) {
			sp += strspn(sp, "/");
			if (strlen(sp) > 0)
				return (NULL);
		}
	}

	if (!np)
		np = "/";

	ndmpd_log(LOG_ERR, "Restore path %s/%s.", np, pp);

	if (!tlm_cat_path(bp, np, pp)) {
		ndmpd_log(LOG_ERR, "Restore path too long %s/%s.", np, pp);
		return (NULL);
	}

	return (bp);
}

/*
 * mknewname
 *
 * This is used as callback for creating the restore path. This function
 * can handle both single destination and multiple restore paths.
 *
 * Make up the restore destination path for a particular file/directory, path,
 * based on nm3_opath and nm3_dpath.  path should have matched nm3_opath
 * in some way.
 */
char *
mknewname(const struct rs_name_maker *rnp, char *buf, int idx, char *path)
{
	char *rv;
	ndmp_lbr_params_t *nlp;
	mem_ndmp_name_v3_t *ep;
	ndmpd_log(LOG_DEBUG, "mknewname");
	rv = NULL;
	if (!buf) {
		ndmpd_log(LOG_DEBUG, "buf is NULL");
	} else if (!path) {
		ndmpd_log(LOG_DEBUG, "path is NULL");
	} else if ((nlp = rnp->rn_nlp) == 0) {
		ndmpd_log(LOG_DEBUG, "rnp->rn_nlp is NULL");
	} else if (!nlp->nlp_params) {
		ndmpd_log(LOG_DEBUG, "nlp->nlp_params is NULL");
	} else
		if (!ndmp_full_restore_path) {
			ndmpd_log(LOG_DEBUG,"let's try partial ");

			if (idx < 0 || idx >= (int)nlp->nlp_nfiles) {
				ndmpd_log(LOG_DEBUG,
				    "Invalid idx %d range (0, %ld)",
				    idx, nlp->nlp_nfiles);
			} else if (!(ep = (mem_ndmp_name_v3_t *)MOD_GETNAME(
			    nlp->nlp_params, idx))) {
				ndmpd_log(LOG_DEBUG,
				    "nlist entry %d is NULL", idx);
			} else {
				rv = mkrsp(buf, path, ep->nm3_opath,
				    ep->nm3_dpath);

				ndmpd_log(LOG_DEBUG,
				    "idx %d org \"%s\" dst \"%s\"",
				    idx, ep->nm3_opath, ep->nm3_dpath);
				if (rv) {
					ndmpd_log(LOG_DEBUG,
					    "path \"%s\": \"%s\"", path, rv);
				} else {
					ndmpd_log(LOG_DEBUG,
					    "path \"%s\": NULL", path);
				}
			}
		} else {
			if (!tlm_cat_path(buf, nlp->nlp_restore_path, path)) {
				ndmpd_log(LOG_ERR, "Path too long %s/%s.",
				    nlp->nlp_restore_path, path);
				rv = NULL;
			} else {
				rv = buf;
				ndmpd_log(LOG_DEBUG,
				    "path \"%s\": \"%s\"", path, rv);
			}
		}

	return (rv);
}

/*
 * chopslash
 *
 * Remove the slash from the end of the given path
 */
static void
chopslash(char *cp)
{
	int ln;

	if (!cp || !*cp)
		return;

	ln = strlen(cp);
	cp += ln - 1; /* end of the string */
	while (ln > 0 && *cp == '/') {
		*cp-- = '\0';
		ln--;
	}
}

/*
 * joinpath
 *
 * Join two given paths
 */
static char *
joinpath(char *bp, char *pp, char *np)
{
	if (pp && *pp) {
		if (np && *np)
			(void) tlm_cat_path(bp, pp, np);
		else
			(void) strlcpy(bp, pp, TLM_MAX_PATH_NAME);
	} else {
		if (np && *np)
			(void) strlcpy(bp, np, TLM_MAX_PATH_NAME);
		else
			bp = NULL;
	}

	return (bp);
}

/*
 * voliswr
 *
 * Is the volume writable?
 */
static int
voliswr(char *path)
{
	ndmpd_log(LOG_DEBUG, "voliswr");
	return 1;
}

/*
 * get_bk_path_v3
 *
 * Get the backup path from the NDMP environment variables.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure.
 *
 * Returns:
 *   The backup path: if anything is specified
 *   NULL: Otherwise
 */
char *
get_bk_path_v3(ndmpd_module_params_t *params)
{
	char *bkpath;
	int len;
	int itr;

	ndmpd_log(LOG_DEBUG, "get_bk_path_v3");
	bkpath = MOD_GETENV(params, "PREFIX");
	if (!bkpath)
		bkpath = MOD_GETENV(params, "FILESYSTEM");

	if (!bkpath) {
		MOD_LOGV3(params, NDMP_LOG_ERROR,
				    "Backup path not defined.\n");
	} else {
		ndmpd_log(LOG_DEBUG, "bkpath: \"%s\"", bkpath);
	}

	// remove the tailing "/"
	len = strlen(bkpath);
	for(itr=len-1;itr>=0;itr--)
		if(bkpath[itr]=='/')
			bkpath[itr]='\0';
		else
    			break;
	return (bkpath);
}

/*
 * is_valid_backup_dir_v3
 *
 * Checks the validity of the backup path.  Backup path should
 * have the following characteristics to be valid:
 *	1) It should be an absolute path.
 *	2) It should be a directory.
 *	3) It should not be checkpoint root directory
 *	4) If the file system is read-only, the backup path
 *	    should be a checkpointed path.  Checkpoint cannot
 *	    be created on a read-only file system.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure.
 *   bkpath (input) - the backup path
 *
 * Returns:
 *   TRUE: if everything's OK
 *   FALSE: otherwise.
 */
static bool_t
is_valid_backup_dir_v3(ndmpd_module_params_t *params, char *bkpath)
{
	char *msg;
	struct stat st;

	if (*bkpath != '/') {
		MOD_LOGV3(params, NDMP_LOG_ERROR,
			"Relative backup path not allowed \"%s\".\n", bkpath);

		return (FALSE);
	}
	if (stat(bkpath, &st) < 0) {
		msg = strerror(errno);
		MOD_LOGV3(params, NDMP_LOG_ERROR, "\"%s\" %s.\n", bkpath, msg);

		return (FALSE);
	}
	if (!S_ISDIR(st.st_mode)) {
		/* only directories can be specified as the backup path */
		MOD_LOGV3(params, NDMP_LOG_ERROR, "\"%s\" is not a directory.\n", bkpath);

		return (FALSE);
	}

	return (TRUE);
}

/*
 * log_level_v3
 *
 * Log the backup level and date of the last and the current
 * backup for level-type backup in the system log and also
 * send them as normal log to the client.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   void
 */
static void
log_level_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp)
{
	MOD_LOGV3(params, NDMP_LOG_NORMAL,
	    "Date of the last level '%u': %s.\n", nlp->nlp_llevel,
	    cctime(&nlp->nlp_ldate));

	MOD_LOGV3(params, NDMP_LOG_NORMAL,
	    "Date of this level '%u': %s.\n", nlp->nlp_clevel,
	    cctime(&nlp->nlp_cdate));

	MOD_LOGV3(params, NDMP_LOG_NORMAL, "Update: %s.\n",
	    NDMP_TORF(NLP_ISSET(nlp, NLPF_UPDATE)));
}

/*
 * log_bk_params_v3
 *
 * Dispatcher function which calls the appropriate function
 * for logging the backup date and level in the system log
 * and also send them as normal log message to the client.
 *
 * Parameters:
 *   session (input) - pointer to the session
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   void
 */
static void
log_bk_params_v3(ndmpd_session_t *session, ndmpd_module_params_t *params,
    ndmp_lbr_params_t *nlp)
{
	MOD_LOGV3(params, NDMP_LOG_NORMAL, "Backing up \"%s\".\n",
	    nlp->nlp_backup_path);

	if (session->ns_mover.md_data_addr.addr_type == NDMP_ADDR_LOCAL)
		MOD_LOGV3(params, NDMP_LOG_NORMAL,
		    "Tape record size: %d.\n",
		    session->ns_mover.md_record_size);

	MOD_LOGV3(params, NDMP_LOG_NORMAL, "File history: %c.\n",
	    NDMP_YORN(NLP_ISSET(nlp, NLPF_FH)));

	if (NLP_ISSET(nlp, NLPF_LEVELBK))
		log_level_v3(params, nlp);
	else {
		MOD_LOGV3(params, NDMP_LOG_ERROR,
		    "Internal error: backup level not defined for \"%s\".\n",
		    nlp->nlp_backup_path);
	}
}

/*
 * get_update_env_v3
 *
 * Is the UPDATE environment variable specified?  If it is
 * the corresponding flag is set in the flags field of the
 * nlp structure, otherwise the flag is cleared.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   void
 */
static void
get_update_env_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp)
{
	char *envp;

	envp = MOD_GETENV(params, "UPDATE");
	if (!envp) {
		NLP_SET(nlp, NLPF_UPDATE);
		ndmpd_log(LOG_DEBUG,
		    "env(UPDATE) not defined, default to TRUE");
	} else {
		ndmpd_log(LOG_DEBUG, "env(UPDATE): \"%s\"", envp);
		if (IS_YORT(*envp))
			NLP_SET(nlp, NLPF_UPDATE);
		else
			NLP_UNSET(nlp, NLPF_UPDATE);
	}
}

/*
 * get_hist_env_v3
 *
 * Is backup history requested?  If it is, the corresponding
 * flag is set in the flags field of the nlp structure, otherwise
 * the flag is cleared.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   void
 */
static void
get_hist_env_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp)
{
	char *envp;

	envp = MOD_GETENV(params, "HIST");
	if (!envp) {
		ndmpd_log(LOG_DEBUG, "env(HIST) not defined, default to N");
		NLP_UNSET(nlp, NLPF_FH);
	} else {
		ndmpd_log(LOG_DEBUG, "env(HIST): \"%s\"", envp);
		if (IS_YORT(*envp))
			NLP_SET(nlp, NLPF_FH);
		else
			NLP_UNSET(nlp, NLPF_FH);
	}
}

/*
 * get_exc_env_v3
 *
 * Gets the EXCLUDE environment variable and breaks it
 * into strings.  The separator of the EXCLUDE environment
 * variable is the ',' character.
 *
 * For QNAP version, we change this to match EMC behavior.
 * Exclude will be sperate to exclude file and folder.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   void
 */
static void
get_exc_env_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp)
{
	char *envp;
	char env_parameter[13]; // QNAP_EFILE99, QNAP_EDIR99
	char tmpenv[1024];

	/*
	 * we support only 99 exclude file name patterns
	 * */
	int idx,max_count=100, exclude_count=0;


	for(idx=1,exclude_count=0;idx<max_count;idx++){
#ifdef EMC_MODEL
		sprintf(env_parameter,"EMC_EFILE%02d",idx);
#else
		sprintf(env_parameter,"QNAP_EFILE%02d",idx);
#endif
		envp = MOD_GETENV(params, env_parameter);
		if(envp)
			exclude_count++;

#ifdef EMC_MODEL
		sprintf(env_parameter,"EMC_EDIR%02d",idx);
#else
		sprintf(env_parameter,"QNAP_EDIR%02d",idx);
#endif
		envp = MOD_GETENV(params, env_parameter);
		if(envp)
			exclude_count++;
	}

	exclude_count+=1; // and an end directive.

	nlp->nlp_exl=malloc(sizeof(char*)*exclude_count);

	for(idx=1,exclude_count=0;idx<max_count;idx++){
#ifdef EMC_MODEL
		sprintf(env_parameter,"EMC_EFILE%02d",idx);
#else
		sprintf(env_parameter,"QNAP_EFILE%02d",idx);
#endif
		envp = MOD_GETENV(params, env_parameter);
		if(envp){
			sprintf(tmpenv, "f_%s",envp);
			nlp->nlp_exl[exclude_count]=strdup(tmpenv);
			exclude_count++;
		}

#ifdef EMC_MODEL
		sprintf(env_parameter,"EMC_EDIR%02d",idx);
#else
		sprintf(env_parameter,"QNAP_EDIR%02d",idx);
#endif
		envp = MOD_GETENV(params, env_parameter);
		if(envp){
			sprintf(tmpenv, "d_%s",envp);
			nlp->nlp_exl[exclude_count]=strdup(tmpenv);
			exclude_count++;
		}
	}

	nlp->nlp_exl[exclude_count]=NULL;
	prl(nlp->nlp_exl);

}

/*
 * get_inc_env_v3
 *
 * Gets the FILES environment variable that shows which files
 * should be backed up, and breaks it into strings.  The
 * separator of the FILES environment variable is the space
 * character.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   void
 */
static void
get_inc_env_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp)
{
	char *envp;

	envp = MOD_GETENV(params, "FILES");
	if (!envp) {
		ndmpd_log(LOG_DEBUG, "env(FILES) not defined, default to \"\"");
		nlp->nlp_inc = NULL;
	} else {
		ndmpd_log(LOG_DEBUG, "env(FILES): \"%s\"", envp);
		nlp->nlp_inc = split_env(envp, ',');
		prl(nlp->nlp_inc);
	}
}

/*
 * get_snap_env_v3
 *
 * Gets the SNAPSHOT environment variable.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   void
 */
static void
get_snap_env_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp)
{
	char *envp;

	envp = MOD_GETENV(params, "SNAPSURE");
	if (!envp) {
		ndmpd_log(LOG_DEBUG, "env(SNAPSURE) not defined, default to N");
		NLP_UNSET(nlp, NLPF_SNAP);
	} else {
		ndmpd_log(LOG_DEBUG, "env(SNAPSURE): \"%s\"", envp);
		ndmpd_log(LOG_DEBUG, "Do not support SNAPSURE on this machine", envp);
		NLP_UNSET(nlp, NLPF_SNAP);
	}
}

/*
 * get_backup_level_v3
 *
 * Gets the backup level from the environment variables.  If
 * BASE_DATE is specified, it will be used, otherwise LEVEL
 * will be used.  If neither is specified, LEVEL = '0' is
 * assumed.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   NDMP_NO_ERR: on success
 *   != NDMP_NO_ERR: Otherwise
 */
static ndmp_error
get_backup_level_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp)
{
	char *envp;
	ndmp_error rv;

	/*
	 * If the BASE_DATE env variable is specified use it, otherwise
	 * look to see if LEVEL is specified.  If LEVEL is not
	 * specified either, backup level '0' must be made. Level backup
	 * does not clear the archive bit.
	 *
	 * If LEVEL environment varaible is specified, values for
	 * 'F', 'D', 'I' and 'A' (for 'Full', 'Differential',
	 * 'Incremental', and 'Archive' is checked first.  Then
	 * level '0' to '9' will be checked.
	 *
	 * LEVEL environment variable can hold only one character.
	 * If its length is longer than 1, an error is returned.
	 */

	// we will just use level backup.
	envp = MOD_GETENV(params, "LEVEL");
	if (!envp) {
		ndmpd_log(LOG_DEBUG, "env(LEVEL) not defined, default to 0");
		NLP_SET(nlp, NLPF_LEVELBK);
		nlp->nlp_llevel = 0;
		nlp->nlp_ldate = 0;
		nlp->nlp_clevel = 0;
		/*
		 * The value of nlp_cdate will be set to the checkpoint
		 * creation time after it is created.
		 */
		return (NDMP_NO_ERR);
	}

	if (*(envp+1) != '\0') {
		MOD_LOGV3(params, NDMP_LOG_ERROR,
		    "Invalid backup level \"%s\".\n", envp);
		return (NDMP_ILLEGAL_ARGS_ERR);
	}

	if (!isdigit(*envp)) {
		MOD_LOGV3(params, NDMP_LOG_ERROR,
		    "Invalid backup level \"%s\".\n", envp);
		return (NDMP_ILLEGAL_ARGS_ERR);
	}

	NLP_SET(nlp, NLPF_LEVELBK);
	nlp->nlp_llevel = *envp - '0';
	nlp->nlp_ldate = 0;
	nlp->nlp_clevel = nlp->nlp_llevel;
	/*
	 * The value of nlp_cdate will be set to the checkpoint
	 * creation time after it is created.
	 */
	if (ndmpd_get_dumptime(nlp->nlp_backup_path, &nlp->nlp_llevel,
	    &nlp->nlp_ldate) < 0) {
		MOD_LOGV3(params, NDMP_LOG_ERROR,
		    "Getting dumpdates for %s level '%c'.\n",
		    nlp->nlp_backup_path, *envp);
		return (NDMP_NO_MEM_ERR);
	} else {
		get_update_env_v3(params, nlp);
		rv = NDMP_NO_ERR;
	}

	return (rv);
}

/*
 * save_level_v3
 *
 * Save the date and level of the current backup in the dumpdates
 * file.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   void
 */
static void
save_level_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp)
{
	if (!params || !nlp)
		return;

	if (!NLP_SHOULD_UPDATE(nlp)) {
		ndmpd_log(LOG_DEBUG, "update not requested");
	} else if (ndmpd_put_dumptime(nlp->nlp_backup_path, nlp->nlp_clevel,
	    nlp->nlp_cdate) < 0) {
		MOD_LOGV3(params, NDMP_LOG_ERROR, "Logging backup date.\n");
	}
}

/*
 * save_backup_date_v3
 *
 * A dispatcher function to call the corresponding save function
 * based on the backup type.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   void
 */
static void
save_backup_date_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp)
{
	ndmpd_log(LOG_DEBUG, "save_backup_date_v3 ");
	if (!params || !nlp)
		return;

	if (NLP_ISSET(nlp, NLPF_LEVELBK))
		save_level_v3(params, nlp);
	else {
		MOD_LOGV3(params, NDMP_LOG_ERROR,
		    "Internal error: lost backup level type for \"%s\".\n",
		    nlp->nlp_backup_path);
	}
}

/*
 * backup_alloc_structs_v3
 *
 * Create the structures for V3 backup.  This includes:
 *	Job stats
 *	Reader writer IPC
 *	File history callback structure
 *
 * Parameters:
 *   session (input) - pointer to the session
 *   jname (input) - name assigned to the current backup for
 *	job stats strucure
 *
 * Returns:
 *   0: on success
 *   -1: otherwise
 */
static int
backup_alloc_structs_v3(ndmpd_session_t *session, char *jname)
{
	int n;
	long xfer_size;
	ndmp_lbr_params_t *nlp;
	tlm_commands_t *cmds;

	nlp = ndmp_get_nlp(session);
	if (!nlp) {
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
		return (-1);
	}

	nlp->nlp_jstat = tlm_new_job_stats(jname);
	if (!nlp->nlp_jstat) {
		ndmpd_log(LOG_DEBUG, "Creating job stats");
		return (-1);
	}

	cmds = &nlp->nlp_cmds;
	(void) memset(cmds, 0, sizeof (*cmds));

	xfer_size = ndmp_buffer_get_size(session);
	if (xfer_size < 512*KB) {
		/*
		 * Read multiple of mover_record_size near to 512K.  This
		 * will prevent the data being copied in the mover buffer
		 * when we write the data.
		 */
		n = 512 * KB / xfer_size;
		if (n <= 0)
			n = 1;
		xfer_size *= n;
		ndmpd_log(LOG_DEBUG, "Adjusted read size: %ld",
		    xfer_size);
	}

	cmds->tcs_command = tlm_create_reader_writer_ipc(TRUE, xfer_size);
	if (!cmds->tcs_command) {
		tlm_un_ref_job_stats(jname);
		return (-1);
	}

	nlp->nlp_logcallbacks = lbrlog_callbacks_init(session,
	    ndmpd_fhpath_v3_cb, ndmpd_fhdir_v3_cb, ndmpd_fhnode_v3_cb);

	if (!nlp->nlp_logcallbacks) {
		tlm_release_reader_writer_ipc(cmds->tcs_command);
		tlm_un_ref_job_stats(jname);
		return (-1);
	}
	nlp->nlp_jstat->js_callbacks = (void *)(nlp->nlp_logcallbacks);
	nlp->nlp_restored = NULL;
	ndmpd_log(LOG_DEBUG, "going out backup_alloc_structs_v3");
	return (0);
}

/*
 * restore_alloc_structs_v3
 *
 * Create the structures for V3 Restore.  This includes:
 *	Job stats
 *	Reader writer IPC
 *	File recovery callback structure
 *
 * Parameters:
 *   session (input) - pointer to the session
 *   jname (input) - name assigned to the current backup for
 *	job stats strucure
 *
 * Returns:
 *   0: on success
 *   -1: otherwise
 */
int
restore_alloc_structs_v3(ndmpd_session_t *session, char *jname)
{
	long xfer_size;
	ndmp_lbr_params_t *nlp;
	tlm_commands_t *cmds;

	nlp = ndmp_get_nlp(session);
	if (!nlp) {
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
		return (-1);
	}

	/* this is used in ndmpd_path_restored_v3() */
	nlp->nlp_lastidx = -1;

	nlp->nlp_jstat = tlm_new_job_stats(jname);
	if (!nlp->nlp_jstat) {
		ndmpd_log(LOG_DEBUG, "Creating job stats");
		return (-1);
	}

	cmds = &nlp->nlp_cmds;
	(void) memset(cmds, 0, sizeof (*cmds));

	xfer_size = ndmp_buffer_get_size(session);
	cmds->tcs_command = tlm_create_reader_writer_ipc(FALSE, xfer_size);
	if (!cmds->tcs_command) {
		tlm_un_ref_job_stats(jname);
		return (-1);
	}

	nlp->nlp_logcallbacks = lbrlog_callbacks_init(session,
	    ndmpd_path_restored_v3, NULL, NULL);
	if (!nlp->nlp_logcallbacks) {
		tlm_release_reader_writer_ipc(cmds->tcs_command);
		tlm_un_ref_job_stats(jname);
		return (-1);
	}
	nlp->nlp_jstat->js_callbacks = (void *)(nlp->nlp_logcallbacks);

	return (0);
}

/*
 * free_structs_v3
 *
 * Release the resources allocated by backup_alloc_structs_v3
 * function.
 *
 * Parameters:
 *   session (input) - pointer to the session
 *   jname (input) - name assigned to the current backup for
 *	job stats strucure
 *
 * Returns:
 *   void
 */
/*ARGSUSED*/
static void
free_structs_v3(ndmpd_session_t *session, char *jname)
{
	ndmp_lbr_params_t *nlp;
	tlm_commands_t *cmds;

	nlp = ndmp_get_nlp(session);
	if (!nlp) {
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
		return;
	}
	cmds = &nlp->nlp_cmds;
	if (!cmds) {
		ndmpd_log(LOG_DEBUG, "cmds == NULL");
	}

	if (nlp->nlp_logcallbacks) {
		lbrlog_callbacks_done(nlp->nlp_logcallbacks);
		nlp->nlp_logcallbacks = NULL;
	} else
		ndmpd_log(LOG_DEBUG, "FH CALLBACKS == NULL");

	if (cmds->tcs_command) {
		if (cmds->tcs_command->tc_buffers != NULL)
			tlm_release_reader_writer_ipc(cmds->tcs_command);
		else
			ndmpd_log(LOG_DEBUG, "BUFFERS == NULL");
		cmds->tcs_command = NULL;
	} else
		ndmpd_log(LOG_DEBUG, "COMMAND == NULL");

}

/*
 * Dump the memory value in HEX.
 */
void 
mem_dump(const char *value, size_t size)
{
	static char *encoded;
	static size_t encoded_size;
	int n;
	printf("data\n");

	static const char *digits = "0123456789abcdef";

	for (n = 0; n < size; n++, value++) {
		printf("%c",digits[((unsigned char)*value >> 4)]);
		printf("%c",digits[((unsigned char)*value & 0x0F)]);
	}
	printf("\nend\n");
}

/*
 * backup_dirv3
 *
 * Backup a directory and update the bytes processed field of the
 * data server.
 *
 * Parameters:
 *   bpp (input) - pointer to the backup parameters structure
 *   pnp (input) - pointer to the path node
 *   enp (input) - pointer to the entry node
 *
 * Returns:
 *   0: on success
 *   != 0: otherwise
 */
static int
backup_dirv3(bk_param_v3_t *bpp, fst_node_t *pnp,
    fst_node_t *enp)
{
	longlong_t apos, bpos;
	acl_t acl = NULL;
	char *acltp = NULL;
	struct stat st;
	char fullpath[TLM_MAX_PATH_NAME];
	char *p;

	ndmpd_log(LOG_DEBUG, "***********backup_dirv3***************");

	if (!bpp || !pnp || !enp) {
		ndmpd_log(LOG_DEBUG, "Invalid argument");
		return (-1);
	}

	ndmpd_log(LOG_DEBUG, "d(%s)", bpp->bp_tmp);

	if (lstat(bpp->bp_tmp, &st) != 0)
		return (0);

	acl = acl_get_file(bpp->bp_tmp, ACL_TYPE_NFS4);
     	int acl_len=0;
     	int xattr_len=0;
     	int acl_all_len=0;

	if (acl && (acltp = acl_to_text(acl, 0)) != NULL)
		acl_len= strlen(acltp);
	if (acl != NULL)
		acl_free(acl);

	bpp->bp_tlmacl->acl_info.attr_len = acl_len;
	bpp->bp_tlmacl->acl_info.attr_info = NULL;

	acl_all_len =acl_len+xattr_len;
	if (acl_all_len > 0) {
		bpp->bp_tlmacl->acl_info.attr_info = (char*)malloc(acl_all_len);
		if (bpp->bp_tlmacl->acl_info.attr_info != NULL) {
			if (acl_len > 0)
				(void) strlcpy(bpp->bp_tlmacl->acl_info.attr_info,acltp, acl_len);
		}
	}

	if(acltp!=NULL)
		acl_free(acltp);

	bpos = tlm_get_data_offset(bpp->bp_lcmd);
	p = bpp->bp_tmp + strlen(bpp->bp_chkpnm);
	if (*p == '/')
		(void) snprintf(fullpath, TLM_MAX_PATH_NAME, "%s%s",
		    bpp->bp_unchkpnm, p);
	else
		(void) snprintf(fullpath, TLM_MAX_PATH_NAME, "%s/%s",
		    bpp->bp_unchkpnm, p);

	if (tm_tar_ops.tm_putdir != NULL)
		(void) (tm_tar_ops.tm_putdir)(fullpath, bpp->bp_tlmacl,
		    bpp->bp_lcmd, bpp->bp_js);

	apos = tlm_get_data_offset(bpp->bp_lcmd);
	bpp->bp_session->ns_data.dd_module.dm_stats.ms_bytes_processed +=
	    apos - bpos;

	return (0);
}

/*
 * backup_filev3
 *
 * Backup a file and update the bytes processed field of the
 * data server.
 *
 * Parameters:
 *   bpp (input) - pointer to the backup parameters structure
 *   pnp (input) - pointer to the path node
 *   enp (input) - pointer to the entry node
 *
 * Returns:
 *   0: on success
 *   != 0: otherwise
 */
static int
backup_filev3(bk_param_v3_t *bpp, fst_node_t *pnp,
    fst_node_t *enp)
{
	ndmpd_log(LOG_DEBUG, "***********backup_filev3***************");

	char *ent;
	longlong_t rv;
	longlong_t apos, bpos;
	acl_t acl = NULL;
	char *acltp=NULL;
	struct stat st;
	char fullpath[TLM_MAX_PATH_NAME];
	char *p;

	if (!bpp || !pnp || !enp) {
		ndmpd_log(LOG_DEBUG, "Invalid argument");
		return (-1);
	}

	ndmpd_log(LOG_DEBUG, "f(%s)", bpp->bp_tmp);

	if (lstat(bpp->bp_tmp, &st) != 0)
		return (0);

	if (!S_ISLNK(bpp->bp_tlmacl->acl_attr.st_mode)) {
		acl = acl_get_file(bpp->bp_tmp, ACL_TYPE_NFS4);
	     	int acl_len=0;
	     	int xattr_len=0;
	     	int acl_all_len=0;

		if (acl && (acltp = acl_to_text(acl, 0)) != NULL) {
			acl_len= strlen(acltp);
		}

		if (acl != NULL)
			acl_free(acl);

		bpp->bp_tlmacl->acl_info.attr_len = acl_len;
		bpp->bp_tlmacl->acl_info.attr_info = NULL;

		acl_all_len =acl_len+xattr_len;
		if (acl_all_len > 0) {
			bpp->bp_tlmacl->acl_info.attr_info = (char*)malloc(acl_all_len);
			if (bpp->bp_tlmacl->acl_info.attr_info != NULL) {
				if (acl_len > 0) {
					(void) strlcpy(bpp->bp_tlmacl->acl_info.attr_info,acltp,
						acl_len);
				}
			}
		}
	}

	if(acltp!=NULL)
		acl_free(acltp);

	bpos = tlm_get_data_offset(bpp->bp_lcmd);
	ent = enp->tn_path ? enp->tn_path : "";

	p = pnp->tn_path + strlen(bpp->bp_chkpnm);
	if (*p == '/')
		(void) snprintf(fullpath, TLM_MAX_PATH_NAME, "%s%s",
		    bpp->bp_unchkpnm, p);
	else
		(void) snprintf(fullpath, TLM_MAX_PATH_NAME, "%s/%s",
		    bpp->bp_unchkpnm, p);

	if (tm_tar_ops.tm_putfile != NULL){

		rv = (tm_tar_ops.tm_putfile)(fullpath, ent, pnp->tn_path,
		    bpp->bp_tlmacl, bpp->bp_cmds, bpp->bp_lcmd, bpp->bp_js,
		    bpp->bp_session->hardlink_q);

	}
	apos = tlm_get_data_offset(bpp->bp_lcmd);

	ndmpd_log(LOG_DEBUG, "apos=%llu",apos);
	ndmpd_log(LOG_DEBUG, "bpos=%llu",bpos);

	bpp->bp_session->ns_data.dd_module.dm_stats.ms_bytes_processed +=
	    apos - bpos;

	return (rv < 0 ? rv : 0);
}

/*
 * check_bk_args
 *
 * Check the argument of the bpp.  This is shared function between
 * timebk_v3 and lbrbk_v3 functions.  The checks include:
 *	- The bpp itself.
 *	- If the session pointer of the bpp is valid.
 *	- If the session connection to the DMA is closed.
 *	- If the nlp pointer of the bpp is valid.
 *	- If the backup is aborted.
 *
 * Parameters:
 *   bpp (input) - pointer to the backup parameters structure
 *
 * Returns:
 *   0: if everything's OK
 *   != 0: otherwise
 */
static int
check_bk_args(bk_param_v3_t *bpp)
{
	int rv;

	if (!bpp) {
		rv = -1;
		ndmpd_log(LOG_DEBUG, "Lost bpp");
	} else if (!bpp->bp_session) {
		rv = -1;
		ndmpd_log(LOG_DEBUG, "Session is NULL");
	} else if (bpp->bp_session->ns_eof) {
		rv = -1;
		ndmpd_log(LOG_INFO,
		    "Connection client is closed for backup \"%s\"",
		    bpp->bp_nlp->nlp_backup_path);
	} else if (!bpp->bp_nlp) {
		ndmpd_log(LOG_DEBUG, "Lost nlp");
		return (-1);
	} else if (bpp->bp_session->ns_data.dd_abort) {
		rv = -1;
		ndmpd_log(LOG_INFO, "Backup aborted \"%s\"",
		    bpp->bp_nlp->nlp_backup_path);
	} else
		rv = 0;

	return (rv);
}

/*
 * shouldskip
 *
 * Determines if the current entry should be skipped or it
 * should be backed up.
 *
 * Parameters:
 *   bpp (input) - pointer to the backup parameters structure
 *   pnp (input) - pointer to the path node
 *   enp (input) - pointer to the entry node
 *   errp (output) - pointer to the error value that should be
 *	returned by the caller
 *
 * Returns:
 *   TRUE: if the entry should not be backed up
 *   FALSE: otherwise
 */
static bool_t
shouldskip(bk_param_v3_t *bpp, fst_node_t *pnp,
    fst_node_t *enp, int *errp)
{
	ndmpd_log(LOG_DEBUG, "********************shouldskip************************");

	char *ent;
	bool_t rv;
	struct stat *estp;

	if (!bpp || !pnp || !enp || !errp) {
		ndmpd_log(LOG_DEBUG, "Invalid argument");
		return (TRUE);
	}

	if (!enp->tn_path) {
		ent = "";
		estp = pnp->tn_st;

	} else {
		ent = enp->tn_path;
		estp = enp->tn_st;
	}

	// exclude from client
	if (tlm_is_excluded(pnp->tn_path, ent, bpp->bp_nlp->nlp_exl)) {
		rv = TRUE;
		*errp = S_ISDIR(estp->st_mode) ? FST_SKIP : 0;
		ndmpd_log(LOG_DEBUG, "excl %d \"%s/%s\"", *errp, pnp->tn_path, ent);
	} else if (!S_ISDIR(estp->st_mode) &&
	    !ininc(bpp->bp_nlp->nlp_inc, ent)) {
		rv = TRUE;
		*errp = 0;
		ndmpd_log(LOG_DEBUG, "!in \"%s/%s\"", pnp->tn_path, ent);
	} else
		rv = FALSE;

	ndmpd_log(LOG_DEBUG, "--------------shouldskip--------- path=%s rv=%d",ent,rv);
	return (rv);
}

/*
 * ischngd
 *
 * Check if the object specified should be backed up or not.
 * If stp belongs to a directory and if it is marked in the
 * bitmap vector, it shows that either the directory itself is
 * modified or there is something below it that will be backed
 * up.
 *
 * By setting ndmp_force_bk_dirs global variable to a non-zero
 * value, directories are backed up anyways.
 *
 * Backing up the directories unconditionally helps
 * restoring the metadata of directories as well, when one
 * of the objects below them are being restored.
 *
 * For non-directory objects, if the modification or change
 * time of the object is after the date specified by the
 * bk_selector_t, the the object must be backed up.
 */
static bool_t
ischngd(struct stat *stp, time_t t, ndmp_lbr_params_t *nlp)
{

	ndmpd_log(LOG_DEBUG, "********************ischngd************************");

	bool_t rv;

	if (!stp) {
		rv = FALSE;
		ndmpd_log(LOG_DEBUG, "stp is NULL");
	} else if (!nlp) {
		rv = FALSE;
		ndmpd_log(LOG_DEBUG, "nlp is NULL");
	} else if (t == 0) {
		/*
		 * if we are doing base backup then we do not need to
		 * check the time, for we should backup everything.
		 */
		rv = TRUE;
		ndmpd_log(LOG_DEBUG, "Base Backup");
	} else if (S_ISDIR(stp->st_mode) && ndmp_force_bk_dirs) {
		rv = TRUE;
		ndmpd_log(LOG_DEBUG, "d(%u)", (u_int)stp->st_ino);
	} else if (stp->st_mtime > t) {
		rv = TRUE;
		ndmpd_log(LOG_DEBUG, "m(%u): %u > %u",
		    (u_int)stp->st_ino, (u_int)stp->st_mtime, (u_int)t);
	} else if (stp->st_ctime > t) {
		if (NLP_IGNCTIME(nlp)) {
			rv = FALSE;
			ndmpd_log(LOG_DEBUG, "ign c(%u): %u > %u",
			    (u_int)stp->st_ino, (u_int)stp->st_ctime,
			    (u_int)t);
		} else {
			rv = TRUE;
			ndmpd_log(LOG_DEBUG, "c(%u): %u > %u",
			    (u_int)stp->st_ino, (u_int)stp->st_ctime,
			    (u_int)t);
		}
	} else {
		rv = FALSE;
		ndmpd_log(LOG_DEBUG, "mc(%u): (%u,%u) < %u",
		    (u_int)stp->st_ino, (u_int)stp->st_mtime,
		    (u_int)stp->st_ctime, (u_int)t);
	}

	return (rv);
}

/*
 * iscreated
 *
 * This function is used to check last mtime (currently inside the ACL
 * structure) instead of ctime for checking if the file is to be backed up
 * or not. See option "inc.lmtime" for more details
 */
/*ARGSUSED*/
int iscreated(ndmp_lbr_params_t *nlp, char *name, tlm_acls_t *tacl,
    time_t t)
{
	ndmpd_log(LOG_DEBUG, "iscreated");

	int ret;
	acl_t acl = NULL;
	char *acltp = NULL;

	ndmpd_log(LOG_DEBUG, "flags %x", nlp->nlp_flags);

	if (NLP_INCLMTIME(nlp) == FALSE)
		return (0);

        acl = acl_get_file(name, ACL_TYPE_NFS4);
	int acllen= -1;
	if (acl && (acltp = acl_to_text(acl, 0)) != NULL) {
		acllen= strlen(acltp);
	}
	if (acl != NULL)
		acl_free(acl);

	ndmpd_log(LOG_DEBUG, "strlen of ACL=%d",acllen);

	if (acllen > 0) {
		tacl->acl_info.attr_info = (char*)malloc(acllen);
		if (tacl->acl_info.attr_info != NULL) {
			tacl->acl_info.attr_len = acllen;
			(void) strlcpy(tacl->acl_info.attr_info,acltp, acllen);
		}
	} else {
		tacl->acl_info.attr_len = 0;
		tacl->acl_info.attr_info = NULL;
	}

	if (acltp != NULL)
		acl_free(acltp);

	ndmpd_log(LOG_DEBUG, "tacl->acl_info.attr_info = %s",tacl->acl_info.attr_info);

	return (0);
}

/*
 * timebk_v3
 *
 * The callback function for backing up objects based on
 * their time stamp.  This is shared between token-based
 * and level-based backup, which look at the time stamps
 * of the objects to determine if they should be backed
 * up.
 *
 * Parameters:
 *   arg (input) - pointer to the backup parameters structure
 *   pnp (input) - pointer to the path node
 *   enp (input) - pointer to the entry node
 *
 * Returns:
 *   0: if backup should continue
 *   -1: if the backup should be stopped
 *   FST_SKIP: if backing up the current directory is enough
 */
static int
timebk_v3(void *arg, fst_node_t *pnp, fst_node_t *enp)
{
	ndmpd_log(LOG_DEBUG, "timebk_v3 - callback.");

	char *ent;
	int rv;
	time_t t;
	bk_param_v3_t *bpp;
	struct stat *stp;
	fs_fhandle_t *fhp;

	bpp = (bk_param_v3_t *)arg;

	rv = check_bk_args(bpp);

	if (rv != 0)
		return (rv);

	stp = enp->tn_path ? enp->tn_st : pnp->tn_st;

	if (shouldskip(bpp, pnp, enp, &rv))
		return (rv);

	if (enp->tn_path) {
		ent = enp->tn_path;
		stp = enp->tn_st;
		fhp = enp->tn_fh;
	} else {
		ent = "";
		stp = pnp->tn_st;
		fhp = pnp->tn_fh;
	}

	if (!tlm_cat_path(bpp->bp_tmp, pnp->tn_path, ent)) {
		ndmpd_log(LOG_DEBUG, "Path too long %s/%s.", pnp->tn_path, ent);
		return (FST_SKIP);
	}

	if (NLP_ISSET(bpp->bp_nlp, NLPF_LEVELBK)) {
		t = bpp->bp_nlp->nlp_ldate;
	} else {
		ndmpd_log(LOG_DEBUG, "Unknown backup type on \"%s/%s\"",
		    pnp->tn_path, ent);
		return (-1);
	}

	if (S_ISDIR(stp->st_mode)) {
		ndmpd_log(LOG_DEBUG, "backup folder");

		// file history
		bpp->bp_tlmacl->acl_dir_fh = *fhp;

		(void) ndmpd_fhdir_v3_cb(bpp->bp_nlp->nlp_logcallbacks,
		    bpp->bp_tmp, stp);

		if (ischngd(stp, t, bpp->bp_nlp)) {
			(void) memcpy(&bpp->bp_tlmacl->acl_attr, stp,
			    sizeof (struct stat));
			rv = backup_dirv3(bpp, pnp, enp);
		}
	} else {
		if (ischngd(stp, t, bpp->bp_nlp) ||
		    iscreated(bpp->bp_nlp, bpp->bp_tmp, bpp->bp_tlmacl, t)) {
			rv = 0;
			(void) memcpy(&bpp->bp_tlmacl->acl_attr, stp, sizeof (struct stat));
			bpp->bp_tlmacl->acl_fil_fh = *fhp;

			ndmpd_log(LOG_DEBUG, "backup file");

			(void) backup_filev3(bpp, pnp, enp);
		}
	}

	return (rv);
}

/*
 * backup_reader_v3
 *
 * The reader thread for the backup.  It sets up the callback
 * parameters and traverses the backup hierarchy in level-order
 * way.
 *
 * Parameters:
 *   jname (input) - name assigned to the current backup for
 *	job stats strucure
 *   nlp (input) - pointer to the nlp structure
 *   cmds (input) - pointer to the tlm_commands_t structure
 *
 * Returns:
 *   0: on success
 *   != 0: otherwise
 */

static int
backup_reader_v3(backup_reader_arg_t *argp)
{
	ndmpd_log(LOG_DEBUG, "***********backup_reader_v3**************");

	int rv;
	tlm_cmd_t *lcmd;
	tlm_acls_t tlm_acls;
	longlong_t bpos, n;
	bk_param_v3_t bp;
	fs_traverse_t ft;
	char *jname;
	ndmp_lbr_params_t *nlp;
	tlm_commands_t *cmds;

	if (!argp)
		return (-1);

	jname = argp->br_jname;
	nlp = argp->br_nlp;
	cmds = argp->br_cmds;
	rv = 0;
	lcmd = cmds->tcs_command;
	lcmd->tc_ref++;
	cmds->tcs_reader_count++;

	(void) memset(&tlm_acls, 0, sizeof (tlm_acls));

	/* NDMP parameters */
	bp.bp_session = nlp->nlp_session;
	bp.bp_nlp = nlp;

	/* LBR-related parameters  */
	bp.bp_js = tlm_ref_job_stats(jname);
	bp.bp_cmds = cmds;
	bp.bp_lcmd = lcmd;
	bp.bp_tlmacl = &tlm_acls;
	bp.bp_opr = 0;

	/* release the parent thread, after referencing the job stats */
	(void) pthread_barrier_wait(&argp->br_barrier);

	bp.bp_tmp = ndmp_malloc(sizeof (char) * TLM_MAX_PATH_NAME);
	if (!bp.bp_tmp)
		return (-1);

	/*
	 * Make the checkpointed paths for traversing the
	 * backup hierarchy, if we make the checkpoint.
	 */
	bp.bp_unchkpnm = nlp->nlp_backup_path;

	if (NLP_ISSNAP(nlp)) {
		tlm_acls.acl_checkpointed = TRUE;

		bp.bp_chkpnm = nlp->nlp_snap_backup_path;
		ndmpd_log(LOG_DEBUG, "using SNAPSURE with path=%s",bp.bp_chkpnm);
	} else {
		tlm_acls.acl_checkpointed = FALSE;
		bp.bp_chkpnm = nlp->nlp_backup_path;
		ndmpd_log(LOG_DEBUG, "NOT using SNAPSURE");
	}

	ndmpd_log(LOG_DEBUG, " nlp->nlp_backup_path = %s", nlp->nlp_backup_path);

	bp.bp_excls = ndmpd_make_exc_list();
	ft.ft_path = bp.bp_chkpnm;

	ndmpd_log(LOG_DEBUG, "path %s", ft.ft_path);

	if (NLP_ISSET(nlp, NLPF_LEVELBK)) {
		ndmpd_log(LOG_DEBUG, "backup using timebk_v3");
		ft.ft_callbk = timebk_v3;
		tlm_acls.acl_clear_archive = FALSE;
	} else {
		rv = -1;
		MOD_LOGV3(nlp->nlp_params, NDMP_LOG_ERROR, "Unknow backup type.\n");
	}

	ft.ft_arg = &bp;
	ft.ft_flags = FST_VERBOSE;	/* Solaris */

	/* take into account the header written to the stream so far */
	n = tlm_get_data_offset(lcmd);
	nlp->nlp_session->ns_data.dd_module.dm_stats.ms_bytes_processed = n;
	if (rv == 0) {
		/* start traversing the hierarchy and actual backup */
		rv = traverse_level(&ft, TRUE);
		if (rv == 0) {
			/* write the trailer and update the bytes processed */
			bpos = tlm_get_data_offset(lcmd);
			(void) write_tar_eof(lcmd);
			n = tlm_get_data_offset(lcmd) - bpos;
			nlp->nlp_session->
			    ns_data.dd_module.dm_stats.ms_bytes_processed += n;
			ndmpd_log(LOG_DEBUG, "write_tar_eof  -  ms_bytes_processed = %llu",n);
		}
	}

	NDMP_FREE(bp.bp_tmp);
	NDMP_FREE(bp.bp_excls);

	cmds->tcs_reader_count--;
	lcmd->tc_writer = TLM_STOP;
	tlm_release_reader_writer_ipc(lcmd);
	tlm_un_ref_job_stats(jname);

	return (rv);
}

/*
 * ndmp_tar_reader_v3
 *
 * NDMP Tar reader thread. This threads keep reading the tar
 * file from the tape and wakes up the consumer thread to extract
 * it on the disk
 */
int
ndmp_tar_reader_v3(ndmp_tar_reader_arg_t *argp)
{
	int bidx;
	int err;
	tlm_buffer_t *buf;
	tlm_buffers_t *bufs;
	tlm_cmd_t *lcmd;	/* Local command */
	ndmpd_session_t *session;
	ndmpd_module_params_t *mod_params;
	tlm_commands_t *cmds;
	ndmpd_log(LOG_DEBUG, "++++++++ndmp_tar_reader_v3++++++++");
	if (!argp)
		return (-1);

	session = argp->tr_session;
	mod_params = argp->tr_mod_params;
	cmds = argp->tr_cmds;

	err = 0;
	if (session == NULL) {
		ndmpd_log(LOG_DEBUG, "session == NULL");
		err = -1;
	} else if (cmds == NULL) {
		ndmpd_log(LOG_DEBUG, "cmds == NULL");
		err = -1;
	}

	if (err != 0) {
		return (err);
	}

	lcmd = cmds->tcs_command;
	bufs = lcmd->tc_buffers;

	lcmd->tc_ref++;
	cmds->tcs_reader_count++;

	/* release the parent thread, after referencing the job stats */
	(void) pthread_barrier_wait(&argp->br_barrier);

	buf = tlm_buffer_in_buf(bufs, &bidx);
	while (cmds->tcs_reader == TLM_RESTORE_RUN &&
	    lcmd->tc_reader == TLM_RESTORE_RUN) {
		(void)pthread_yield();
		if (buf->tb_full) {
			/*
			 * The buffer is still full, wait for the consumer
			 * thread to use it.
			 */
			tlm_buffer_out_buf_timed_wait(bufs, 100);
			buf = tlm_buffer_in_buf(bufs, NULL);
		} else {

			if(buf->tb_read_buf_read){
				(void) mutex_lock(&bufs->tbs_mtx);
				if ((err = MOD_READ(mod_params, buf->tb_buffer_data,
						bufs->tbs_data_transfer_size)) != 0){
					if (err < 0) {
						ndmpd_log(LOG_DEBUG, 
							"Reading buffer %d, pos: %lld",
							bidx, session->ns_mover.md_position);

						/* Force the writer to stop. */
						buf->tb_eot = buf->tb_eof = TRUE;
					} else if (err == 1) {
						ndmpd_log(LOG_DEBUG,
							"operation aborted or session terminated");
						err = 0;
					} else {
						ndmpd_log(LOG_DEBUG, "force terminated");
						err = 0;
					}

					(void) mutex_unlock(&bufs->tbs_mtx);

					MOD_LOGV3(mod_params, NDMP_LOG_ERROR,
						"Read from remote error. Restore stopped.\n");
					// gracefully stop
					cmds->tcs_reader = TLM_STOP;
					lcmd->tc_reader = TLM_STOP;
					continue;
				}

				buf->tb_read_buf_read = FALSE;

				// read buffer success
				buf->tb_eof = buf->tb_eot = FALSE;
				buf->tb_errno = 0;
				buf->tb_buffer_size = bufs->tbs_data_transfer_size;
				buf->tb_buffer_spot = 0;
				buf->tb_full = TRUE;
				(void) mutex_unlock(&bufs->tbs_mtx);

				(void) tlm_buffer_advance_in_idx(bufs);

				buf = tlm_buffer_in_buf(bufs, &bidx);
				tlm_buffer_release_in_buf(bufs);
			}
		}
	}

	/*
	 * If the consumer is waiting for us, wake it up so that it detects
	 * we're quiting.
	 */
	lcmd->tc_writer = TLM_STOP;
	tlm_buffer_release_in_buf(bufs);

	/*
	 * Clean up.
	 */
	cmds->tcs_reader_count--;
	lcmd->tc_ref--;

	ndmpd_log(LOG_DEBUG, "--------ndmp_tar_reader_v3--------");

	return (err);
}

// control the reader thread.
int FORCE_STOP_TRAVEL;
/*
 * ndmpd_tar_write_v3
 *
 * Only support write to socket.
 *
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *
 * Returns:
 *   0: on success
 *   != 0: otherwise
 */
static int
ndmp_tar_writer_v3(ndmpd_session_t *session,
	ndmpd_module_params_t *mod_params, tlm_commands_t *cmds)
{
	int bidx, nw;
	int err;
	tlm_buffer_t *buf;
	tlm_buffers_t *bufs;

	tlm_cmd_t *lcmd;	/* Local command */
	ndmpd_log(LOG_DEBUG,
		"ndmp_tar_writer_v3 --------------  write to socket using data in the output buffer");
	err = 0;
	if (session == NULL) {
		ndmpd_log(LOG_DEBUG, "session == NULL");
		err = -1;
	} else if (mod_params == NULL) {
		ndmpd_log(LOG_DEBUG, "mod_params == NULL");
		err = -1;
	} else if (cmds == NULL) {
		ndmpd_log(LOG_DEBUG, "cmds == NULL");
		err = -1;
	}

	if (err != 0)
		return (err);

	lcmd = cmds->tcs_command;
	bufs = lcmd->tc_buffers;

	lcmd->tc_ref++;
	cmds->tcs_writer_count++;

	nw = 0;
	buf = tlm_buffer_out_buf(bufs, &bidx);

	while (cmds->tcs_writer != (int)TLM_ABORT &&
	    lcmd->tc_writer != (int)TLM_ABORT) {
		(void)pthread_yield();
		if (buf->tb_full) {
			// we will only do really write if the content of buffer is filled.
			if (buf->tb_write_buf_filled) {
				(void) mutex_lock(&bufs->tbs_mtx);
				if (MOD_WRITE(mod_params, buf->tb_buffer_data,
					buf->tb_buffer_size) != 0) {
					ndmpd_log(LOG_DEBUG,
						"Writing buffer %d, pos: %lld",
						bidx, session->ns_mover.md_position);
					err = -1;

					(void) mutex_unlock(&bufs->tbs_mtx);
					// gracefully stop,
					cmds->tcs_writer = (int)TLM_ABORT;
					lcmd->tc_writer = (int)TLM_ABORT;
					MOD_LOGV3(mod_params, NDMP_LOG_ERROR,
						"Write to remote error. Backup stopped.\n");
					continue;
				}

				buf->tb_write_buf_filled = FALSE;
				buf->tb_full = buf->tb_eof = buf->tb_eot = FALSE;
				buf->tb_errno = 0;

				(void) mutex_unlock(&bufs->tbs_mtx);

				(void) tlm_buffer_advance_out_idx(bufs);
				buf = tlm_buffer_out_buf(bufs, &bidx);

				tlm_buffer_release_out_buf(bufs);
				nw++;
			}
		} else {
			if (lcmd->tc_writer != TLM_BACKUP_RUN) {
				ndmpd_log(LOG_DEBUG,
				    "tc_writer!=TLM_BACKUP_RUN; time to exit");
				break;
			} else {

				tlm_buffer_in_buf_timed_wait(bufs, 100);

			}
		}
	}
	cmds->tcs_writer_count--;
	lcmd->tc_reader = TLM_STOP;
	lcmd->tc_ref--;
	return (err);
}

void setWriteBufDone(tlm_buffers_t *bufs){
	(void) mutex_lock(&bufs->tbs_mtx);
	bufs->tbs_buffer[bufs->tbs_buffer_out ].tb_write_buf_filled = TRUE;
	(void) mutex_unlock(&bufs->tbs_mtx);
}

void setReadBufDone(tlm_buffers_t *bufs){
	(void) mutex_lock(&bufs->tbs_mtx);
	bufs->tbs_buffer[bufs->tbs_buffer_in].tb_read_buf_read = TRUE;
	(void) mutex_unlock(&bufs->tbs_mtx);
}

/*
 * ndmp_write_utf8magic_v3
 *
 * This is the same with ndmp_write_utf8magic in v2 version
 *
 * Write a magic pattern to the tar header. This is used
 * as a crest to indicate that tape belongs to us.
 */
int
ndmp_write_utf8magic_v3(tlm_cmd_t *cmd)
{
	char *cp;
	long actual_size;

	if (cmd->tc_buffers == NULL) {
		ndmpd_log(LOG_DEBUG, "cmd->tc_buffers == NULL");
		return (-1);
	}

	setWriteBufDone(cmd->tc_buffers);

	cp = tlm_get_write_buffer(RECORDSIZE, &actual_size,
	    cmd->tc_buffers, TRUE);
	if (actual_size < RECORDSIZE) {

		ndmpd_log(LOG_DEBUG, "Couldn't get enough buffer");
		return (-1);
	}

	(void) strlcpy(cp, NDMPUTF8MAGIC, RECORDSIZE);

	setWriteBufDone(cmd->tc_buffers);

	return (0);
}

/*
 * tar_backup_v3
 *
 * Traverse the backup hierarchy if needed.
 * Then launch reader and writer threads to do the actual backup.
 *
 * Parameters:
 *   session (input) - pointer to the session
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *   jname (input) - job name
 *
 * Returns:
 *   0: on success
 *   != 0: otherwise
 */
int
tar_backup_v3(ndmpd_session_t *session, ndmpd_module_params_t *params,
    ndmp_lbr_params_t *nlp, char *jname)
{

	ndmpd_log(LOG_DEBUG, "***********tar_backup_v3******************************");

	tlm_commands_t *cmds;
	backup_reader_arg_t arg;
	pthread_t rdtp;

	char info[256];
	int result;

	int err;

	if (ndmp_get_bk_dir_ino(nlp))
		return (-1);

	result = err = 0;

	// exit as if there was an internal error
	if (session->ns_eof)
		return (-1);

	if (!session->ns_data.dd_abort) {
		if (backup_alloc_structs_v3(session, jname) < 0) {
			return (-1);
		}
		FORCE_STOP_TRAVEL=0; // enable traverse of the folders.

		nlp->nlp_jstat->js_start_ltime = time(NULL);
		nlp->nlp_jstat->js_start_time = nlp->nlp_jstat->js_start_ltime;
		nlp->nlp_jstat->js_chkpnt_time = nlp->nlp_cdate;

		cmds = &nlp->nlp_cmds;
		cmds->tcs_reader = cmds->tcs_writer = TLM_BACKUP_RUN;
		cmds->tcs_command->tc_reader = TLM_BACKUP_RUN;
		cmds->tcs_command->tc_writer = TLM_BACKUP_RUN;

		if (ndmp_write_utf8magic_v3(cmds->tcs_command) < 0) {
			free_structs_v3(session, jname);
			return (-1);
		}

		ndmpd_log(LOG_DEBUG,
		    "Backing up \"%s\" started.", nlp->nlp_backup_path);

		(void) memset(&arg, 0, sizeof (backup_reader_arg_t));
		arg.br_jname = jname;
		arg.br_nlp = nlp;
		arg.br_cmds = cmds;

		(void) pthread_barrier_init(&arg.br_barrier, 0, 2);

		err = pthread_create(&rdtp, NULL, (funct_t)backup_reader_v3,
		    (void *)&arg);
		if (err == 0) {
			(void) pthread_barrier_wait(&arg.br_barrier);
		} else {
			(void) pthread_barrier_destroy(&arg.br_barrier);
			free_structs_v3(session, jname);
			ndmpd_log(LOG_DEBUG, "Launch backup_reader_v3 fail");
			return (-1);
		}

		if ((err = ndmp_tar_writer_v3(session, params, cmds)) != 0)
			result = EIO;

		nlp->nlp_jstat->js_stop_time = time(NULL);

		(void) snprintf(info, sizeof (info),
			"Runtime [%s] %llu bytes (%llu): %ld seconds\n",
		    nlp->nlp_backup_path,
		    session->ns_data.dd_module.dm_stats.ms_bytes_processed,
		    session->ns_data.dd_module.dm_stats.ms_bytes_processed,
		    (nlp->nlp_jstat->js_stop_time - nlp->nlp_jstat->js_start_ltime));

		MOD_LOGV3(params, NDMP_LOG_NORMAL, info);

		FORCE_STOP_TRAVEL=1; // force stop the traverse.
		(void) pthread_join(rdtp, NULL);
		(void) pthread_barrier_destroy(&arg.br_barrier);

		//exit as if there was an internal error
		if (session->ns_eof) {
			result = EPIPE;
			err = -1;
		}
		if (!session->ns_data.dd_abort) {

			ndmpd_log(LOG_DEBUG, "Backing up \"%s\" Finished.",
			    nlp->nlp_backup_path);
		}
	}

	if (session->ns_data.dd_abort) {
		ndmpd_log(LOG_DEBUG,
		    "Backing up \"%s\" aborted.", nlp->nlp_backup_path);
		err = -1;
	}

	free_structs_v3(session, jname);
	return (err);
}

/*
 * get_backup_size
 *
 * Find the estimate of backup size. This is used to get an estimate
 * of the progress of backup during NDMP backup.
 */
void
get_backup_size(ndmpd_session_t *session, char *path)
{
	fs_traverse_t ft;
	u_longlong_t bk_size;
	int rv;

	if (path == NULL)
		return;

	bk_size = 0;

	/*
	 * Because every share folder in ES is a volume.
	 * we can just use the size information from the volume.
	 */
	struct statfs	*mounts, mnt;
	int nmnt;
	nmnt = getmntinfo (&mounts, MNT_NOWAIT);
	while(--nmnt){
		mnt = mounts[nmnt];
		if (strncmp (mnt.f_mntonname,path,strlen(mnt.f_mntonname))!=0)
			continue;
		bk_size = mnt.f_bsize*mnt.f_blocks-mnt.f_bfree;
	}

	session->ns_data.dd_data_size = bk_size;
	ndmpd_log(LOG_DEBUG, "bksize %lld, %lldKB, %lldMB\n",
		bk_size, bk_size / 1024, bk_size /(1024 * 1024));
}

/*
 * get_rs_path_v3
 *
 * Find the restore path
 */
ndmp_error
get_rs_path_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp)
{
	char *dp;
	ndmp_error rv;
	mem_ndmp_name_v3_t *ep;
	int i, nm_cnt;
	char *nm_dpath_list[MULTIPLE_DEST_DIRS];
	static char mdest_buf[256];

	ndmpd_log(LOG_DEBUG, "get_rs_path_v3");

	*mdest_buf = 0;
	*nm_dpath_list = "";
	for (i = 0, nm_cnt = 0; i < (int)nlp->nlp_nfiles; i++) {
		ep = (mem_ndmp_name_v3_t *)MOD_GETNAME(params, i);
		if (!ep) {
			ndmpd_log(LOG_DEBUG, "Can't get Nlist[%d]", i);
			return (NDMP_ILLEGAL_ARGS_ERR);
		}
		if (strcmp(nm_dpath_list[nm_cnt], ep->nm3_dpath) != 0 &&
		    nm_cnt < MULTIPLE_DEST_DIRS - 1)
			nm_dpath_list[++nm_cnt] = ep->nm3_dpath;
	}

	multiple_dest_restore = (nm_cnt > 1);
	nlp->nlp_restore_path = mdest_buf;

	for (i = 1; i < nm_cnt + 1; i++) {
		if (ISDEFINED(nm_dpath_list[i]))
			dp = nm_dpath_list[i];
		else
			/* the default destination path is backup directory */
			dp = nlp->nlp_backup_path;

		/* check the destination directory exists and is writable */
		if (!fs_volexist(dp)) {
			rv = NDMP_ILLEGAL_ARGS_ERR;
			MOD_LOGV3(params, NDMP_LOG_ERROR,
					"Invalid destination path volume \"%s\".\n", dp);
		} else if (!voliswr(dp)) {
			rv = NDMP_ILLEGAL_ARGS_ERR;
			MOD_LOGV3(params, NDMP_LOG_ERROR,
			    "The destination path volume"
			    " is not writable \"%s\".\n", dp);
		} else {
			rv = NDMP_NO_ERR;
			(void) strlcat(nlp->nlp_restore_path, dp,
			    sizeof (mdest_buf));
			ndmpd_log(LOG_DEBUG, "rspath: \"%s\"", dp);
		}

		/*
		 * Exit if there is an error or it is not a multiple
		 * destination restore mode
		 */
		if (rv != NDMP_NO_ERR || !multiple_dest_restore)
			break;

		if (i < nm_cnt)
			(void) strlcat(nlp->nlp_restore_path, ", ",
			    sizeof (mdest_buf));
	}

	return (rv);
}

/*
 * fix_nlist_v3
 *
 * Check if the recovery list is valid and fix it if there are some
 * unspecified entries in it. It checks for original, destination
 * and new path for all NDMP names provided inside the list.
 *
 * V3: dpath is the destination directory.  If newnm is not NULL, the
 * destination path is dpath/newnm.  Otherwise the destination path is
 * dpath/opath_last_node, where opath_last_node is the last node in opath.
 *
 * V4: If newnm is not NULL, dpath is the destination directory, and
 * dpath/newnm is the destination path.  If newnm is NULL, dpath is
 * the destination path (opath is not involved in forming destination path).
 */
ndmp_error
fix_nlist_v3(ndmpd_session_t *session, ndmpd_module_params_t *params,
    ndmp_lbr_params_t *nlp)
{
	char *cp, *bp;
	char buf[TLM_MAX_PATH_NAME];
	int i, n;
	int iswrbk;
	int bvexists;
	ndmp_error rv;
	mem_ndmp_name_v3_t *ep;
	char *dp;
	char *nm;
	int existsvol;
	int isrwdst;

	ndmpd_log(LOG_DEBUG, "fix_nlist_v3");

	bvexists = fs_volexist(nlp->nlp_backup_path);
	iswrbk = voliswr(nlp->nlp_backup_path);

	rv = NDMP_NO_ERR;
	n = session->ns_data.dd_nlist_len;
	for (i = 0; i < n; i++) {
		ep = (mem_ndmp_name_v3_t *)MOD_GETNAME(params, i);
		if (!ep)
			continue;

		/* chop off the trailing slashes */
		chopslash(ep->nm3_opath);

		chopslash(ep->nm3_dpath);
		chopslash(ep->nm3_newnm);

		/* existing and non-empty destination path */
		if (ISDEFINED(ep->nm3_dpath)) {
			dp = ep->nm3_dpath;
			existsvol = fs_volexist(dp);
			isrwdst = voliswr(dp);
		} else {
			/* the default destination path is backup directory */
			dp = nlp->nlp_backup_path;
			existsvol = bvexists;
			isrwdst = iswrbk;
		}

		/* check the destination directory exists and is writable */
		if (!existsvol) {
			rv = NDMP_ILLEGAL_ARGS_ERR;
			MOD_LOGV3(params, NDMP_LOG_ERROR,
			    "Invalid destination path volume "
			    "\"%s\".\n", dp);
			break;
		}
		if (!isrwdst) {
			rv = NDMP_ILLEGAL_ARGS_ERR;
			MOD_LOGV3(params, NDMP_LOG_ERROR,
			    "The destination path volume is not "
			    "writable \"%s\".\n", dp);
			break;
		}

		/*
		 * If new name is not specified, the default new name is
		 * the last component of the original path, if any
		 * (except in V4).
		 */
		if (session->ns_protocol_version == NDMPV4) {
			nm = ep->nm3_newnm;
		} else {
			if (ISDEFINED(ep->nm3_newnm)) {
				nm = ep->nm3_newnm;
			} else {
				/*
				 * Find the last component of nm3_opath.
				 * nm3_opath has no trailing '/'.
				 */
				char *p = strrchr(ep->nm3_opath, '/');
				nm = p? p : ep->nm3_opath;
			}
		}

		bp = joinpath(buf, dp, nm);
		if (!bp) {
			/*
			 * Note: What should be done with this entry?
			 * We leave it untouched for now, hence no path in
			 * the backup image matches with this entry and will
			 * be reported as not found.
			 */
			MOD_LOGV3(params, NDMP_LOG_ERROR,
			    "Destination path too long(%s/%s)", dp, nm);
			continue;
		}
		cp = strdup(bp);
		if (!cp) {
			MOD_LOGV3(params, NDMP_LOG_ERROR,
			    "Insufficient memory.\n");
			rv = NDMP_NO_MEM_ERR;
			break;
		}
		free(ep->nm3_dpath);
		ep->nm3_dpath = cp;
		NDMP_FREE(ep->nm3_newnm);

		bp = joinpath(buf, nlp->nlp_backup_path, ep->nm3_opath);
		if (!bp) {
			/*
			 * Note: The same problem of above with long path.
			 */
			MOD_LOGV3(params, NDMP_LOG_ERROR,
			    "Path too long(%s/%s)",
			    nlp->nlp_backup_path, ep->nm3_opath);
			continue;
		}
		cp = strdup(bp);
		if (!cp) {
			MOD_LOGV3(params, NDMP_LOG_ERROR,
			    "Insufficient memory.\n");
			rv = NDMP_NO_MEM_ERR;
			break;
		}
		NDMP_FREE(ep->nm3_opath);
		ep->nm3_opath = cp;

		ndmpd_log(LOG_DEBUG, "orig[%d]: \"%s\"", i, ep->nm3_opath);
		if (ep->nm3_dpath) {
			ndmpd_log(LOG_DEBUG,
			    "dest[%d]: \"%s\"", i, ep->nm3_dpath);
		} else {
			ndmpd_log(LOG_DEBUG, "dest[%d]: \"%s\"", i, "NULL");
		}
	}

	return (rv);
}

/*
 * log_rs_params_v3
 *
 * Log a copy of all values of the restore parameters
 */
void
log_rs_params_v3(ndmpd_session_t *session, ndmpd_module_params_t *params,
    ndmp_lbr_params_t *nlp)
{
	MOD_LOGV3(params, NDMP_LOG_NORMAL, "Restoring to \"%s\".\n",
	    (nlp->nlp_restore_path) ? nlp->nlp_restore_path : "NULL");

	if (session->ns_data.dd_data_addr.addr_type == NDMP_ADDR_LOCAL) {
		MOD_LOGV3(params, NDMP_LOG_NORMAL, "Tape server: local.\n");
		MOD_LOGV3(params, NDMP_LOG_NORMAL,
		    "Tape record size: %d.\n",
		    session->ns_mover.md_record_size);
	} else if (session->ns_data.dd_data_addr.addr_type == NDMP_ADDR_TCP)
		MOD_LOGV3(params, NDMP_LOG_NORMAL,
		    "Tape server: remote at %s:%d.\n",
		    inet_ntoa(IN_ADDR(session->ns_data.dd_data_addr.tcp_ip_v3)),
		    session->ns_data.dd_data_addr.tcp_port_v3);
	else
		MOD_LOGV3(params, NDMP_LOG_ERROR,
		    "Unknown tape server address type.\n");

	if (NLP_ISSET(nlp, NLPF_DIRECT))
		MOD_LOGV3(params, NDMP_LOG_NORMAL,
		    "Direct Access Restore.\n");
}

/*
 * send_unrecovered_list_v3
 *
 * Create the list of files that were in restore list but
 * not recovered due to some errors.
 */
int
send_unrecovered_list_v3(ndmpd_module_params_t *params, ndmp_lbr_params_t *nlp)
{
	int i, rv;
	int err;

	if (!params) {
		ndmpd_log(LOG_DEBUG, "params == NULL");
		return (-1);
	}
	if (!nlp) {
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
		return (-1);
	}

	if (nlp->nlp_lastidx != -1) {

		err=0;
		(void) ndmp_send_recovery_stat_v3(params, nlp,
		    nlp->nlp_lastidx, err);
		nlp->nlp_lastidx = -1;
	}

	return (rv);
}

/*
 * restore_dar_alloc_structs_v3
 *
 * Allocates the necessary structures for running DAR restore.
 * It just creates the reader writer IPC.
 * This function is called for each entry in the restore entry list.
 *
 * Parameters:
 *   session (input) - pointer to the session
 *   jname (input) - Job name
 *
 * Returns:
 *    0: on success
 *   -1: on error
 */
int
restore_dar_alloc_structs_v3(ndmpd_session_t *session, char *jname)
{
	long xfer_size;
	ndmp_lbr_params_t *nlp;
	tlm_commands_t *cmds;

	nlp = ndmp_get_nlp(session);
	if (!nlp) {
		ndmpd_log(LOG_DEBUG, "nlp == NULL");
		return (-1);
	}

	cmds = &nlp->nlp_cmds;
	(void) memset(cmds, 0, sizeof (*cmds));

	xfer_size = ndmp_buffer_get_size(session);
	cmds->tcs_command = tlm_create_reader_writer_ipc(FALSE, xfer_size);
	if (!cmds->tcs_command) {
		tlm_un_ref_job_stats(jname);
		return (-1);
	}

	return (0);
}

/*
 * ndmpd_rs_sar_tar_v3
 *
 * Main non-DAR restore function. It will try to restore all the entries
 * that have been backed up.
 *
 * Parameters:
 *   session (input) - pointer to the session
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   0: on success
 *   -1: on error
 */
static int
ndmpd_rs_sar_tar_v3(ndmpd_session_t *session, ndmpd_module_params_t *params,
    ndmp_lbr_params_t *nlp)
{
	char jname[MAX_BACKUP_JOB_NAME];
	char *excl;
	char **sels;
	int flags;
	int err;
	tlm_commands_t *cmds;
	struct rs_name_maker rn;
	ndmp_tar_reader_arg_t arg;
	pthread_t rdtp;
	int result;

	ndmpd_log(LOG_DEBUG, "++++++++ndmpd_rs_sar_tar_v3++++++++");
	result = err = 0;
	(void) ndmp_new_job_name(jname);
	if (restore_alloc_structs_v3(session, jname) < 0)
		return (-1);

	sels = setupsels(session, params, nlp, 0);
	if (!sels) {
		free_structs_v3(session, jname);
		return (-1);
	}
	excl = NULL;
	flags = RSFLG_OVR_ALWAYS;
	rn.rn_nlp = nlp;
	rn.rn_fp = mknewname;

	nlp->nlp_jstat->js_start_ltime = time(NULL);
	nlp->nlp_jstat->js_start_time = nlp->nlp_jstat->js_start_ltime;

	if (!session->ns_data.dd_abort && !session->ns_data.dd_abort) {
		cmds = &nlp->nlp_cmds;
		cmds->tcs_reader = cmds->tcs_writer = TLM_RESTORE_RUN;
		cmds->tcs_command->tc_reader = TLM_RESTORE_RUN;
		cmds->tcs_command->tc_writer = TLM_RESTORE_RUN;

		ndmpd_log(LOG_DEBUG, "Restoring to \"%s\" started.",
		    (nlp->nlp_restore_path) ? nlp->nlp_restore_path : "NULL");

		arg.tr_session = session;
		arg.tr_mod_params = params;
		arg.tr_cmds = cmds;

		(void) pthread_barrier_init(&arg.br_barrier, 0, 2);

		err = pthread_create(&rdtp, NULL, (funct_t)ndmp_tar_reader_v3,
		    (void *)&arg);
		if (err == 0) {
			(void) pthread_barrier_wait(&arg.br_barrier);
		} else {
			(void) pthread_barrier_destroy(&arg.br_barrier);
			ndmpd_log(LOG_DEBUG, "Launch ndmp_tar_reader_v3: %m");
			free_structs_v3(session, jname);
			return (-1);
		}

		if (!ndmp_check_utf8magic(cmds->tcs_command)) {
			ndmpd_log(LOG_DEBUG, "UTF8Magic not found!");
		} else {
			ndmpd_log(LOG_DEBUG, "UTF8Magic found");
		}

		cmds->tcs_command->tc_ref++;
		cmds->tcs_writer_count++;

		if (tm_tar_ops.tm_getdir != NULL)
			err = (tm_tar_ops.tm_getdir)(cmds, cmds->tcs_command,
			    nlp->nlp_jstat, &rn, 1, 1, sels, &excl, flags, 0,
			    session->hardlink_q);

		cmds->tcs_writer_count--;
		cmds->tcs_command->tc_ref--;

		nlp->nlp_jstat->js_stop_time = time(NULL);

		/* Send the list of un-recovered files/dirs to the client.  */
		(void) send_unrecovered_list_v3(params, nlp);

		ndmp_stop_local_reader(session, cmds);

		ndmpd_log(LOG_DEBUG, "waiting for reader to stop");

		(void) pthread_join(rdtp, NULL);
		(void) pthread_barrier_destroy(&arg.br_barrier);

		ndmpd_log(LOG_DEBUG, "reader stopped");

		ndmp_stop_remote_reader(session);

		/* exit as if there was an internal error */
		ndmpd_log(LOG_DEBUG,
			"err=%d, session->ns_eof = %d session->ns_data.dd_abort=%d",
			err, session->ns_eof,session->ns_data.dd_abort);

		if (session->ns_eof)
			err = -1;
		if (err == -1)
			result = EIO;
	}

	(void) send_unrecovered_list_v3(params, nlp); /* nothing restored. */

	if (session->ns_data.dd_abort) {
		ndmpd_log(LOG_DEBUG, "Restoring to \"%s\" aborted.",
		    (nlp->nlp_restore_path) ? nlp->nlp_restore_path : "NULL");
		result = EINTR;

		err = -1;
	} else {

		ndmpd_log(LOG_DEBUG, "Restoring to \"%s\" finished. (%d)",
		    (nlp->nlp_restore_path) ? nlp->nlp_restore_path : "NULL",
		    err);
	}

	NDMP_FREE(sels);
	free_structs_v3(session, jname);
	ndmpd_log(LOG_DEBUG, "--------ndmpd_rs_sar_tar_v3--------");
	return (err);
}

/*
 * ndmpd_rs_dar_tar_v3
 *
 * Main DAR function. It calls the constructor, then for each entry it
 * calls the locate_window_v3 to find the exact position of the file. Then
 * it restores the file.
 * When all restore requests are done it calls the deconstructor to clean
 * everything up.
 *
 * Parameters:
 *   session (input) - pointer to the session
 *   params (input) - pointer to the parameters structure
 *   nlp (input) - pointer to the nlp structure
 *
 * Returns:
 *   0: on success
 *   -1: on error
 */
static int
ndmpd_rs_dar_tar_v3(ndmpd_session_t *session, ndmpd_module_params_t *params,
    ndmp_lbr_params_t *nlp)
{
	return ndmpd_rs_sar_tar_v3(session, params,nlp);
}


/*
 * ndmp_backup_get_params_v3
 *
 * Get the backup parameters from the NDMP env variables
 * and log them in the system log and as normal messages
 * to the DMA.
 *
 * Parameters:
 *   session (input) - pointer to the session
 *   params (input) - pointer to the parameters structure
 *
 * Returns:
 *   NDMP_NO_ERR: on success
 *   != NDMP_NO_ERR: otherwise
 */
ndmp_error
ndmp_backup_get_params_v3(ndmpd_session_t *session,
    ndmpd_module_params_t *params)
{
	ndmp_error rv;
	ndmp_lbr_params_t *nlp;

	ndmpd_log(LOG_DEBUG, "ndmp_backup_get_params_v3");

	if (!session || !params)
		return (NDMP_ILLEGAL_ARGS_ERR);

	rv = NDMP_NO_ERR;
	nlp = ndmp_get_nlp(session);
	if (!nlp) {
		MOD_LOGV3(params, NDMP_LOG_ERROR,"Internal error: NULL nlp.\n");
		rv = NDMP_ILLEGAL_ARGS_ERR;
	} else {
		if (!(nlp->nlp_backup_path = get_bk_path_v3(params)))
			rv = NDMP_ILLEGAL_ARGS_ERR;
		else if (!is_valid_backup_dir_v3(params, nlp->nlp_backup_path))
			rv =  NDMP_FILE_NOT_FOUND_ERR;
	}

	if (rv != NDMP_NO_ERR)
		return (rv);

	/* Should the st_ctime be ignored when backing up? */
	if (ndmp_ignore_ctime) {
		NLP_SET(nlp, NLPF_IGNCTIME);
	} else {
		NLP_UNSET(nlp, NLPF_IGNCTIME);
	}

	if (ndmp_include_lmtime == TRUE) {
		ndmpd_log(LOG_DEBUG, "including st_lmtime");
		NLP_SET(nlp, NLPF_INCLMTIME);
	} else {
		NLP_UNSET(nlp, NLPF_INCLMTIME);
	}

	ndmpd_log(LOG_DEBUG, "flags %x", nlp->nlp_flags);

	get_hist_env_v3(params, nlp);
	get_exc_env_v3(params, nlp);
	get_inc_env_v3(params, nlp);
	get_snap_env_v3(params, nlp);

	rv = get_backup_level_v3(params, nlp);

	return (rv);
}

/*
 * ndmpd_tar_backup_starter_v3
 *
 * Entry point of 0x409
 *
 * Create the checkpoint for the backup and do the backup,
 * then remove the backup checkpoint if we created it.
 * Save the backup time information based on the backup
 * type and stop the data server.
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *
 * Returns:
 *   0: on success
 *   != 0: otherwise
 */
int
ndmpd_tar_backup_starter_v3(ndmpd_module_params_t *params)
{
	int err=0;
	ndmpd_session_t *session;
	ndmp_lbr_params_t *nlp;
	char jname[MAX_BACKUP_JOB_NAME];
	char snapshot_path[TLM_MAX_PATH_NAME];

	// snapshot ID only used for TS. For ES, snapshot_id will always be 0;
	int snapshot_id;

	ndmpd_log(LOG_DEBUG, "ndmpd_tar_backup_starter_v3");

	session = (ndmpd_session_t *)(params->mp_daemon_cookie);

	/* use nlp to save the information of session, 
	 * this will be prettier then use *(params->mp_module_cookie)
	 */
	*(params->mp_module_cookie) = nlp = ndmp_get_nlp(session);

	ndmp_session_ref(session);
	(void) ndmp_new_job_name(jname);

	ndmpd_log(LOG_DEBUG, "snapshot %c", err, NDMP_YORN(NLP_ISSNAP(nlp)));
	get_backup_size(session, nlp->nlp_backup_path);

	if (err == 0) {
		err = ndmp_get_cur_bk_time(nlp, &nlp->nlp_cdate);
		if (err != 0) {
			ndmpd_log(LOG_DEBUG, "err %d", err);
		} else {
			ndmpd_log(LOG_DEBUG, "ndmpd_tar_backup_starter_v3 start the backup.");
			log_bk_params_v3(session, params, nlp);
			err = tar_backup_v3(session, params, nlp, jname);
		}
	}

	if (err == 0)
		save_backup_date_v3(params, nlp);

	/* call finish up function	*/
	MOD_DONE(params, err);
	/* nlp_params is allocated in start_backup_v3() */
	NDMP_FREE(nlp->nlp_params);

	ndmp_session_unref(session);
	pthread_exit(NULL);

	return (err);
}

/*
 * ndmpd_tar_backup_abort_v3
 *
 * Abort the backup operation and stop the reader thread.
 *
 * Parameters:
 *   module_cookie (input) - pointer to the nlp structure
 *
 * Returns:
 *   0: always
 */
int
ndmpd_tar_backup_abort_v3(void *module_cookie)
{
	ndmp_lbr_params_t *nlp;

	nlp = (ndmp_lbr_params_t *)module_cookie;
	if (nlp && nlp->nlp_session) {
		if (nlp->nlp_session->ns_data.dd_data_addr.addr_type ==
		    NDMP_ADDR_TCP &&
		    nlp->nlp_session->ns_data.dd_sock != -1) {
			(void) close(nlp->nlp_session->ns_data.dd_sock);
			nlp->nlp_session->ns_data.dd_sock = -1;
		}
		ndmp_stop_reader_thread(nlp->nlp_session);
	}

	return (0);
}

/*
 * ndmp_restore_get_params_v3
 *
 * Get the parameters specified for recovery such as restore path, type
 * of restore (DAR, non-DAR) etc
 *
 * Parameters:
 *   session (input) - pointer to the session
 *   params (input) - pointer to the parameters structure
 *
 * Returns:
 *   NDMP_NO_ERR: on success
 *   != NDMP_NO_ERR: otherwise
 */
ndmp_error
ndmp_restore_get_params_v3(ndmpd_session_t *session,
    ndmpd_module_params_t *params)
{
	ndmp_error rv;
	ndmp_lbr_params_t *nlp;
	ndmpd_log(LOG_DEBUG, "++++++++ndmp_restore_get_params_v3++++++++");
	if (!(nlp = ndmp_get_nlp(session))) {
		ndmpd_log(LOG_DEBUG, "nlp is NULL");
		rv = NDMP_ILLEGAL_ARGS_ERR;
	} else if (!(nlp->nlp_backup_path = get_bk_path_v3(params)))
		rv = NDMP_ILLEGAL_ARGS_ERR;
	else if ((nlp->nlp_nfiles = session->ns_data.dd_nlist_len) == 0) {
		ndmpd_log(LOG_DEBUG, "nfiles: %ld", nlp->nlp_nfiles);
		rv = NDMP_ILLEGAL_ARGS_ERR;
	} else if (get_rs_path_v3(params, nlp) != NDMP_NO_ERR) {
		rv = NDMP_ILLEGAL_ARGS_ERR;
	} else if ((rv = fix_nlist_v3(session, params, nlp)) != NDMP_NO_ERR) {
		ndmpd_log(LOG_DEBUG, "fix_nlist_v3: %d", rv);
	} else {
		rv = NDMP_NO_ERR;
		log_rs_params_v3(session, params, nlp);
	}
	ndmpd_log(LOG_DEBUG, "--------ndmp_restore_get_params_v3--------");
	return (rv);
}

/*
 * ndmpd_tar_restore_starter_v3
 *
 * The main restore starter function. It will start a DAR or
 * non-DAR recovery based on the parameters. (V3 and V4 only)
 *
 * Parameters:
 *   params (input) - pointer to the parameters structure
 *
 * Returns:
 *   NDMP_NO_ERR: on success
 *   != NDMP_NO_ERR: otherwise
 */
int
ndmpd_tar_restore_starter_v3(ndmpd_module_params_t *params)
{
	int err;
	ndmpd_session_t *session;
	ndmp_lbr_params_t *nlp;
	ndmpd_log(LOG_DEBUG, "++++++++ndmpd_tar_restore_starter_v3++++++++");

	session = (ndmpd_session_t *)(params->mp_daemon_cookie);
	*(params->mp_module_cookie) = nlp = ndmp_get_nlp(session);
	ndmp_session_ref(session);

	if (NLP_ISSET(nlp, NLPF_DIRECT))
		err = ndmpd_rs_dar_tar_v3(session, params, nlp);
	else
		err = ndmpd_rs_sar_tar_v3(session, params, nlp);

	MOD_DONE(params, err);
	/* nlp_params is allocated in start_recover() */
	NDMP_FREE(nlp->nlp_params);

	ndmp_session_unref(session);

	pthread_exit(NULL);
	ndmpd_log(LOG_DEBUG, "--------ndmpd_tar_restore_starter_v3--------");

	return (err);
}

/*
 * ndmp_tar_restore_abort_v3
 *
 * Restore abort function (V3 and V4 only)
 *
 * Parameters:
 *   module_cookie (input) - pointer to nlp
 *
 * Returns:
 *   0
 */
int
ndmpd_tar_restore_abort_v3(void *module_cookie)
{
	ndmp_lbr_params_t *nlp;

	nlp = (ndmp_lbr_params_t *)module_cookie;
	if (nlp != NULL && nlp->nlp_session != NULL) {
		if (nlp->nlp_session->ns_data.dd_mover.addr_type ==
		    NDMP_ADDR_TCP &&
		    nlp->nlp_session->ns_data.dd_sock != -1) {
			(void) close(nlp->nlp_session->ns_data.dd_sock);
			nlp->nlp_session->ns_data.dd_sock = -1;
		}
		nlp_event_nw(nlp->nlp_session);
		ndmp_stop_writer_thread(nlp->nlp_session);
	}

	return (0);
}
