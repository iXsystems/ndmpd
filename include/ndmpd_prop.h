
/*
 *
 */
#ifndef	_NDMPD_PROP_H
#define	_NDMPD_PROP_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NDMP property parameter flags */
#define	NDMP_CF_NOTINIT		0x00	/* Not initialized yet */
#define	NDMP_CF_DEFINED		0x01	/* Defined/read from env */
#define	NDMP_CF_MODIFIED	0x02	/* Has been modified */

#define NDMP_CONFIG_FILE	"/usr/local/etc/ndmpd.conf"

typedef enum {
	NDMP_LISTEN_NIC,
	NDMP_SERVE_NIC,
	/*
	 * Force backing up the directories leading to
	 * a modified object for 'dump' format backup.
	 */
	NDMP_DUMP_PATHNODE_ENV,
	/*
	 * Force backing up the directories leading to
	 * a modified object for 'tar' format backup.
	 */
	NDMP_TAR_PATHNODE_ENV,
	/*
	 * Force to send the file history node entries
	 * along with the file history dir entries for
	 * all directories containing the changed files
	 * to the client for incremental backup.
	 *
	 * Note: This variable is added to support BakBone
	 * Software's NetVault DMA which expects to get the
	 * FH ADD NODES for all upper directories which
	 * contain the changed files in incremental backup
	 * along with the FH ADD DIRS.
	 */
	NDMP_FHIST_INCR_ENV,
	/* Ignore st_ctime when backing up. */
	NDMP_IGNCTIME_ENV,
	/* If we should check for the last modification time. */
	NDMP_INCLMTIME_ENV,
	/* Environment variable name for the active version. */
	NDMP_VERSION_ENV,
	/*
	 * Environment variable name for restore path.
	 * Suppose that a dircetroy named "/d1/d11" is backed
	 * up and there is a file "/d1/d11/d111/f" under that
	 * directory and  the restore path is "/d1/r1".
	 * If restore path mechanism is set to 0 which means
	 * partial path restore, then the result will be
	 * "/d1/r1/d111/f". If it is set to 1 which means full
	 * path restore, the result will be "/d1/r1/d1/d11/d111/f"
	 */
	NDMP_FULL_RESTORE_PATH,
	NDMP_DEBUG_PATH,
	NDMP_PLUGIN_PATH,
	NDMP_SOCKET_CSS,
	NDMP_SOCKET_CRS,
	NDMP_MOVER_RECSIZE,
	NDMP_RESTORE_WILDCARD_ENABLE,
	NDMP_CRAM_MD5_USERNAME,
	NDMP_CRAM_MD5_PASSWORD,
	NDMP_CLEARTEXT_USERNAME,
	NDMP_CLEARTEXT_PASSWORD,
	NDMP_TCP_PORT,
	NDMP_BACKUP_QTN,
	NDMP_RESTORE_QTN,
	NDMP_OVERWRITE_QTN,
	NDMP_MAXALL
} ndmpd_cfg_id_t;

/*
 * Loads all the NDMP configuration parameters and sets up the
 * config table.
 */


int		ndmpd_load_prop(char*);
char *	ndmpd_get_prop(ndmpd_cfg_id_t id);
char *	ndmpd_get_prop_default(ndmpd_cfg_id_t id, char *dflt);
int		ndmpd_get_prop_yorn(ndmpd_cfg_id_t id);




#ifdef	__cplusplus
}
#endif

#endif /* _NDMPD_PROP_H */
