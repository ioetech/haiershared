/*
 --------------------------
 Project: 空调BC26
 Date: 2019-5-11
 Author: MaxPowell
 Copyright: 智城慧商信息技术有限公司 (ZCHS)
 Description: 主程序入口.
 --------------------------
*/
//包含头文件
#include "main.h"
#include "uart.h"
#include "timer.h"
#include "network.h"
#include "BuildMsg.h"


//mutex id
//u32 s_iMutexId = 0;


u8 env_len = 0;



u8 Main_Board_Ver = Ver_218;

//程序主函数
void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;

    //关闭模组休眠模式
    Ql_SleepDisable();

    //初始化串口
    ret = Init_UART();

	GPIO_Init();

    //初始化定时器
    ret = Init_Timer();
	ret = Init_LedTimer();

    //初始化网络定时器
    ret = Init_Network();


	iMax35_Init(&giMax35Msg);

    //输出启动信息，启动代码/错误代码
    APP_DEBUG("<-- BootMessage: Air Conditioning NB Application -->\r\n");
    APP_DEBUG("<-- BootMessage: Boot Code:%d -->\r\n the Boot Code = 0 is Successfully \r\n", ret);

	ret = Ql_Timer_Start(Stack_timer, Timer_Count_5s, TRUE);
	if (ret < 0)
	{
		APP_DEBUG("\r\n<-- failed!! stack timer,Ql_Timer_Start,ret=%d -->\r\n", ret);
	}
	APP_DEBUG("\r\n<-- stack timer,Ql_Timer_Start(Id=%d,Interval=%d),ret=%d -->\r\n", Stack_timer, Timer_Count_5s, ret);

	while(1)
	{
		if(Timer_State = TIMER_INIT_OK)
		{
			Main_Board_Ver = Ver_218;
			break;
		}
		if(ack_cnt >= 5)
		{
			Main_Board_Ver = Ver_100;
			Timer_State = TIMER_INIT_OK;
			Main_Board_Ver = Ver_218;
			break;
		}
		Ql_Sleep(20);
	}

    ret = Ql_Timer_Stop(Stack_timer);
     if(ret < 0)
     {
       APP_DEBUG("\r\n<--failed!! stack timer Ql_Timer_Stop ret=%d-->\r\n",ret);           
     }
     APP_DEBUG("\r\n<--stack timer,Ql_Timer_Stop(Id=%d,) ret=%d-->\r\n",Stack_timer,ret); 	

    //主循环，循环接收线程消息
    // START MESSAGE LOOP OF THIS TASK
    while (TRUE)
    {
        //获取系统消息
        Ql_OS_GetMessage(&msg);
        switch (msg.message)
        {
        //系统RIL已就绪
        case MSG_ID_RIL_READY:
            APP_DEBUG("<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize(); //初始化RIL

            break;
        //模组URC已就绪
        case MSG_ID_URC_INDICATION:
            //APP_DEBUG("<-- Received URC: type: %d, -->\r\n", msg.param1);
            switch (msg.param1)
            {
            //系统初始化就绪
            case URC_SYS_INIT_STATE_IND:
                APP_DEBUG("<-- Sys Init Status %d -->\r\n", msg.param2);
                break;
            //SIM卡已就绪
            case URC_SIM_CARD_STATE_IND:
                APP_DEBUG("<-- SIM Card Status:%d -->\r\n", msg.param2);
                //开启注网定时器
                m_lwm2m_state = STATE_NW_QUERY_STATE; //从头开始
                ret = Start_Network();                //启动定时器
                APP_DEBUG("\r\n<-- Network Start,ret=%d -->\r\n", ret);
                break;
            case URC_LwM2M_OBSERVE:
            {
                lwm2m_urc_param_ptr = msg.param2;
                if (0 == lwm2m_urc_param_ptr->observe_flag && lwm2m_send_param_t.obj_id == lwm2m_urc_param_ptr->obj_id)
                {
                    APP_DEBUG("<-- urc_lwm2m_observer,obj_id(%d),ins_id(%d),res_id(%d) -->\r\n",
                              lwm2m_urc_param_ptr->obj_id, lwm2m_urc_param_ptr->ins_id, lwm2m_urc_param_ptr->res_num);

                    //发送注网数据
                    m_lwm2m_state = STATE_LwM2M_SEND;
                    Ql_Timer_Start(LwM2M_TIMER_ID, LwM2M_TIMER_PERIOD, TRUE);
                }
            }
            break;
            case URC_LwM2M_RECV_DATA:
            {
                //数据接收消息
                lwm2m_urc_param_ptr = msg.param2;
                if (lwm2m_send_param_t.obj_id == lwm2m_urc_param_ptr->obj_id)
                {
                    if (lwm2m_urc_param_ptr->access_mode == LWM2M_ACCESS_MODE_DIRECT)
                    {
                        //Debug打印收到的数据
                        APP_DEBUG("<-- Recv Data Length(%d),Recv Data(%s) -->\r\n", lwm2m_urc_param_ptr->recv_length, lwm2m_urc_param_ptr->recv_buffer);
                        //接收数据处理进程
                        Recv_Data_Handle();
                    }
                }
            }
            break;
            //EGPRG网络就绪
            case URC_EGPRS_NW_STATE_IND:
                APP_DEBUG("<-- EGPRS Network Status:%d -->\r\n", msg.param2);
                break;
            //无线已就绪
            case URC_CFUN_STATE_IND:
                APP_DEBUG("<-- CFUN Status:%d -->\r\n", msg.param2);
                break;
            default:
                //其他URC消息提示
                APP_DEBUG("<-- Other URC: type=%d\r\n", msg.param1);
                break;
            }
            break;
        case MSG_ID_APP_PUSH:
            //收到应用层消息推送
            switch (msg.param1)
            {
            case SEND_ALARM_MESSAGE:
                //收到报警消息,上报数据
                UpdataWarningInfo();
				#if 0
                //Debug输出
                for (int i = 0; i < Data_Buffer_Len; i++)
                {
                    APP_DEBUG("0x%02X,", SendMsgBuf[i]);
                }
                APP_DEBUG("\r\n");
				#endif
                //发送消息到服务器
                ret = Send_Msg_to_Server();

                APP_DEBUG("\r\n<-- Air Alarm, Send Data ret=%d -->\r\n", ret);

                break;
            case SEND_CON_VUALE_MESSAGE:
                //收到“超过设定值”消息

                //上报超阀值
                UpdataLimitdInfo();
				#if 0
                //Debug输出
                for (int i = 0; i < Data_Buffer_Len; i++)
                {
                    APP_DEBUG("0x%02X,", SendMsgBuf[i]);
                }
                APP_DEBUG("\r\n");
				#endif
                //发送消息到服务器
                ret = Send_Msg_to_Server();
                APP_DEBUG("\r\n<-- Air Condition Value Max or Min!, Send Data ret=%d -->\r\n", ret);

                break;
            case SEND_SET_CHANGENED_MESSAGE:
                //收到“设置被更改”消息

                //上报状态
                UpdataAirStatusInfo();
                //Debug输出
                #if 0
                for (int i = 0; i < Data_Buffer_Len; i++)
                {
                    APP_DEBUG("0x%02X,", SendMsgBuf[i]);
                }
                APP_DEBUG("\r\n");
				#endif
                //发送消息到服务器
                ret = Send_Msg_to_Server();
                APP_DEBUG("\r\n<-- Air Set is Changend!, Send Data ret=%d -->\r\n", ret);
                break;
            case SAVE_15MIN_DATA:
                //收到“15分钟储存数据”消息
                if (env_len == 0)
                {
                    ST_Time datetime;
                    ST_Time *tm;
                    s32 ret;
                    u64 sec;
                    tm = Ql_GetLocalTime(&datetime);

                    APP_DEBUG("\r\n<-- %d/%d/%d %d:%d:%d %d -->\r\n", tm->year, tm->month, tm->day, tm->hour, tm->minute, tm->second, tm->timezone);
                    //Get total seconds elapsed since 1970.01.01 00:00:00
                    sec = Ql_Mktime(tm);

                    APP_DEBUG("\r\n<-- Ql_Mktime,sec=%lld -->\r\n", sec);
                    //Convert the seconds elapsed since 1970.01.01 00:00:00 to local date and time
                    tm = Ql_MKTime2CalendarTime(sec, &datetime);
                    APP_DEBUG("\r\n<-- Save Data Time: %d/%d/%d %d:%d:%d %d -->\r\n", tm->year, tm->month, tm->day, tm->hour, tm->minute, tm->second, tm->timezone);
                    g_iAppMsg.env_buf_pack[0].Hours = tm->hour;
                    g_iAppMsg.env_buf_pack[0].Mins = tm->minute;
                }
                if (env_len < 24)
                {
                	if (Main_Board_Ver == Ver_218)
                	{
	                    g_iAppMsg.env_buf_pack[env_len].temp = message.ROOMTMP;
	                    g_iAppMsg.env_buf_pack[env_len].HR = message.ROOMHUM;
	                    g_iAppMsg.env_buf_pack[env_len].PM25 = message.ROOM_PM_VALUE;
	                    g_iAppMsg.env_buf_pack[env_len].HCHO = message.GAS_VALUE;
	                    g_iAppMsg.env_buf_pack[env_len].AQI = message.VOC_VALUE;
	                    g_iAppMsg.env_buf_pack[env_len].CO2 = message.CO2_VALUE;
                	}else
                	{
	                    g_iAppMsg.env_buf_pack[env_len].temp = v100_message.TEMP;
	                    g_iAppMsg.env_buf_pack[env_len].HR = v100_message.HUMSD & 0xFF;
	                    g_iAppMsg.env_buf_pack[env_len].PM25 = v100_message.HHOFF;
	                    g_iAppMsg.env_buf_pack[env_len].HCHO = 0;
	                    g_iAppMsg.env_buf_pack[env_len].AQI = (v100_message.HHON >> 8) &0xFF;
	                    g_iAppMsg.env_buf_pack[env_len].CO2 = 0;

					}
                    g_iAppMsg.env_buf_pack[env_len].flg = 1;
                    env_len++;
                }
                else
                {
                    env_len = 0;
					
					
                }
                //取消事件响应
                Timer_15Min_Count = 0;
                APP_DEBUG("Sava Curve Data! Cnt %d\r\n",env_len);
                break;
            case SEND_6HOUR_BIG_DATA:
                //收到“6小时发送大数据+曲线”消息
                UpdataCurveInfo();

                //发送到服务器
                ret = Send_Msg_to_Server();
                //取消事件响应
                Timer_6Hour_Count = 0;
                APP_DEBUG("Send Curve Data!\r\n");
                break;
            case SEND_24HOUR_POWER_DATA:
                //收到“24小时发送电量”消息
                UpdataPowerInfo();
				#if 0
                //Debug输出
                for (int i = 0; i < Data_Buffer_Len; i++)
                {
                    APP_DEBUG("0x%02X,", SendMsgBuf[i]);
                }
                APP_DEBUG("\r\n");
				#endif
                ret = Send_Msg_to_Server();
                //取消事件响应
                Timer_24Hour_Count = 0;
				Timer_6Hour_Count = 0;
                APP_DEBUG("Send Power Data!\r\n");
                break;
			 case SEND_ANSER_CTL_DATA:
                //收到“6小时发送大数据+曲线”消息
                AnswerCtrMsg(1);

                //发送到服务器
                ret = Send_Msg_to_Server();
                //取消事件响应
        
                APP_DEBUG("Send up ctl  Data!\r\n");
                break;	
            default:
                break;
            }

        default:
            break;
        }
    }
}
