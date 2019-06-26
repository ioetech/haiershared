//包含头文件
#include "string.h"
#include "ql_stdlib.h"
#include "stdio.h"
#include "ril_LwM2M.h"
#include "ql_error.h"
#include "ril_network.h"
#include "ril_LwM2M.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_system.h"
#include "ql_timer.h"

/*****************************************************************
* define process state
******************************************************************/
typedef enum{
    STATE_NW_QUERY_STATE,
    STATE_LwM2M_SERV,
    STATE_LwM2M_CONF,
    STATE_LwM2M_ADDOBJ,
    STATE_LwM2M_OPEN,
    STATE_LwM2M_UPDATE,
    STATE_LwM2M_CFG,
    STATE_LwM2M_SEND,
    //STATE_LwM2M_RD,
    STATE_LwM2M_CLOSE,
    STATE_LwM2M_DELETE,
    STATE_TOTAL_NUM
}Enum_ONENETSTATE;
extern u8 m_lwm2m_state;

extern u16 Data_Buffer_Len;

/*****************************************************************
* LwM2M  timer param
******************************************************************/
#define LwM2M_TIMER_ID         TIMER_ID_USER_START
#define LwM2M_TIMER_PERIOD     1000


/*****************************************************************
* Server Param
******************************************************************/
#define SRVADDR_BUFFER_LEN  100
#define SEND_BUFFER_LEN     1024
#define RECV_BUFFER_LEN     1024

//服务器地址，端口，缓存等
static u8 m_send_buf[SEND_BUFFER_LEN]={0};
static u8 m_recv_buf[RECV_BUFFER_LEN]={0};
static u8  m_SrvADDR[SRVADDR_BUFFER_LEN] = "180.101.147.115\0";
//static u8  m_SrvADDR[SRVADDR_BUFFER_LEN] = "117.60.157.137\0";

static u32 m_SrvPort = 5683;

extern u8 Net_Recv_Buffer[RECV_BUFFER_LEN];

extern u8 Net_Recv_Ctrl_Buffer[512];

extern ST_Lwm2m_Send_Param_t lwm2m_send_param_t;

extern Lwm2m_Urc_Param_t *lwm2m_urc_param_ptr;

extern int Send_Data_type;

s32 Send_Msg_to_Server(void);

#define SERVER_DOWN_CTRL_ID   0x00  //messageID，具体根据profile

#define SERVER_DOWN_CTRL      0x04

typedef struct
{
    u16 year;
    u16 mon;
    u16 day;
    u16 hour;
    u16 min;
    u16 sec;
} TimeData;



