#ifndef _TS_SNAPSHOT_H
#define _TS_SNAPSHOT_H


typedef	int		int32_t;
typedef	long	int64_t;

#define MAX_LV_NAME             64
#define MAX_OWNER_NAME          64
#define AUTH_SIDSIZE            9 /* nineth byte is for 0 */
#define AUTH_IPSIZE             64      //for ipv6
#define AUTH_USERSIZE           57
#define AUTH_RESERVEDSIZE       16

typedef struct _Snapshot_lite_CONFIG
{
    int snapshot_id;                   /*!< Snapshot identifier. */
    char name[MAX_LV_NAME];            /*!< Snapshot name. */
    int64_t size;                      /*!< size in sector. */
    int expire_min;                    /*!< Snapshot expire time (unit minute); 0 for never */
    int32_t time;                      /*!< Snapshot taken time, represents the number of seconds elapsed since the Epoch, 1970-01-01 00:00:00 +0000 (UTC). */
    char owner[MAX_OWNER_NAME];        /*!< Owner. */
    int32_t vital;                     /*!< can be recycled or not? 1 for no. */
    int status;
} Snapshot_lite_CONFIG;


typedef struct _session {
#ifdef AUTH_USE_UPTIME
    long touched;
#else
    time_t touched;
#endif
    char sid[AUTH_SIDSIZE];
    char ip[AUTH_IPSIZE+1];
    char user[AUTH_USERSIZE];
    int type;
    char reserved[AUTH_RESERVEDSIZE];
} session;

typedef struct _Snapshot_Create_CONFIG
{
    char name[MAX_LV_NAME];            /*!< Snapshot name. */
    int expire_min;                    /*!< Snapshot expire time (unit minute); 0 for never */
    int32_t vital;                     /*!< can be recycled or not? 1 for no. */
} Snapshot_Create_CONFIG;

/**
 * @struct Snapshot_Create_CONFIG
 * @brief  Snapshot configuration structure for creation.
 */
typedef struct _Snapshot_Msg
{
    long mtype;
    char snapshot_id[10];
} Snapshot_Msg;

typedef struct _Snapshot_Feature
{
	int support;                  /*!< Is this feature supported. */
} Snapshot_Feature;

int NAS_Snapshot_Create_By_Vol(session *ss, int vol_id, Snapshot_Create_CONFIG *confP, Snapshot_lite_CONFIG *lite_confP);

int NAS_Snapshot_Mount_Msg(int snapshot_id, int msg_type);
int NAS_Snapshot_Mount(int snapshot_id);
int NAS_Snapshot_Get_Preview_Path(int snapshot_id, char *filepath, int buf_len);
int NAS_Snapshot_Delete(session *ss, int snapshot_id);
int Snapshot_Set_PID(int snapshot_id, int pid);
int NAS_Snapshot_Create_For_App(int vol_id, Snapshot_Create_CONFIG *confP);
int NAS_Get_Snapshot_Feature(Snapshot_Feature *snapshot_featureP);

#endif //_SNAPSHOT_H

