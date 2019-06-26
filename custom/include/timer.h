//包含头文件
#include "string.h"
#include "ql_stdlib.h"
#include "ql_timer.h"
#include "ql_time.h"


#define TIMER_INIT_STEP1 0
#define TIMER_INIT_STEP2 1
#define TIMER_INIT_OK 2

extern u32 Stack_timer;
extern u32 Led_timer;


extern u32 Timer_Count_5s;
extern u32 Timer_Count_1s;

extern u8 Timer_State;

extern u32 Timer_24Hour_Count;

extern u32 Timer_15Min_Count;

extern u32 Timer_6Hour_Count;

extern u8 led_blink_state;

extern u8 ack_cnt;
extern u8 g_ack_flg;

extern u32 g_rand_time;



typedef enum
{
    STATE_REG_NET,
    STATE_APP_RUN,
    STATE_DATA_POST,
    STATE_DATA_RECV
} Enum_LEDSTATE;


void Timer_handler(u32 timerId, void *param);

s32 Init_Timer(void);

s32 Init_LedTimer(void);

void Timer_Led_handler(u32 timerId, void *param);

void GPIO_Init(void);

u32 GetRandTime(void);




