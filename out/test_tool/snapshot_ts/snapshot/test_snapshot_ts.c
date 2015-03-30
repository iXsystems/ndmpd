#include <sys/stat.h>
#include <stdio.h>
#include "ts_snapshot.h"
#include <string.h>
#include <pthread.h>



/*
 *
 * Returns:

 * 	>0 	: Success and returns the snapshot id
 * 	<= 0 : Fail
 *
 * */

int create_snapshot(char *vol_path, char* snapshot_name, char *snapshot_path)
{
    int ret=0;

    int vol_id, snapshot_id=0;
    char mount_path[1024] = {0};

    Snapshot_Create_CONFIG ss_config;

    memset(&ss_config, 0, sizeof(Snapshot_Create_CONFIG));


    printf("get volume id on =%s\n",vol_path);
//	if(Volume_Get_Vol_ID_By_Mount_Path(vol_path, &vol_id)==0){
//		// take success
//		printf("get volume ID success. ID=%d\n",vol_id);
//	}else{
//		// get volume id fail
//		printf("get volume ID fail ID=%d\n",vol_id);
//		return -1;
//	}

    vol_id = 3;

	// in case we didnt delete it.
	ss_config.expire_min = 1;
	ss_config.vital = 0;

	printf("snapshot name=%s\n", snapshot_name);

	snprintf(ss_config.name, sizeof(ss_config.name), "%s", snapshot_name);

	snapshot_id = NAS_Snapshot_Create_For_App(vol_id, &ss_config);

	if(snapshot_id>0){
		printf("take snapshot success. snapshot id=%d\n", snapshot_id);
	}else{
		printf("take snapshot fail.\n");
		return -2;
	}

	if(NAS_Snapshot_Mount(snapshot_id)==0 && NAS_Snapshot_Mount_Msg(snapshot_id, 0)){
		printf("mount snapshot success\n");
		Snapshot_Set_PID(snapshot_id, 0);
	}else{
		printf("mount snapshot fail\n");
		return -3;
	}

	if(NAS_Snapshot_Get_Preview_Path(snapshot_id, mount_path, sizeof(mount_path))==0){
		printf("get snapshot path success on path=%s\n",mount_path);
		memcpy(snapshot_path, mount_path, strlen(mount_path));
		ret = snapshot_id;
	}else{
		printf("get snapshot path fail\n");
		return -4;
	}


    return ret;
}

int delete_snapshot(session ss_auth_session,int snapshot_id){
	return NAS_Snapshot_Delete(&ss_auth_session, snapshot_id);
}



void* snapshot_test(void *ptarg){
	char *path="/share/CACHEDEV3_DATA";
    char *snapshot_name="test_snapshot";
    char snapshot_path[1024] = {0};

    session ss_auth_session;

	int snapshot_id = create_snapshot(path,snapshot_name, snapshot_path);

	if(snapshot_id >0){
		printf("get snapshot path success on path=%s\n",snapshot_path);
		if(delete_snapshot(ss_auth_session, snapshot_id)==0)
			printf("delete snapshot success\n");
		else
			printf("delete snapshot fail\n");
	}
	return (NULL);
}

int main(void){
	   pthread_attr_t tattr;
		pthread_t thread;
		int rc;
/*
		(void) pthread_attr_init(&tattr);
		(void) pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
		rc = pthread_create(&thread, &tattr, snapshot_test, NULL);
		(void) pthread_attr_destroy(&tattr);
	while(1)
		sleep(1);
	*/

	Snapshot_Feature *snap_feature;
	NAS_Get_Snapshot_Feature(snap_feature);
	printf("is support snapshot=%d\n",snap_feature->support);



	return 0;
}
