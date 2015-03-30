
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>

#include "tlm.h"
#include "tlm_proto.h"

#define	HL_DBG_INIT		0x0001
#define	HL_DBG_CLEANUP	0x0002
#define	HL_DBG_GET	0x0004
#define	HL_DBG_ADD	0x0008

static int hardlink_q_dbg = -1;


struct hardlink_q *
hardlink_q_init()
{
	struct hardlink_q *qhead;

	qhead = (struct hardlink_q *)malloc(sizeof (struct hardlink_q));
	if (qhead) {
		SLIST_INIT(qhead);
	}

	if (hardlink_q_dbg & HL_DBG_INIT)
		ndmpd_log(LOG_DEBUG, "qhead = %p", qhead);

	return (qhead);
}

void
hardlink_q_cleanup(struct hardlink_q *hl_q)
{
	struct hardlink_node *hl;

	if (hardlink_q_dbg & HL_DBG_CLEANUP)
		ndmpd_log(LOG_DEBUG, "(1): qhead = %p", hl_q);

	if (!hl_q)
		return;

	while (!SLIST_EMPTY(hl_q)) {
		hl = SLIST_FIRST(hl_q);

		if (hardlink_q_dbg & HL_DBG_CLEANUP)
			ndmpd_log(LOG_DEBUG, "(2): remove node, inode = %lu",
			    hl->inode);

		SLIST_REMOVE_HEAD(hl_q, next_hardlink);

		/* remove the temporary file */
		if (hl->is_tmp) {
			if (hl->path) {
				ndmpd_log(LOG_DEBUG, "(3): remove temp file %s",
				    hl->path);
				if (remove(hl->path)) {
					ndmpd_log(LOG_DEBUG,
					    "error removing temp file");
				}
			} else {
				ndmpd_log(LOG_DEBUG, "no link name, inode = %lu",
				    hl->inode);
			}
		}

		if (hl->path)
			free(hl->path);
		free(hl);
	}

	free(hl_q);
}

/*
 * Return 0 if a list node has the same inode, and initialize offset and path
 * with the information in the list node.
 * Return -1 if no matching node is found.
 */
int
hardlink_q_get(struct hardlink_q *hl_q, unsigned long inode,
    unsigned long long *offset, char **path)
{
	struct hardlink_node *hl;

	if (hardlink_q_dbg & HL_DBG_GET)
		ndmpd_log(LOG_DEBUG, "(1): qhead = %p, inode = %lu",
		    hl_q, inode);

	if (!hl_q)
		return (-1);

	SLIST_FOREACH(hl, hl_q, next_hardlink) {
		if (hardlink_q_dbg & HL_DBG_GET)
			ndmpd_log(LOG_DEBUG, "(2): checking, inode = %lu",
			    hl->inode);

		if (hl->inode != inode)
			continue;

		if (offset)
			*offset = hl->offset;

		if (path)
			*path = hl->path;

		return (0);
	}

	return (-1);
}

/*
 * Add a node to hardlink_q.  Reject a duplicated entry.
 *
 * Return 0 if successful, and -1 if failed.
 */
int
hardlink_q_add(struct hardlink_q *hl_q, unsigned long inode,
    unsigned long long offset, char *path, int is_tmp_file)
{
	struct hardlink_node *hl;

	if (hardlink_q_dbg & HL_DBG_ADD)
		ndmpd_log(LOG_DEBUG,
		    "(1): qhead = %p, inode = %lu, path = %p (%s)",
		    hl_q, inode, path, path? path : "(--)");

	if (!hl_q)
		return (-1);

	if (!hardlink_q_get(hl_q, inode, 0, 0)) {
		ndmpd_log(LOG_DEBUG, "hardlink (inode = %lu) exists in queue %p",
		    inode, hl_q);
		return (-1);
	}

	hl = (struct hardlink_node *)malloc(sizeof (struct hardlink_node));
	if (!hl)
		return (-1);

	hl->inode = inode;
	hl->offset = offset;
	hl->is_tmp = is_tmp_file;
	if (path)
		hl->path = strdup(path);
	else
		hl->path = NULL;

	if (hardlink_q_dbg & HL_DBG_ADD)
		ndmpd_log(LOG_DEBUG,
		    "(2): added node, inode = %lu, path = %p (%s)",
		    hl->inode, hl->path, hl->path? hl->path : "(--)");

	SLIST_INSERT_HEAD(hl_q, hl, next_hardlink);

	return (0);
}

int
hardlink_q_dump(struct hardlink_q *hl_q)
{
	struct hardlink_node *hl;

	if (!hl_q)
		return (0);

	(void) printf("Dumping hardlink_q, head = %p:\n", (void *) hl_q);

	SLIST_FOREACH(hl, hl_q, next_hardlink)
		(void) printf(
		    "\t node = %lu, offset = %llu, path = %s, is_tmp = %d\n",
		    hl->inode, hl->offset, hl->path? hl->path : "--",
		    hl->is_tmp);

	return (0);
}
