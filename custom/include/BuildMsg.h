#include "ql_stdlib.h"


#include "custom_feature_def.h"
#include "ril.h"
#include "ril_util.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_trace.h"
#include "ql_system.h"

#include "main.h"



#define envBufListLen 24

#define UPDAT_CURVE_MSG 0x0101
#define UPDAT_STATUS_CHANTE 0x0102
#define UPDAT_WARNING_INFO 0x0103
#define UPDAT_POWER_ON_MSG 0x0104
#define UPDAT_POWER_MSG 0x0105
#define UPDAT_LIMITED_MSG 0x0106

typedef struct
{
	
    u8  imsi[16];
	u8  imei[16];
	u8  ccid[22];
	s16 rsrp;
	s16 snr;
	u32 cellid;
	u8 uPlusId[32];
	u8 ver[2];
	u8 sc_ecl;
	
} NB_CONNECT;


typedef struct
{
	u8 flg;
	u8 Hours;
	u8 Mins;
	u8 temp;
	u8 HR;
	u16 PM25;
	u16 HCHO;
	u16 AQI;
	u16 CO2;
} ENVIRONMENT_BUF_PACK; //环境曲线数据

typedef struct _iApp_Msg
{
	ENVIRONMENT_BUF_PACK env_buf_pack[envBufListLen];
	u8 BigDat[50];
	float PowerDat;
	u8 VerDat[38];
} iApp_Msg;

extern u8 SendMsgBuf[1024];

extern NB_CONNECT  g_nb_conn;

extern iApp_Msg g_iAppMsg;


#define iMax35_PACK_HEADER 0x68
#define iMax35_PACK_TYPE 0x50

#define iMax35_PACK_END 0x16

#define iMax35_PACK_WELL '#'

#define iMax35_STATE_HEAD 0
#define iMax35_STATE_TYPE 1
#define iMax35_STATE_LEN1 2
#define iMax35_STATE_LEN2 3
#define iMax35_STATE_CODE 4
#define iMax35_STATE_SN 5
#define iMax35_STATE_CMD1 6
#define iMax35_STATE_CMD2 7
#define iMax35_STATE_DATA 8
#define iMax35_STATE_CHECK 9
#define iMax35_STATE_END 10

#define iMax35_STATE_SHELL 11

//////////////////////////////////////////////

#define iMax35_CMD_CONNECT 0x01
#define iMax35_CMD_CAN1 0x02
#define iMax35_CMD_CAN2 0x03
#define iMax35_CMD_VERSION 0x50

#define iMax35_CMD_CAN_CLEAR 0x1E
#define iMax35_CMD_CAN_CLEAR_OK 0x1F
#define iMax35_CMD_CAN_RESET 0x2E
#define iMax35_CMD_CAN_RESET_OK 0x2F
#define iMax35_CMD_CAN_SEND 0x3E
#define iMax35_CMD_CAN_MODE 0x4E
#define iMax35_CMD_CAN_READ 0x5E
#define iMax35_CMD_CAN_DATA 0x5F
#define iMax35_CMD_CAN_TICK 0x6E
#define iMax35_CMD_CAN_INFO 0x6F

#define iMax35_PACK_DATA_MAX 400
typedef struct _iMax35_MSG_
{

	u8 devType;
	u8 ctrCode;
	u8 sn;
	// protocol deal
	u8 RxState;
	u16 DataLen;
	u16 RxLen;
	// pack data
	u8 Check;
	u16 Cmd;
	u8 Data[iMax35_PACK_DATA_MAX];
} iMax35_MSG;

extern iMax35_MSG giMax35Msg;

void iMax35_Init(iMax35_MSG *pMsg);

u8 iMax35_MainProc(iMax35_MSG *pMsg,u8 *pDat,u16 len);

void NB_DataPro(void);

extern u8 g_ctr_down_flg ;


extern u8 g_ctr_up_data[200];
extern u8 g_ctr_up_data_len;


