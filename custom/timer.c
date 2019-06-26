#include "timer.h"
#include "uart.h"
#include "BuildMsg.h"


u8 ack_cnt = 0;
u8 g_ack_flg = 0;


u32 Stack_timer = 0x102; //定时器地址
u32 Timer_Count_5s = 5000;
u32 Timer_Count_1s = 1000;

u32 Timer_24Hour_Count = 0;

u32 Timer_15Min_Count = 0;

u32 Timer_6Hour_Count = 0;

u8 Timer_State = 0;


u32 Led_timer = 0x103; //定时器地址


u8 led_blink_state = STATE_REG_NET;

u8 led_blink_count = 0;



//设置本地时间
s32 Set_Local_Time(ST_Time time)
{
    s32 ret;
    //set local time
    /*time.year = 2015;
    time.month = 12;
    time.day = 10;
    time.hour = 12;
    time.minute = 30;
    time.second = 18;
    time.timezone = 8; // The range is(-11~12).one digit expresses an hour, for example: 8 indicates "GMT+8"
    */
    ret = Ql_SetLocalTime(&time);
    APP_DEBUG("<-- Ql_SetLocalTime(%d.%02d.%02d %02d:%02d:%02d timezone=%02d)=%d -->\n\r", 
        time.year, time.month, time.day, time.hour, time.minute, time.second, time.timezone, ret);
    return ret;
  
}

//获取本地时间
s32 Get_Local_Time(ST_Time *tm)
{
    ST_Time datetime;
    s32 ret;
    u64 sec;
    tm = Ql_GetLocalTime(&datetime);

    APP_DEBUG("<-- %d/%d/%d %d:%d:%d %d -->\r\n",tm->year, tm->month, tm->day, tm->hour, tm ->minute, tm->second, tm->timezone); 
    //Get total seconds elapsed since 1970.01.01 00:00:00
    sec = Ql_Mktime(tm);

    APP_DEBUG("\r\n<-- Ql_Mktime,sec=%lld -->\r\n",sec);
    //Convert the seconds elapsed since 1970.01.01 00:00:00 to local date and time
    tm=Ql_MKTime2CalendarTime(sec, & datetime);
    APP_DEBUG("<-- %d/%d/%d %d:%d:%d %d -->\r\n",tm->year, tm->month, tm->day, tm->hour, tm ->minute, tm->second, tm->timezone); 
    return ret;
}

//初始化定时器
s32 Init_Timer(void)
{
    s32 ret;
    //注册定时器，每五秒查询一次
    ret = Ql_Timer_Register(Stack_timer, Timer_handler, NULL);

    if (ret < 0)
    {
        APP_DEBUG("\r\n<--failed!!, Ql_Timer_Register: timer(%d) fail ,ret = %d -->\r\n", Stack_timer, ret);
    }
    APP_DEBUG("\r\n<--Register: timerId=%d, ret = %d -->\r\n", Stack_timer, ret);

    return ret;
}


s32 Init_LedTimer(void)
{
    s32 ret;
    //注册定时器，每五秒查询一次
    ret = Ql_Timer_Register(Led_timer, Timer_Led_handler, NULL);

    if (ret < 0)
    {
        APP_DEBUG("\r\n<--failed!!, Ql_Timer_Register: timer(%d) fail ,ret = %d -->\r\n", Led_timer, ret);
    }
    APP_DEBUG("\r\n<--Register: timerId=%d, ret = %d -->\r\n", Led_timer, ret);

    return ret;
}


#if 0
//定时器函数
// timer callback function
void Timer_handler(u32 timerId, void *param)
{
    if (Stack_timer == timerId) //到达计时
    {
        if (Timer_State == TIMER_INIT_OK)
        {
            if (Timer_15Min_Count >= 180) //是否到达15分钟
            {
                //定时器不做处理，向主进程推送消息
                Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SAVE_15MIN_DATA, NULL);
            }
            else
            {
                Timer_15Min_Count++;
            }

            if (Timer_6Hour_Count >= 4320) //是否到达6小时
            {
                //定时器不做处理，向主进程推送消息
                Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_6HOUR_BIG_DATA, NULL);
            }
            else
            {
                Timer_6Hour_Count++;
            }

            if (Timer_24Hour_Count >= 17280) //是否到达24小时
            {
                //定时器不做处理，向主进程推送消息
                Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_24HOUR_POWER_DATA, NULL);
            }
            else
            {
                Timer_24Hour_Count++;
            }
            Send_Check_Cmd();
        }
        else if (Timer_State == TIMER_INIT_STEP1)
        {
            Send_Con_Step1_Cmd();
        }
        else if (Timer_State == TIMER_INIT_STEP2)
        {
            Send_Con_Step2_Cmd();
        }
        //每五秒查询一次空调数据
        //Set_Local_Time();
    }
}
#endif


Enum_PinName  gpioPin = PINNAME_NETLIGHT;

void GPIO_Init(void)
{
    // Specify a GPIO pin
 

    // Define the initial level for GPIO pin
    Enum_PinLevel gpioLvl = PINLEVEL_HIGH;

    // Initialize the GPIO pin (output high level, pull up)
    Ql_GPIO_Init(gpioPin, PINDIRECTION_OUT, gpioLvl, PINPULLSEL_PULLUP);

	Ql_GPIO_SetLevel(gpioPin, PINLEVEL_LOW);
}


void Led_On(void)
{
	Ql_GPIO_SetLevel(gpioPin, PINLEVEL_HIGH);

}

void Led_Off(void)
{
	Ql_GPIO_SetLevel(gpioPin, PINLEVEL_LOW);

}

void Led_Trigger(void)
{
	if(Ql_GPIO_GetLevel(gpioPin))
	{
		Led_Off();
	}else
	{
		Led_On();
	}
}


static unsigned long int next = 1;

int rand(void) // RAND_MAX assumed to be 32767
{
    next = next * 1103515245 + 12345;
    return (unsigned int)(next/65536) % 32768;
}

void srand(unsigned int seed)
{
    next = seed;
}


u32 g_rand_time = 0;
u32 GetRandTime(void)
{
	u32 TEMP,TIMES;
	TEMP = Ql_atoi(&g_nb_conn.imei[10]);  //保留后6位
	
	

	srand(TEMP);
	TIMES = TEMP%(6*3600/30)*30+rand()%30;

	return TIMES;

}

//定时器函数
// timer callback function
void Timer_handler(u32 timerId, void *param)
{
	ST_Time time;
    if (Stack_timer == timerId) //到达计时
    {
		 
		
        if (Timer_State == TIMER_INIT_OK)
        {
        	 
        	 Send_Check_Cmd();

			 Timer_15Min_Count++;

            if (Timer_15Min_Count >= 180) //是否到达15分钟   //180
          // if (Timer_15Min_Count >= 1) //是否到达15分钟   //180
            {
               // Timer_15Min_Count = 0;
                //定时器不做处理，向主进程推送消息
                Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SAVE_15MIN_DATA, NULL);
	
            }
            else
            {
             //   Timer_15Min_Count++;
            }

			Timer_6Hour_Count++;
            if (Timer_6Hour_Count*5 >= g_rand_time) //是否到达6小时
           // if (Timer_6Hour_Count >= 24) //是否到达6小时
            {
               // Timer_6Hour_Count = 0;
                //定时器不做处理，向主进程推送消息

			   	     

		           Ql_GetLocalTime(&time);

				   if((time.hour == 00)&&(time.minute == 00) &&(time.second < 5))
				   {
						Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_24HOUR_POWER_DATA, NULL);

		   		   }else
		   		   {
				   		Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_6HOUR_BIG_DATA, NULL);
		   		   }	
            }
            else
            {
             ;//   Timer_6Hour_Count++;
            }
#if 0
            if (Timer_24Hour_Count >= 17280) //是否到达24小时
            {
               // Timer_24Hour_Count = 0;
                //定时器不做处理，向主进程推送消息
                Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_24HOUR_POWER_DATA, NULL);
            }
            else
            {
                Timer_24Hour_Count++;
            }
#endif
			Timer_24Hour_Count++;
			Send_Check_Cmd();
			APP_DEBUG("\r\n  @@@Timer_24Hour_Count = %d -->\r\n", Timer_24Hour_Count);
        }
        else if (Timer_State == TIMER_INIT_STEP1)
        {
            Send_Con_Step1_Cmd();
			ack_cnt++;
        }
        else if (Timer_State == TIMER_INIT_STEP2)
        {
            Send_Con_Step2_Cmd();
        }
        //每五秒查询一次空调数据
    }

	
}


void Timer_Led_handler(u32 timerId, void *param)
{

	if(Led_timer == timerId)
	{

	//APP_DEBUG("\r\n  @@@ led Timer-->\r\n");
	 switch (led_blink_state)
        {
            case STATE_REG_NET:  //0.3s
            {
                Led_Trigger();
                break;
            }
            case STATE_APP_RUN:        //1s
            {
                Led_On();
                Ql_Sleep(100);
                Led_Off();
                break;
            }
            case STATE_DATA_POST:   //发送
            {
                Led_Trigger();
                Ql_Sleep(50);
                Led_Trigger();
                Ql_Sleep(50);
                Led_Trigger();
                Ql_Sleep(50);
                Led_Trigger();
                Ql_Sleep(50);
                led_blink_state = STATE_APP_RUN;
                break;
            }
            case STATE_DATA_RECV:        //接收
            {
                Led_Trigger();
                Ql_Sleep(50);
                Led_Trigger();
                Ql_Sleep(50);
                led_blink_state = STATE_APP_RUN;
                break;
            }
            default:
            break;
        }
		
	}

}




