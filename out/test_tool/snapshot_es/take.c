#include <stdlib.h>
#include <libzfs.h>
#include <libuutil.h>


libzfs_handle_t *g_zfs;


int delete_snapshot( char* fsname, char* snapname )
{
    int ret = 0;

    zfs_handle_t *zhp;


    printf("delete snapshot %s@%s\n", fsname, snapname);

    /* open the snapshot */
    if ((zhp = zfs_open(g_zfs, fsname, ZFS_TYPE_DATASET)) == NULL) {
        printf("cannot open open dataset: %s\n", fsname);
        return errno;
    }

    ret = zfs_destroy_snaps(zhp, snapname, B_FALSE /*defer*/);
    if (ret)
        printf("delete_snapshot failed: %s(%d)\n", strerror(ret), ret);

    zfs_close(zhp);
    return ret;
}



int zfs_create_snapshot( char* fs_name )
{
    nvlist_t *props = NULL;
    if (zfs_snapshot(g_zfs, fs_name, 0, NULL) != 0)
        return -1;
    return 0;
}

void zfs_cleanup(){
    libzfs_fini(g_zfs);
    g_zfs=0;
}

int take_snapshot(){
    zfs_handle_t *zhp;
    char *snapshotname="pool_ndmp/cc@bartTestingSnap";

    if((g_zfs = libzfs_init())==NULL){
        printf("failed to initialize ZFS library");
        return -1;
    }
    /*
       if(zfs_create_snapshot(snapshotname)==0){
       printf("take snapshot success");
       }else{
       printf("take snapshot failed with name:%s",snapshotname);
       return -2;
       }
       */
    //  zfs_cleanup();
    //
    //
    delete_snapshot("pool_ndmp/cc","bartTestingSnap");
    return (0);

}


int main(void){
    take_snapshot();
    return 0;
}
