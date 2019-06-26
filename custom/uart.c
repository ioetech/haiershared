

/*
 --------------------------
 Project: 空调BC26
 Date: 2019-5-12
 Author: MaxPowell
 Copyright: 智城慧商信息技术有限公司 (ZCHS)
 Description: 串口处理.
 --------------------------
*/
#include "uart.h"
#include "BuildMsg.h"
#include "network.h"
#include "timer.h"

u8				Air_Addr[ADDR_SIGN_LEN] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


u8				Alarm_Addr[ADDR_SIGN_LEN] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


u8				Con_State[] =
{
	0xFF, 0xFF, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0x00, 0x03, 0x6E
};


//握手包1
u8				Con_ACK[] =
{
	0xFF, 0xFF, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x78
};


//握手包2
u8				Check_Cmd[] =
{
	0xFF, 0xFF, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x4D, 0xFE, 0x56
};


//查询大数据
u8				Alarm_ACK[] =
{
	0xFF, 0xFF, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x0D
};


//报警回复帧
u8				Check_Status_Cmd[] =
{
	0xFF, 0xFF, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x4D, 0x01, 0x59
};


//查询大数据
u8				Check_Cmd_Len = 9;

Frame_Data		frame_data;

Message_Data	message;

Message_Data	message_backup;


Message_Data_Ex v100_message;

Message_Data_Ex v100_message_backup;



Condition_Data	condition_rule; //环境阀值/规则设置

u8				Send_Cmd_Temp[FRAME_MAX_LEN + 2];

u8				Big_Data_All[BIG_DATA_ALL_LEN];

int 			Air_Set = 0;

int 			Air_Alarm = 0;

int 			Air_State = 0;

static Enum_SerialPort m_myUartPort2 = UART_PORT2;

//Debug串口回调
static void CallBack_UART2_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void * customizedPara)
{

}


s32 Init_UART(void)
{
	s32 			ret;

	//注册以及开启串口
	//注册，打开Debug串口
	ret 				= Ql_UART_Register(m_myUartPort2, CallBack_UART2_Hdlr, NULL);

	ret 				= Ql_UART_Open(m_myUartPort2, 115200, FC_NONE);

	//注册，打开主串口
	ret 				= Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);

	if (ret < QL_RET_OK)
		{
		APP_DEBUG("Fail to register serial port[%d], ret=%d\r\n", m_myUartPort, ret);
		}

	ret 				= Ql_UART_Open(m_myUartPort, 9600, FC_NONE);

	if (ret < QL_RET_OK)
		{
		APP_DEBUG("Fail to open serial port[%d], ret=%d\r\n", m_myUartPort, ret);
		}

	//初始化相关数据
	Ql_memset(message_backup.Set_Message, 0x0, sizeof(message_backup.Set_Message));

	//初始化默认阀值，规则
	condition_rule.ROOMTMP_VALUE_MAX = 127;
	condition_rule.ROOMTMP_VALUE_MIN = 0;

	condition_rule.ROOMHUM_VALUE_MAX = 100;
	condition_rule.ROOMHUM_VALUE_MIN = 0;

	condition_rule.ROOM_PM_VALUE_MAX = 4095;
	condition_rule.ROOM_PM_VALUE_MIN = 0;

	condition_rule.GAS_VALUE_MAX = 65535;
	condition_rule.GAS_VALUE_MIN = 0;

	condition_rule.VOC_VALUE_MAX = 1023;
	condition_rule.VOC_VALUE_MIN = 0;

	condition_rule.CO2_VALUE_MAX = 65535;
	condition_rule.CO2_VALUE_MIN = 0;

	return ret;
}


//串口数据发送
static void WriteSerialPort(Enum_SerialPort port, /*[out]*/ u8 * pBuffer, /*[in]*/ u32 bufLen)
{
	Ql_UART_Write(port, (u8 *) pBuffer, bufLen);
}


//求校验和
static u8 Get_Check_Byte(u8 * Data, u8 EndLen)
{
	u8				i;
	u16 			ck_bit = 0;

	for (i = 2; i < EndLen; i++)
		{
		ck_bit				+= Data[i];
		}

	return (u8) (ck_bit & 0xff);
}


//发送状态握手包
void Send_Con_Step1_Cmd(void)
{
	APP_DEBUG("<-- Waiting Air Connect... -->\r\n");
	Ql_UART_Write(m_myUartPort, (u8 *) Con_State, 13);
}


//发送识别握手包
void Send_Con_Step2_Cmd(void)
{
	APP_DEBUG("<-- Waiting Air Connect... -->\r\n");
	Ql_UART_Write(m_myUartPort, (u8 *) Con_ACK, 11);
}


//定时查询
void Send_Check_Cmd(void)
{

	APP_DEBUG("Send_Check_Cmd  ver:%d\r\n",Main_Board_Ver);
	if (Main_Board_Ver == Ver_218)
		{
		Ql_UART_Write(m_myUartPort, (u8 *) Check_Cmd, 13);
		}
	else 
		{
		Ql_UART_Write(m_myUartPort, (u8 *) Check_Status_Cmd, 13);
		}
}


//回复报警ACK
static void Send_Alarm_ACK(void)
{
	Ql_UART_Write(m_myUartPort, (u8 *) Alarm_ACK, 11);
}


//串口数据读取
static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/ u8 * pBuffer, /*[in]*/ u32 bufLen)
{
	s32 			rdLen = 0;
	s32 			rdTotalLen = 0;

	if (NULL == pBuffer || 0 == bufLen)
		{
		return - 1;
		}

	Ql_memset(pBuffer, 0x0, bufLen);

	while (1)
		{
		rdLen				= Ql_UART_Read(port, pBuffer + rdTotalLen, bufLen - rdTotalLen);

		if (rdLen <= 0) // All data is read out, or Serial Port Error!
			{
			break;
			}

		rdTotalLen			+= rdLen;

		// Continue to read...
		}

	if (rdLen < 0) // Serial Port Error!
		{
		APP_DEBUG("Fail to read from port[%d]\r\n", port);
		return - 99;
		}

	return rdTotalLen;
}


//串口回调函数
void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void * customizedPara)
{
	//返回长度，返回结果定义
	u32 			rdLen = 0;
	s32 			ret;

	//APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
	switch (msg)
		{
		//接收事件已就绪
		case EVENT_UART_READY_TO_READ:
				{
				//是否为指定串口
				if (m_myUartPort == port)
					{
					Ql_memset(m_RxBuf_Uart, 0x0, sizeof(m_RxBuf_Uart));
					rdLen				= ReadSerialPort(port, m_RxBuf_Uart, sizeof(m_RxBuf_Uart));





					//注释条目为输入立即返回输入内容

					/*
						s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart, sizeof(m_RxBuf_Uart));
						if (totalBytes <= 0)
						{
							APP_DEBUG("<-- No data in UART buffer! -->\r\n");
							return;
						}
						{// Read data from UART
							s32 ret;
							char* pCh = NULL;
							
							// Echo
							Ql_UART_Write(m_myUartPort, m_RxBuf_Uart, totalBytes);

							pCh = Ql_strstr((char*)m_RxBuf_Uart, "\r\n");
							if (pCh)
							{
								*(pCh + 0) = '\0';
								*(pCh + 1) = '\0';
							}

							// No permission for single <cr><lf>
							if (Ql_strlen((char*)m_RxBuf_Uart) == 0)
							{
								return;
							}
							ret = Ql_RIL_SendATCmd((char*)m_RxBuf_Uart, totalBytes, ATResponse_Handler, NULL, 0);
						}
						*/
					//帧头2byte，直接判断
					if (m_RxBuf_Uart[0] == START_FRAME && m_RxBuf_Uart[1] == START_FRAME)
						{
						//收到帧头
						if (m_RxBuf_Uart[START_FRAME_LEN] != 0x00)
							{
							//后续数据不为0
							u8 *			buffer_index, *buffer_last;

							//保存帧长
							frame_data.length	= m_RxBuf_Uart[START_FRAME_LEN];

							APP_DEBUG("Frame Length:%d\r\n", frame_data.length);


							APP_DEBUG("\r\nUartRev Len:%d Rev Data: ", frame_data.length + 6);

							for (int i = 0; i < frame_data.length + 6; i++)
								{
								APP_DEBUG("%02X ", m_RxBuf_Uart[i]);
								}

							APP_DEBUG("\r\n");

							//小于最大帧长，大于1
							if (frame_data.length > 1 && frame_data.length <= FRAME_MAX_LEN)
								{
								u8 *			level_1, *level_2;

								//过滤FF 55
								frame_data.message_length = frame_data.length - 1;
								buffer_index		= &m_RxBuf_Uart[START_FRAME_LEN + 1];
								buffer_last 		= &m_RxBuf_Uart[rdLen - 1];
								level_1 			= memchr(buffer_index, START_FRAME, rdLen - START_FRAME_LEN);

								while (level_1 != NULL && * (level_1 + 1) == FRAME_MASK_BIT)
									{
									level_2 			= level_1;

									while (level_2 < buffer_last)
										{
										level_2 			= level_2 + 1;
										*level_2			= * (level_2 + 1);
										}

									level_1 			= level_1 + 1;
									level_1 			= memchr(level_1, START_FRAME, buffer_last - level_1);

									//APP_DEBUG("%d\r\n",buffer_last-level_1);
									}

								//取有效负荷+校验和
								Ql_memset(frame_data.buffer, 0x0, sizeof(frame_data.buffer));
								Ql_memcpy(frame_data.buffer, buffer_index, frame_data.length);

								frame_data.check_byte = frame_data.buffer[frame_data.message_length];

								//frame_data.buffer[frame_data.message_length] = 0x00;
								APP_DEBUG("\r\nframe_data Data:");

								for (int i = 0; i < frame_data.length; i++)
									{
									APP_DEBUG("%02X ", frame_data.buffer[i]);
									}

								APP_DEBUG("\r\n");

								if (frame_data.message_length >= ADDR_SIGN_LEN)
									{
									//解地址标识
									Ql_memset(frame_data.addr_sign, 0x0, sizeof(frame_data.addr_sign));
									Ql_memcpy(frame_data.addr_sign, frame_data.buffer, ADDR_SIGN_LEN);
									APP_DEBUG("Message Addr:");

									for (int i = 0; i < ADDR_SIGN_LEN; i++)
										{
										APP_DEBUG("0x%02X,", frame_data.addr_sign[i]);
										}

									APP_DEBUG("\r\n");
									}

								if (frame_data.message_length > ADDR_SIGN_LEN)
									{
									//解帧类型
									frame_data.type 	= frame_data.buffer[ADDR_SIGN_LEN];
									APP_DEBUG("Message Type:0x%02X\r\n", frame_data.type);
									}

								if (frame_data.message_length > (ADDR_SIGN_LEN + 1))
									{
									//是否报警
									if (frame_data.type == MESSAGE_TYPE_ALARM)
										{
										//模块发生报警
										Air_Alarm			= 1;

										//立即回复报警ACK
										Send_Alarm_ACK();
										APP_DEBUG("<-- Air Alarm! -->\r\n");
										}
									else 
										{
										Air_Alarm			= 0;
										}

									if (frame_data.type == MESSAGE_TYPE_STATE)
										{
										if (g_ctr_down_flg)
											{
											APP_DEBUG("<-- up  ctr data! -->\r\n");
											Ql_memset(g_ctr_up_data, 0x0, sizeof(g_ctr_up_data));
											Ql_memcpy(g_ctr_up_data, m_RxBuf_Uart, frame_data.length + 5);
											g_ctr_up_data_len	= frame_data.length + 5;
											Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_ANSER_CTL_DATA, NULL);
											g_ctr_down_flg		= 0;
											}
										}

									//识别握手帧
									if (frame_data.type == MESSAGE_TYPE_CON_S1)
										{
											u8 *			message_index;
											Timer_State = TIMER_INIT_STEP2;

											message_index		= &frame_data.buffer[ADDR_SIGN_LEN + 1];
											Ql_memset(g_iAppMsg.VerDat, 0x0, sizeof(g_iAppMsg.VerDat));
											Ql_memcpy(g_iAppMsg.VerDat, message_index, BIG_DATA_ALL_LEN);

										}

									if (frame_data.type == MESSAGE_TYPE_CON_S2)
										{
											Timer_State = TIMER_INIT_OK;
											APP_DEBUG("<-- Air Connect OK! -->\r\n");
										}

									if (Main_Board_Ver == Ver_218)
										{
										//解状态消息
										if (frame_data.type == MESSAGE_TYPE_STATE ||
											 frame_data.type == MESSAGE_TYPE_ALARM)
											{
											u8 *			message_index;

											message_index		= &frame_data.buffer[ADDR_SIGN_LEN + 1];

											//測試用，一層層判斷，可不定長發送，進行測試
											if (frame_data.message_length > (ADDR_SIGN_LEN + 30)) //有效消息共30bit，除去开头命令报文2bit，28bit，大数据多10条/14bit，最大42bit
												{
												message.MESSAGE_TYPE = *message_index;

												message_index		= message_index + 2;

												if (message.MESSAGE_TYPE == MESSAGE_BIG_RET_TYPE &&
													 frame_data.message_length > (ADDR_SIGN_LEN + 44))
													{
													Ql_memset(g_iAppMsg.BigDat, 0x0, sizeof(g_iAppMsg.BigDat));
													Ql_memcpy(g_iAppMsg.BigDat, message_index, BIG_DATA_ALL_LEN);
													}

												Ql_memset(message.Set_Message, 0x0, sizeof(message.Set_Message));
												Ql_memcpy(message.Set_Message, message_index, MESSAGE_SET_LEN);

												message.SETTMP		= (*message_index) + 16;

												// APP_DEBUG("<1> SETTMP:%d C\r\n", message.SETTMP);
												message_index		= message_index + 1;
												message.SETUPDN 	= *message_index;

												// APP_DEBUG("<2> SETUPDN:0x%02X\r\n", message.SETUPDN);
												u8				tmp;

												message_index		= message_index + 1;
												tmp 				= *message_index;
												message.SETMODE 	= tmp >> 5;
												message.SETSPMODE	= ((u8) (tmp << 3)) >> 6;
												message.SETSPEED	= ((u8) (tmp << 5)) >> 5;

												// APP_DEBUG("<3> SETMODE:0x%02X,SETSPMODE:0x%02X,SETSPEED:0x%02X\r\n", message.SETMODE, message.SETSPMODE, message.SETSPEED);
												message_index		= message_index + 1;

												// APP_DEBUG("<4> NOP\r\n");
												message_index		= message_index + 1;
												tmp 				= *message_index;
												message.CTMP_FTMP	= ((u8) (tmp << 2)) >> 7;
												message.COMFORTABLE = ((u8) (tmp << 3)) >> 7;
												message.SMART_MODE	= ((u8) (tmp << 4)) >> 7;
												message.ZERO_POT_FIVE = ((u8) (tmp << 5)) >> 7;
												message.SEN_DISPLAY = ((u8) (tmp << 6)) >> 7;
												message.TEN_HEATING = ((u8) (tmp << 7)) >> 7;
												message_index		= message_index + 1;
												tmp 				= *message_index;
												message.AUTO_SLEEP_CURVE = tmp >> 7;
												message.ELE_LOCK	= ((u8) (tmp << 1)) >> 7;
												message.SLEEP_MUTE	= ((u8) (tmp << 2)) >> 7;
												message.SET_MUTE	= ((u8) (tmp << 3)) >> 7;
												message.STRONG_MODE = ((u8) (tmp << 4)) >> 7;
												message.ELE_HEATING = ((u8) (tmp << 5)) >> 7;
												message.HEALTH_MODE = ((u8) (tmp << 6)) >> 7;
												message.POWER_STATE = ((u8) (tmp << 7)) >> 7;

												//APP_DEBUG("<5> CTMP_FTMP:0x%02X,COMFORTABLE:0x%02X,SMART_MODE:0x%02X,ZERO_POT_FIVE:0x%02X,SEN_DISPLAY:0x%02X,TEN_HEATING:0x%02X,AUTO_SLEEP_CURVE:0x%02X,ELE_LOCK:0x%02X,SLEEP_MUTE:0x%02X,STRONG_MODE:0x%02X,ELE_HEATING:0x%02X,HEALTH_MODE:0x%02X,POWER_STATE:0x%02X\r\n", message.CTMP_FTMP, message.COMFORTABLE, message.SMART_MODE, message.ZERO_POT_FIVE, message.SEN_DISPLAY, message.TEN_HEATING, message.AUTO_SLEEP_CURVE, message.ELE_LOCK, message.SLEEP_MUTE, message.STRONG_MODE, message.ELE_HEATING, message.HEALTH_MODE, message.POWER_STATE);
												message_index		= message_index + 1;
												message.SETHUMIDITY = (*message_index) + 30;

												//APP_DEBUG("<6> SETHUMIDITY:%d\r\n", message.SETHUMIDITY);
												message_index		= message_index + 1;
												tmp 				= *message_index;
												message.SETPERSON	= tmp >> 6;
												message.SETLEFTRIGHT = ((u8) (tmp << 5)) >> 5;

												// APP_DEBUG("<7> SETPERSON:0x%02X,SETLEFTRIGHT:0x%02X\r\n", message.SETPERSON, message.SETLEFTRIGHT);
												message_index		= message_index + 1;
												tmp 				= *message_index;
												message.SETCLEANTIME = ((u8) (tmp << 7)) >> 7;
												message_index		= message_index + 1;
												tmp 				= *message_index;
												message.SETPOWERSAVE = ((u8) (tmp << 1)) >> 7;
												message.SETBACKLIGHT = ((u8) (tmp << 2)) >> 7;
												message.SETAUTOCLEAN = ((u8) (tmp << 3)) >> 7;
												message.SETCLEANGAS = ((u8) (tmp << 4)) >> 7;
												message.SETCLEANDUST = ((u8) (tmp << 5)) >> 7;
												message.SETADDHUM	= ((u8) (tmp << 6)) >> 7;
												message.SETNEWWIND	= ((u8) (tmp << 7)) >> 7;

												//APP_DEBUG("<8> SETCLEANTIME:0x%02X,SETPOWERSAVE:0x%02X,SETBACKLIGHT:0x%02X,SETAUTOCLEAN:0x%02X,SETCLEANGAS:0x%02X,SETCLEANDUST:0x%02X,SETADDHUM:0x%02X,SETNEWWIND:0x%02X\r\n", message.SETCLEANTIME, message.SETPOWERSAVE, message.SETBACKLIGHT, message.SETAUTOCLEAN, message.SETCLEANGAS, message.SETCLEANDUST, message.SETADDHUM, message.SETNEWWIND);
												message_index		= message_index + 1;
												message.ROOMTMP 	= (*message_index) / 2;

												// APP_DEBUG("<9> ROOMTMP:%d C\r\n", message.ROOMTMP);
												message_index		= message_index + 1;
												message.ROOMHUM 	= *message_index;

												//APP_DEBUG("<10> ROOMHUM:%d %%\r\n", message.ROOMHUM);
												message_index		= message_index + 1;

												if (*message_index != 0x00)
													{
													message.OUTTEMP 	= (*message_index) - 64;

													//	  APP_DEBUG("<11> OUTTEMP:%d\r\n", message.OUTTEMP);
													}
												else 
													{
													*message_index		= 0;
													}

												message_index		= message_index + 1;
												tmp 				= *message_index;
												message.AIRPM_TYPE	= tmp >> 7;
												message.AIRPM_MAN	= ((u8) (tmp << 2)) >> 6;
												message.AIRPM_GAS	= ((u8) (tmp << 4)) >> 6;
												message.AIRPM_ROOMPM = ((u8) (tmp << 6)) >> 6;

												//APP_DEBUG("<12> AIRPM_TYPE:0x%02X,AIRPM_MAN:0x%02X,AIRPM_GAS:0x%02X,AIRPM_ROOMPM:0x%02X\r\n", message.AIRPM_TYPE, message.AIRPM_MAN, message.AIRPM_GAS, message.AIRPM_ROOMPM);
												message_index		= message_index + 1;
												message.ERROR_CODE	= *message_index;

												// APP_DEBUG("<13> ERROR:0x%02X\r\n", message.ERROR_CODE);
												message_index		= message_index + 1;
												tmp 				= *message_index;
												message.ERROR_CODE_ACK = tmp >> 7;
												message.AIR_RUN_MODE = ((u8) (tmp << 4)) >> 6;
												message.CRT_CMD 	= ((u8) (tmp << 6)) >> 6;

												// APP_DEBUG("<14> ERROR_CODE_ACK:0x%02X,AIR_RUN_MODE:0x%02X,CRT_CMD:0x%02X\r\n", message.ERROR_CODE_ACK, message.AIR_RUN_MODE, message.CRT_CMD);
												u8				tmp1, tmp2;

												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												message.BROAD_CLEANTIME = (u16) (tmp1 << 8 | tmp2);

												//APP_DEBUG("<15> BROAD_CLEANTIME:%d\r\n", message.BROAD_CLEANTIME);
												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												message.ROOM_PM_VALUE = (u16) (tmp1 << 8 | tmp2);

												//APP_DEBUG("<16> ROOM_PM_VALUE:%d\r\n", message.ROOM_PM_VALUE);
												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												message.OUT_PM_VALUE = (u16) (tmp1 << 8 | tmp2);

												// APP_DEBUG("<17> OUT_PM_VALUE:%d\r\n", message.OUT_PM_VALUE);
												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												message.GAS_VALUE	= (u16) (tmp1 << 8 | tmp2);

												// APP_DEBUG("<18> GAS_VALUE:%d\r\n", message.GAS_VALUE);
												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												message.VOC_VALUE	= (u16) (tmp1 << 8 | tmp2);

												// APP_DEBUG("<19> VOC_VALUE:%d\r\n", message.VOC_VALUE);
												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												message.CO2_VALUE	= (u16) (tmp1 << 8 | tmp2);

												// APP_DEBUG("<20> CO2_VALUE:%d\r\n", message.CO2_VALUE);
												if (message.MESSAGE_TYPE == MESSAGE_BIG_RET_TYPE)
													{
													message_index		= message_index + 1;
													tmp1				= *message_index;
													message_index		= message_index + 1;
													tmp2				= *message_index;
													g_iAppMsg.PowerDat	+= (u16) (tmp1 << 8 | tmp2) * 5 / 3600;

													}

												if (Ql_memcmp(message.Set_Message, message_backup.Set_Message, 10))
													{
													//空调设置发生改变
													Air_Set = 1;

													//中断不处理，向主进程推送消息
													Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_SET_CHANGENED_MESSAGE,
														 NULL);
													Ql_memcpy(message_backup.Set_Message, message.Set_Message, 10);

													/*
													APP_DEBUG("Air Set is Changend! Back Data:\r\n");
													for (int i = 0; i < 42; i++)
													{
														APP_DEBUG("0x%02X,", Big_Data_All[i]);
													}
													APP_DEBUG("\r\n");
													*/
													}
												else 
													{
													Air_Set 			= 0;
													}

												if (Check_Condition_Rules())
													{
													//环境数据超过设定值,大于MAX，或小于MIN
													Air_State			= 1;

													//中断不处理，向主进程推送消息
													Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_CON_VUALE_MESSAGE,
														 NULL);

													//APP_DEBUG("Air Condition Value Max or Min!\r\n");
													}
												else 
													{
													Air_State			= 0;
													}

												if (frame_data.type == MESSAGE_TYPE_ALARM)
													{
													//中断不处理，向主进程推送报警消息
													Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_ALARM_MESSAGE, NULL);
													}

												message_backup		= message;
												}
											}
										}
									else 
										{
										//解状态消息
										if (frame_data.type == MESSAGE_TYPE_STATE ||
											 frame_data.type == MESSAGE_TYPE_ALARM)
											{
											u8 *			message_index;

											message_index		= &frame_data.buffer[ADDR_SIGN_LEN + 1];

											//測試用，一層層判斷，可不定長發送，進行測試
											if (frame_data.message_length > (ADDR_SIGN_LEN + 20)) //有效消息共30bit，除去开头命令报文2bit，28bit，大数据多10条/14bit，最大42bit
												{
												u8				tmp1, tmp2;
												
												v100_message.MESSAGE_TYPE = *message_index;

												message_index		= message_index + 2;


												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.TEMP = (u16) (tmp1 << 8 | tmp2);
												
												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.HHON = (u16) (tmp1 << 8 | tmp2);
												
												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.MMON = (u16) (tmp1 << 8 | tmp2);

												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.HHOFF = (u16) (tmp1 << 8 | tmp2);

												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.MMOFF = (u16) (tmp1 << 8 | tmp2);


												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.MODE = (u16) (tmp1 << 8 | tmp2);

												
												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.WIND = (u16) (tmp1 << 8 | tmp2);

												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.SOLIDH = (u16) (tmp1 << 8 | tmp2);

												
												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.WORDA = (u16) (tmp1 << 8 | tmp2);

												
												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.WORDB = (u16) (tmp1 << 8 | tmp2);

												
												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.HUMSD = (u16) (tmp1 << 8 | tmp2);

												message_index		= message_index + 1;
												tmp1				= *message_index;
												message_index		= message_index + 1;
												tmp2				= *message_index;
												v100_message.STEMP = (u16) (tmp1 << 8 | tmp2);												

												

												if ((v100_message.STEMP != v100_message_backup.STEMP)||(v100_message.MODE != v100_message_backup.MODE)||(v100_message.WIND != v100_message_backup.WIND) \
													||(v100_message.WIND != v100_message_backup.WIND)||(v100_message.WORDA != v100_message_backup.WORDA) \
													||(v100_message.WORDB != v100_message_backup.WORDB)||((v100_message.HUMSD & 0xFF00) != (v100_message_backup.HUMSD & 0xFF)))
													{
													//空调设置发生改变
													Air_Set 			= 1;

													//中断不处理，向主进程推送消息
													Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_SET_CHANGENED_MESSAGE,NULL);
												 
													//Ql_memcpy(v100_message_backup, v100_message, sizeof(v100_message));
													v100_message_backup.STEMP = v100_message.STEMP;
													v100_message_backup.HHOFF = v100_message.HHOFF;
													v100_message_backup.HUMSD = v100_message.HUMSD;
													v100_message_backup.MMOFF = v100_message.MMOFF;
													v100_message_backup.MMON = v100_message.MMON;
													v100_message_backup.MODE = v100_message.MODE;
													v100_message_backup.SOLIDH= v100_message.SOLIDH;
													v100_message_backup.TEMP = v100_message.TEMP;
													v100_message_backup.WIND = v100_message.WIND;
													v100_message_backup.WORDA = v100_message.WORDA;
													v100_message_backup.WORDB = v100_message.WORDB;
													

													/*
													APP_DEBUG("Air Set is Changend! Back Data:\r\n");
													for (int i = 0; i < 42; i++)
													{
														APP_DEBUG("0x%02X,", Big_Data_All[i]);
													}
													APP_DEBUG("\r\n");
													*/
													}
												else 
													{
													Air_Set 			= 0;
													}

												if (v100_message.TEMP > condition_rule.ROOMTMP_VALUE_MAX ||  v100_message.TEMP < condition_rule.ROOMTMP_VALUE_MIN || \
						         					 (v100_message.HUMSD & 0XFF) > condition_rule.ROOMHUM_VALUE_MAX ||  (v100_message.HUMSD & 0XFF) < condition_rule.ROOMHUM_VALUE_MIN || \
                                    				 v100_message.HHOFF > condition_rule.ROOM_PM_VALUE_MAX ||  v100_message.HHOFF < condition_rule.ROOM_PM_VALUE_MIN || \		
                                    				 (v100_message.HHON>>8 &0XFF) > condition_rule.VOC_VALUE_MAX ||  (v100_message.HHON>>8 &0XFF) < condition_rule.VOC_VALUE_MIN)
													{
													//环境数据超过设定值,大于MAX，或小于MIN
													Air_State			= 1;

													//中断不处理，向主进程推送消息
													Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_CON_VUALE_MESSAGE,
														 NULL);

													//APP_DEBUG("Air Condition Value Max or Min!\r\n");
													}
												else 
													{
													Air_State			= 0;
													}

												if (frame_data.type == MESSAGE_TYPE_ALARM)
													{
													//中断不处理，向主进程推送报警消息
													Ql_OS_SendMessage(0, MSG_ID_APP_PUSH, SEND_ALARM_MESSAGE, NULL);
													}

												message_backup		= message;
												}
											}

										}
									}
								}
							else 
								{
								//帧长为1，<或无负荷，或无校验位>
								APP_DEBUG("Frame Length = 1 \r\n");
								}
							}
						else 
							{
							//帧长为0，无意义
							APP_DEBUG("Frame Length = 0 \r\n");
							}

						break;
						}
					}

				break;
				}

			//发送事件已就绪

		case EVENT_UART_READY_TO_WRITE:
			break;

		default:
			break;
		}
}


/*
//AT指令回调函数
static s32 ATResponse_Handler(char* line, u32 len, void* userData)
{
	APP_DEBUG("[ATResponse_Handler] %s\r\n", (u8*)line);
	
	//读取一行，判断是否返回OK
	if (Ql_RIL_FindLine(line, len, "OK"))
	{  
		return	RIL_ATRSP_SUCCESS;
	}
	else if (Ql_RIL_FindLine(line, len, "ERROR"))
	{  
		return	RIL_ATRSP_FAILED;
	}
	else if (Ql_RIL_FindString(line, len, "+CME ERROR"))
	{
		return	RIL_ATRSP_FAILED;
	}
	else if (Ql_RIL_FindString(line, len, "+CMS ERROR:"))
	{
		return	RIL_ATRSP_FAILED;
	}
	//继续接收
	return RIL_ATRSP_CONTINUE; //continue wait
}
*/
