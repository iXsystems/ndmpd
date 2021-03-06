/*
 * Copyright 2009 Sun Microsystems, Inc.  
 * Copyright 2017 Marcelo Araujo <araujo@FreeBSD.org>.
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

#include <sys/param.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <ndmpd.h>
#include <ndmpd_util.h>
#include <ndmpd_tar_v3.h>
#include <tlm.h>
#include <ndmpd_func.h>

/*
 * The dumpdates file on file system.
 */
#define	NDMP_DUMPDATES	"dumpdates"


/*
 * Offsets into the ctime string to various parts.
 */
#define	E_MONTH		4
#define	E_DAY		8
#define	E_HOUR		11
#define	E_MINUTE	14
#define	E_SECOND	17
#define	E_YEAR		20


/*
 * The contents of the file dumpdates is maintained on a linked list.
 */
typedef struct dumpdates {
	char dd_name[TLM_MAX_PATH_NAME];
	char dd_level;
	time_t dd_ddate;
	struct dumpdates *dd_next;
} dumpdates_t;


/*
 * Month names used in ctime string.
 */
static char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";


/*
 * Binary lock for accessing the dumpdates file.
 */
mutex_t ndmp_dd_lock = PTHREAD_MUTEX_INITIALIZER;

int ndmp_isdst = -1;

char *zfs_dumpdate_props[] = {
	"dumpdates:level0",
	"dumpdates:level1",
	"dumpdates:level2",
	"dumpdates:level3",
	"dumpdates:level4",
	"dumpdates:level5",
	"dumpdates:level6",
	"dumpdates:level7",
	"dumpdates:level8",
	"dumpdates:level9",
};


/*
 * lookup
 *
 * Look up the month (3-character) name and return its number.
 *
 * Returns -1 if the months name is not valid.
 */
static int
lookup(char *str)
{
	register char *cp, *cp2;

	if (!str)
		return (-1);

	for (cp = months, cp2 = str; *cp != '\0'; cp += 3)
		if (strncmp(cp, cp2, 3) == 0)
			return ((cp-months) / 3);
	return (-1);
}

/*
 * filecopy - Copy a file
 *
 * Parameters:
 *  char *dest  - Destination path
 *  char *src   - Source path
 *
 * Returns:
 *  0    - No errors
 *  #0   - Error occured
 *		-4   - read/write error
 *		-5   - source modified during copy
 *
 * Simplified version for Solaris
 */
#define	BUFSIZE	32768
int
filecopy(char *dest, char *src)
{
	FILE *src_fh = 0;
	FILE *dst_fh = 0;
	struct stat src_attr;
	struct stat dst_attr;
	char *buf = 0;
	u_longlong_t bytes_to_copy;
	size_t nbytes;
	int file_copied = 0;

	buf = ndmp_malloc(BUFSIZE);
	if (!buf)
		return (-1);

	src_fh = fopen(src, "r");
	if (src_fh == 0) {
		free(buf);
		return (-2);
	}

	dst_fh = fopen(dest, "w");
	if (dst_fh == NULL) {
		free(buf);
		(void) fclose(src_fh);
		return (-3);
	}

	if (stat(src, &src_attr) < 0) {
		free(buf);
		(void) fclose(src_fh);
		(void) fclose(dst_fh);
		return (-2);
	}

	bytes_to_copy = src_attr.st_size;
	while (bytes_to_copy) {
		if (bytes_to_copy > BUFSIZE)
			nbytes = BUFSIZE;
		else
			nbytes = bytes_to_copy;

		if ((fread(buf, nbytes, 1, src_fh) != 1) ||
		    (fwrite(buf, nbytes, 1, dst_fh) != 1))
			break;
		bytes_to_copy -= nbytes;
	}

	(void) fclose(src_fh);
	(void) fclose(dst_fh);

	if (bytes_to_copy > 0) {
		free(buf);
		/* short read/write, remove the partial file */
		return (-4);
	}

	if (stat(src, &dst_attr) < 0) {
		free(buf);
		return (-2);
	}

	free(buf);

	if (!file_copied)
		return (-5);	/* source modified during copy */
	else
		return (0);
}

/*
 * unctime
 *
 * Convert a ctime(3) format string into a system format date.
 * Return the date thus calculated.
 *
 * Return -1 if the string is not in ctime format.
 */
static int
unctime(char *str, time_t *t)
{
	struct tm then;
	char dbuf[26];

	if (!str || !t)
		return (-1);

	(void) memset(&then, 0, sizeof (then));
	(void) strlcpy(dbuf, str, sizeof (dbuf) - 1);
	dbuf[sizeof (dbuf) - 1] = '\0';
	dbuf[E_MONTH+3] = '\0';
	if ((then.tm_mon = lookup(&dbuf[E_MONTH])) < 0)
		return (-1);

	then.tm_mday = atoi(&dbuf[E_DAY]);
	then.tm_hour = atoi(&dbuf[E_HOUR]);
	then.tm_min = atoi(&dbuf[E_MINUTE]);
	then.tm_sec = atoi(&dbuf[E_SECOND]);
	then.tm_year = atoi(&dbuf[E_YEAR]) - 1900;
	then.tm_isdst = ndmp_isdst;

	ndmpd_log(LOG_DEBUG,
	    "yday %d wday %d %d/%d/%d %02d:%02d:%02d",
	    then.tm_yday, then.tm_wday, then.tm_year, then.tm_mon,
	    then.tm_mday, then.tm_hour, then.tm_min, then.tm_sec);

	*t = mktime(&then);

	return (0);
}

/*
 * ddates_pathname
 *
 * Create the dumpdates file full path name.
 */
static char *
ddates_pathname(char *buf)
{
	return (ndmpd_make_bk_dir_path(buf, NDMP_DUMPDATES));
}

/*
 * getline
 *
 * Get a line from the file and handle the continued lines.
 */
static char *
getline_ndmpd(FILE *fp, char *line, int llen)
{
	char *save;
	int len;

	if (!fp || !line)
		return (NULL);

	*(save = line) = '\0';
	do {
		if (fgets(line, llen, fp) != line)
			return (NULL);

		/* comment line? */
		if (*line == '#')
			continue;

		len = strlen(line);
		/* short line */
		if (len <= 0)
			continue;

		line += len-1;
		if (*line != '\n')
			return (NULL);

		/* trim the trailing new line */
		*line = '\0';
		if (--len <= 0)
			break;

		if (*(line-1) != '\\')
			break;

		*(line-1) = '\n';
		llen -= len;
	} while (llen > 0);

	return (save);
}

/*
 * get_ddname
 *
 * Get the path name from the buffer passed.
 *
 * Returns the beginning of the path name.  The buffer pointer is moved
 * forward to point to where the next field (the dump level) begins.
 */
static char *
get_ddname(char **bpp)
{
	char *h, *t, *save;

	if (!bpp || !*bpp)
		return (NULL);

	*bpp += strspn(*bpp, "\t ");
	save = h = t = *bpp;
	while (*t) {
		if (*t == '\t' || *t == ' ') {
			/* consume the '\t' or space character */
			t++;
			break;
		}

		if (*t == '\\')
			switch (*(t+1)) {
			case '\t':
			case ' ':
				t++; /* skip the '\\' */
				break;	/* nothing */
			}

		*h++ = *t++;
	}

	*bpp = t;
	*h++ = '\0';
	return (save);
}

/*
 * get_ddlevel
 *
 * Get the dump level from the buffer passed.
 *
 * Returns the dump level found.  The buffer pointer is moved
 * forward to point to where the next field (the dump date) begins.
 */
static int
get_ddlevel(char **bpp)
{
	char *t, *save;

	if (!bpp || !*bpp)
		return (-1);

	*bpp += strspn(*bpp, "\t ");
	save = t = *bpp;

	/*
	 * For 'F', 'A', 'I', and 'D' return the character itself.
	 */
	if (IS_LBR_BKTYPE(*t)) {
		ndmpd_log(LOG_DEBUG, "Lbr bk type %c", *t);
		/*
		 * Skip the backup type character and null terminate the
		 * string.
		 */
		*++t = '\0';
		*bpp = ++t;
		return (toupper(*save));
	}

	while (isdigit(*t))
		t++;

	*t++ = '\0';
	*bpp = t;
	return (atoi(save));
}

/*
 * get_ddate
 *
 * Get the dump date from the buffer passed.
 *
 * Returns the dump date string. The buffer pointer is moved
 * forward.  It points to the end of the buffer now.
 */
static char *
get_ddate(char **bpp)
{
	char *save;

	if (!bpp || !*bpp)
		return (NULL);

	*bpp += strspn(*bpp, "\t ");
	save = *bpp;
	*bpp += strlen(*bpp);
	return (save);
}

/*
 * put_ddname
 *
 * Print the dump path name to the dumpdates file.  It escapes the space,
 * '\t' and new line characters in the path name.  The same characters are
 * considered in the get_ddname().
 */
static void
put_ddname(FILE *fp, char *nm)
{
	if (!nm)
		return;

	while (*nm)
		switch (*nm) {
		case ' ':
		case '\n':
		case '\t':
			(void) fputc('\\', fp);
			/* FALLTHROUGH */
		default:
			(void) fputc(*nm++, fp);
		}
}

/*
 * put_ddlevel
 *
 * Print the dump level into the dumpdates file.
 */
static void
put_ddlevel(FILE *fp, int level)
{
	if (!fp)
		return;

	(void) fprintf(fp, IS_LBR_BKTYPE(level) ? "%c" : "%d", level);
}

/*
 * put_ddate
 *
 * Print the dump date into the dumpdates file.
 */
static void 
put_ddate(FILE *fp, time_t t)
{
	char tbuf[64];

	if (!fp)
		return;

	ndmpd_log(LOG_DEBUG, "[%lu]", t);

	(void) ctime_r(&t, tbuf);
	/* LINTED variable format specifier */
	(void) fprintf(fp, "%s", tbuf);
}

/*
 * dd_free
 *
 * Free the linked list of dumpdates entries.
 */
static void
dd_free(dumpdates_t *ddheadp)
{
	dumpdates_t *save;

	if (!ddheadp)
		return;

	ddheadp = ddheadp->dd_next;
	while (ddheadp) {
		save = ddheadp->dd_next;
		free(ddheadp);
		ddheadp = save;
	}
}

/*
 * makedumpdate
 *
 * Make the dumpdate node based on the string buffer passed to it.
 */
static int
makedumpdate(dumpdates_t *ddp, char *tbuf)
{
	char *nmp, *un_buf;
	int rv;

	/*
	 * While parsing each line, if a line contains one of the
	 * LBR-type levels, then checking the return value of
	 * get_ddlevel() against negative values, it OK.  Because
	 * neither of the 'F', 'A', 'I' nor 'D' have negative
	 * ASCII value.
	 */
	if (!ddp || !tbuf)
		rv = -1;
	else if (!(nmp = get_ddname(&tbuf))) {
		rv = -1;
		ndmpd_log(LOG_DEBUG, "get_ddname failed 0x%p", nmp);
	} else if ((ddp->dd_level = get_ddlevel(&tbuf)) < 0) {
		rv = -1;
		ndmpd_log(LOG_DEBUG, "dd_level < 0 %d", ddp->dd_level);
	} else if (!(un_buf = get_ddate(&tbuf))) {
		rv = -1;
		ndmpd_log(LOG_DEBUG, "get_ddate failed 0x%p", un_buf);
	} else if (unctime(un_buf, &ddp->dd_ddate) < 0) {
		rv = -1;
		ndmpd_log(LOG_DEBUG, "unctime failed \"%s\"", un_buf);
	} else {
		(void) strlcpy(ddp->dd_name, nmp, TLM_MAX_PATH_NAME);
		rv = 0;
	}

	return (rv);
}

/*
 * getrecord
 *
 * Read a record of dumpdates file and parse it.
 * The records that span multiple lines are covered.
 *
 * Returns:
 *   0 on success
 *   < 0 on error
 */
static int
getrecord(FILE *fp, dumpdates_t *ddatep, int *recno)
{
	char tbuf[BUFSIZ];

	if (!fp || !ddatep || !recno)
		return (-1);

	do {
		if (getline_ndmpd(fp, tbuf, sizeof (tbuf)) != tbuf)
			return (-1);
	} while (!*tbuf);

	if (makedumpdate(ddatep, tbuf) < 0)
		ndmpd_log(LOG_DEBUG,
		    "Unknown intermediate format in %s, line %d", tbuf, *recno);
	(*recno)++;

	if (IS_LBR_BKTYPE(ddatep->dd_level & 0xff)) {
		ndmpd_log(LOG_DEBUG, "Lbr: [%s][%c][%lu]",
		    ddatep->dd_name, ddatep->dd_level, ddatep->dd_ddate);
	} else
		ndmpd_log(LOG_DEBUG, "[%s][%d][%lu]",
		    ddatep->dd_name, ddatep->dd_level, ddatep->dd_ddate);

	return (0);
}

/*
 * readdumptimes
 *
 * Read the dumpdates file and make a linked list of its entries.
 *
 * Returns:
 *   0 on success
 *   < 0 on error
 */
static int
readdumptimes(FILE *fp, dumpdates_t *ddheadp)
{
	int recno;
	register struct	dumpdates *ddwalk;

	if (!fp || !ddheadp)
		return (-1);

	recno = 1;
	(void) memset((void *)ddheadp, 0, sizeof (*ddheadp));
	for (; ; ) {
		ddwalk = ndmp_malloc(sizeof (*ddwalk));
		if (!ddwalk)
			return (-1);

		if (getrecord(fp, ddwalk, &recno) < 0) {
			free(ddwalk);
			break;
		}

		ddwalk->dd_next = ddheadp->dd_next;
		ddheadp->dd_next = ddwalk;
		ddheadp = ddwalk;
	}

	return (0);
}

/*
 * dumprecout
 *
 * Print a record into the dumpdates file.
 */
static void
dumprecout(FILE *fp, dumpdates_t *ddp)
{
	if (!ddp)
		return;

	if (IS_LBR_BKTYPE(ddp->dd_level)) {
		ndmpd_log(LOG_DEBUG, "Lbr: [%s][%c][%lu]",
		    ddp->dd_name, ddp->dd_level, ddp->dd_ddate);
	} else
		ndmpd_log(LOG_DEBUG, "[%s][%d][%lu]",
		    ddp->dd_name, ddp->dd_level, ddp->dd_ddate);

	put_ddname(fp, ddp->dd_name);
	(void) fputc('\t', fp);
	put_ddlevel(fp, ddp->dd_level);
	(void) fputc('\t', fp);
	put_ddate(fp, ddp->dd_ddate);
}

/*
 * initdumptimes
 *
 * Open the dumpdates file and read it into memory.
 *
 * Returns:
 *   0 on success
 *   < 0 on error
 *
 */
static int
initdumptimes(dumpdates_t *ddheadp)
{
	char fname[PATH_MAX];
	int rv;
	FILE *fp;

	if (!ddheadp)
		return (-1);

	if (!ddates_pathname(fname))
		return (-1);

	fp = fopen(fname, "r");
	if (!fp) {
		if (errno != ENOENT) {
			ndmpd_log(LOG_ERR, "Cannot read %s: %m.", fname);
			return (-1);
		}
		/*
		 * Dumpdates does not exist, make an empty one.
		 */
		ndmpd_log(LOG_DEBUG,
		    "No file `%s', making an empty one", fname);

		fp = fopen(fname, "w");
		if (!fp) {
			ndmpd_log(LOG_ERR, "Cannot create %s: %m.", fname);
			return (-1);
		}
		(void) fclose(fp);

		fp = fopen(fname, "r");
		if (!fp) {
			ndmpd_log(LOG_ERR,
			    "Cannot read %s after creating it. %m.", fname);
			return (-1);
		}
	}

	rv = readdumptimes(fp, ddheadp);
	(void) fclose(fp);

	return (rv);
}

/*
 * putdumptime
 *
 * Put the record specified by path, level and backup date to the file.
 * Update the record if such entry already exists; append if not.
 *
 * Returns:
 *   0 on success
 *   < 0 on error
 */
static int
putdumptime(char *path, int level, time_t ddate)
{
	int found;
	char fname[PATH_MAX], bakfname[PATH_MAX];
	FILE *rfp, *wfp;
	dumpdates_t ddhead, tmpdd;
	register dumpdates_t *ddp;
	int rv;

	if (!path)
		return (-1);

	if (IS_LBR_BKTYPE(level))
		ndmpd_log(LOG_DEBUG, "Lbr: [%s][%c][%lu]", path, level, ddate);
	else
		ndmpd_log(LOG_DEBUG, "[%s][%d][%lu]", path, level, ddate);

	if (!ddates_pathname(fname)) {
		ndmpd_log(LOG_ERR, "Cannot get dumpdate file path name.");
		return (-1);
	}

	rfp = fopen(fname, "r");
	if (!rfp) {
		ndmpd_log(LOG_DEBUG, "Creating %s.", fname);
		(void) memset((void *)&ddhead, 0, sizeof (ddhead));
		if (initdumptimes(&ddhead) < 0) {
			ndmpd_log(LOG_ERR, "Could not initialize %s.",
			    NDMP_DUMPDATES);
			dd_free(&ddhead);
			return (-1);
		}
	} else {
		rv = readdumptimes(rfp, &ddhead);

		if (rv < 0) {
			ndmpd_log(LOG_ERR, "Error reading dumpdates file.");
			(void) fclose(rfp);
			dd_free(&ddhead);
			return (-1);
		}
		(void) fclose(rfp);
	}

	(void) snprintf(bakfname, PATH_MAX, "%s.bak", fname);
	wfp = fopen(bakfname, "w");
	if (!wfp) {
		ndmpd_log(LOG_ERR, "Cannot open %s: %m.", bakfname);
		dd_free(&ddhead);
		return (-1);
	}

	ndmpd_log(LOG_DEBUG, "[%s][%s]", fname, bakfname);

	/* try to locate the entry in the file */
	found = 0;
	for (ddp = ddhead.dd_next; ddp; ddp = ddp->dd_next) {
		if (ddp->dd_level != level)
			continue;
		if (strcmp(path, ddp->dd_name))
			continue;

		ndmpd_log(LOG_DEBUG, "Found: [%s][%d][%lu]",
		    ddp->dd_name, ddp->dd_level, ddp->dd_ddate);

		/* update the record for the entry */
		found = 1;
		ddp->dd_ddate = ddate;

		ndmpd_log(LOG_DEBUG,
		    "Updated to: [%s][%d][%lu]",
		    ddp->dd_name, ddp->dd_level, ddp->dd_ddate);
	}

	/* dump all the read records */
	for (ddp = ddhead.dd_next; ddp; ddp = ddp->dd_next)
		dumprecout(wfp, ddp);

	dd_free(&ddhead);

	/* append a new record */
	if (!found) {
		(void) strlcpy(tmpdd.dd_name, path, TLM_MAX_PATH_NAME);
		tmpdd.dd_level = level;
		tmpdd.dd_ddate = ddate;
		dumprecout(wfp, &tmpdd);
	}

	(void) fclose(wfp);
	(void) rename(bakfname, fname);

	return (0);
}

/*
 * append_dumptime
 *
 * Append the record specified by path, level and backup date to the file.
 */
static int
append_dumptime(char *fname, char *path, int level, time_t ddate)
{
	char fpath[PATH_MAX], bakfpath[PATH_MAX];
	FILE *fp;
	dumpdates_t tmpdd;

	if (!fname || !*fname || !path || !*path)
		return (-1);

	if (IS_LBR_BKTYPE(level & 0xff)) {
		ndmpd_log(LOG_DEBUG,
		    "Lbr: [%s][%s][%c][%lu]",
		    fname, path, level, ddate);
	} else
		ndmpd_log(LOG_DEBUG, "[%s][%s][%d][%lu]",
		    fname, path, level, ddate);

	if (!ndmpd_make_bk_dir_path(fpath, fname)) {
		ndmpd_log(LOG_ERR, "Cannot get dumpdate file path name %s.",
		    fname);
		return (-1);
	}

	(void) snprintf(bakfpath, PATH_MAX, "%s.bak", fpath);

	/*
	 * If the file is there and can be opened then make a
	 * backup copy it.
	 */
	fp = fopen(fpath, "r");
	if (fp) {
		(void) fclose(fp);
		if (filecopy(bakfpath, fpath) != 0) {
			ndmpd_log(LOG_ERR, "Cannot copy %s to %s: %m.",
			    fpath, bakfpath);
			return (-1);
		}
	}

	/* open the new copy to append the record to it */
	fp = fopen(bakfpath, "a");
	if (!fp) {
		ndmpd_log(LOG_ERR, "Cannot open %s: %m.", bakfpath);
		return (-1);
	}

	ndmpd_log(LOG_DEBUG, "[%s][%s]", fpath, bakfpath);

	/* append a new record */
	(void) strlcpy(tmpdd.dd_name, path, TLM_MAX_PATH_NAME);
	tmpdd.dd_level = level;
	tmpdd.dd_ddate = ddate;
	dumprecout(fp, &tmpdd);

	(void) fclose(fp);
	(void) rename(bakfpath, fpath);

	return (0);
}

/*
 * find_date
 *
 * Find the specified date
 */
static dumpdates_t *
find_date(dumpdates_t *ddp, char *path, int level, time_t t)
{
	for (; ddp; ddp = ddp->dd_next)
		if (ddp->dd_level == level && ddp->dd_ddate > t &&
		    strcmp(path, ddp->dd_name) == 0)
			break;

	return (ddp);
}

/*
 * Get the dumpdate of the last level backup done on the path.
 *
 * Returns:
 *   0 on success
 *   < 0 on error
 */
int
ndmpd_get_dumptime(char *path, int *level, time_t *ddate)
{
	ndmpd_log(LOG_DEBUG, "++++++++++++++++ndmpd_get_dumptime+++++++++++++");
	int i;
	dumpdates_t ddhead, *ddp, *save;
	char *strval;

	if (!path || !level || !ddate)
		return (-1);

	ndmpd_log(LOG_DEBUG, "[%s] level %d",
	    path, *level);

	if (*level == 0) {
		*ddate = (time_t)0;
		return (0);
	}

	(void) memset((void *)&ddhead, 0, sizeof (ddhead));
	if (initdumptimes(&ddhead) < 0) {
		dd_free(&ddhead);
		return (-1);
	}

	/*
	 * Empty dumpdates file means level 0 for all paths.
	 */
	if ((ddp = ddhead.dd_next) == 0) {
		if (!IS_LBR_BKTYPE(*level & 0xff))
			*level = 0;
		*ddate = 0;
		return (0);
	}

	/*
	 * If it's not level backup, then find the exact record
	 * type.
	 */
	if (IS_LBR_BKTYPE(*level & 0xff)) {
		save = find_date(ddp, path, *level, *ddate);

		ndmpd_log(LOG_DEBUG,
		    "LBR_BKTYPE save 0x%p", save);

		*ddate = save ? save->dd_ddate : (time_t)0;
	} else {
		/*
		 * Go find the entry with the same name for a maximum of a
		 * lower increment and older date.
		 */
		save = NULL;
		for (i = *level - 1; i >= 0; i--) {
			save = find_date(ddp, path, i, *ddate);
			if (save) {
				*level = save->dd_level;
				*ddate = save->dd_ddate;
				break;
			}
		}

		if (!save) {
			*level = 0;
			*ddate = (time_t)0;
		}
	}

	dd_free(&ddhead);
	ndmpd_log(LOG_DEBUG, "---------------ndmpd_get_dumptime--------------");
	return (0);
}

/*
 * Put the date and the level of the back up for the
 * specified path in the dumpdates file.  If there is a line
 * for the same path and the same level, the date is updated.
 * Otherwise, a line is appended to the file.
 *
 * Returns:
 *   0 on success
 *   < 0 on error
 */
int
ndmpd_put_dumptime(char *path, int level, time_t ddate)
{
	ndmpd_log(LOG_DEBUG, "++++++++++++++++ndmpd_put_dumptime+++++++++++++");
	char tbuf[64];
	int rv;

	ndmpd_log(LOG_DEBUG, "[%s][%d][%lu]", path, level,
	    ddate);

	(void) mutex_lock(&ndmp_dd_lock);
	rv = putdumptime(path, level, ddate);
	(void) mutex_unlock(&ndmp_dd_lock);
	ndmpd_log(LOG_DEBUG, "---------------ndmpd_put_dumptime--------------");
	return (rv);
}

/*
 * Append a backup date record to the specified file.
 */
int
ndmpd_append_dumptime(char *fname, char *path, int level, time_t ddate)
{
	ndmpd_log(LOG_DEBUG, "++++++++++++++++++ndmpd_append_dumptime+++++++++++++++++++++");
	char tbuf[64];
	int rv;

	ndmpd_log(LOG_DEBUG, "[%s][%s][%d][%lu]", fname,
	    path, level, ddate);

	(void) mutex_lock(&ndmp_dd_lock);
	rv = append_dumptime(fname, path, level, ddate);
	(void) mutex_unlock(&ndmp_dd_lock);

	ndmpd_log(LOG_DEBUG, "------------------ndmpd_append_dumptime---------------------");

	return (rv);
}
