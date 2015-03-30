#include <sys/stat.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>



int main(void){

//	struct stat st;
//	stat("./",&st);
//
//	printf("blk=%lu, cnt=%lu",st.st_blksize,st.st_blocks);
//	int nmnt;
//	struct mntent *ent;
//	FILE *fp = NULL;
//	fp = setmntent("/proc/mounts", "r");
//	if (!fp) {
//		nmnt=0;
//	}else{
//		while ((ent = getmntent(fp))) {
//				//ndmpd_log(LOG_DEBUG,"from=%s to=%s type=%s opt=%s\n", ent->mnt_fsname, ent->mnt_dir, ent->mnt_type, ent->mnt_opts);
//				//				nmnt++;
//				//						}
//				//							}
//
//		}
//	}
//	endmntent(fp);

	// read link test.
	char *path="/share/bart";
	char *linkname;
	struct stat sb;
	ssize_t r;

	printf("path=%s\n",path);

	lstat(path, &sb);
	printf("sb.st_size =%ld\n",sb.st_size );
	linkname = malloc(sb.st_size + 1);
	if (linkname == NULL) {
	        fprintf(stderr, "insufficient memory\n");
	}else{
		r = readlink(path, linkname, sb.st_size + 1);
		linkname[r] = '\0';
	}
	free(linkname);

	printf("'%s' points to '%s'\n", path, linkname);
    return 0;
}
