#include "network.h"
#include "uart.h"
#include "timer.h"
#include "BuildMsg.h"

#define AIR_TYPE 0x50

#define DEV_TYPE AIR_TYPE

#define UPDATA_MSG_CODE 0x99

#define UPDAT_POWER_MSG 0x0105

#define UPDAT_WARNING_INFO 0x0103

TimeData NB_Time;

TimeData China_Time;

typedef struct
{
    u8 imsi[16];
    u8 imei[16];
    u8 ccid[22];
    s16 rsrp;
    s16 snr;
    u32 cellid;
    u8 uPlusId[32];
    u8 ver[2];
    u8 csq;
    u8 sc_ecl;

} NB_INFO;

typedef struct
{
    u8 sc_earfcn[10];
    u8 sc_earfcn_offset[10];
    u8 sc_pci[10];
    u8 sc_cellid[10];
    s16 sc_rsrp[10];
    u8 sc_rsrq[10];
    u8 sc_rssi[10];
    s16 sc_snr[10];
    u8 sc_band[10];
    u8 sc_tac[10];
    u8 sc_ecl[10];
    u8 sc_tx_pwr[10];
} RECV_QENG;

RECV_QENG RecvQeng_Buff;

NB_INFO NB_info;

u8 Net_Recv_Buffer[RECV_BUFFER_LEN];

u8 Net_Recv_Ctrl_Buffer[512];

u8 m_lwm2m_state = STATE_NW_QUERY_STATE;

int Lwm2m_Send_Len = 0;

u16 Data_Buffer_Len = 0;

u8 Comp_Temp[800];

u8 Lwm2m_Send_Buffer[1000];

char strQENG[256];

char strImei[60];

char strImsi[60];

char strTime[60];

u8 message_id = 0x00;

char strQCCID[60];

int Send_Data_type = 0;

/*****************************************************************
*  LwM2M Param
******************************************************************/
ST_Lwm2m_Send_Param_t lwm2m_send_param_t = {0, 0, 0, 0, NULL, 0};
Lwm2m_Urc_Param_t *lwm2m_urc_param_ptr = NULL;

static u8 test_data[128] = "011111\0"; //send data
u8 res_id[5] = "0\0";                  // Resources id.

bool lwm2m_access_mode = LWM2M_ACCESS_MODE_DIRECT;
//s32 recv_actual_length = 0;
//s32 recv_remain_length = 0;

//定时器回掉
static void Callback_Timer(u32 timerId, void *param);

//u8转uint
#define stdcn_u8s_to_u32(p, var)                               \
    {                                                          \
        var = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3] << 0; \
    }

//过滤字符串中特定字符
void Squeeze(char s[], int c)
{
    int i, j;
    for (i = 0, j = 0; s[i] != '\0'; i++)
    {
        if (s[i] != c)
        {
            s[j++] = s[i];
        }
    }
    s[j] = '\0';
}

//UTC时间转北京时间
TimeData UTCToChina(int year, int mon, int day, int hour, int min, int sec)
{
    TimeData ChinaTime;

    int ChinaYear = 2019, ChinaMonth = 4, ChinaDay = 22, ChinaSeconds = 0, ChinaMinutes = 0, ChinaHour = 0;

    ChinaYear = year;
    ChinaMonth = mon;
    ChinaDay = day;
    ChinaSeconds = sec;
    ChinaMinutes = min;
    ChinaHour = hour + 8;

    if (ChinaHour > 23)
    {
        ChinaHour -= 24;
        ChinaDay++;
        switch (ChinaMonth)
        {
        case 1:
            if (ChinaDay > 31)
            {
                ChinaDay = 1;
                ChinaMonth++;
            }
            break;
        case 2:
            if ((0 == ChinaYear % 4 && 0 != ChinaYear % 100) || 0 == ChinaYear % 400)
            {

                if (ChinaDay > 29)
                {
                    ChinaDay = 1;
                    ChinaMonth++;
                }
            }
            else
            {
                if (ChinaDay > 28)
                {
                    ChinaDay = 1;
                    ChinaMonth++;
                }
            }
            break;
        case 3:
            if (ChinaDay > 31)
            {
                ChinaDay = 1;
                ChinaMonth++;
            }
            break;
        case 4:
            if (ChinaDay > 30)
            {
                ChinaDay = 1;
                ChinaMonth++;
            }
            break;
        case 5:
            if (ChinaDay > 31)
            {
                ChinaDay = 1;
                ChinaMonth++;
            }
            break;
        case 6:
            if (ChinaDay > 30)
            {
                ChinaDay = 1;
                ChinaMonth++;
            }
            break;
        case 7:
            if (ChinaDay > 31)
            {
                ChinaDay = 1;
                ChinaMonth++;
            }
            break;
        case 8:
            if (ChinaDay > 31)
            {
                ChinaDay = 1;
                ChinaMonth++;
            }
            break;
        case 9:
            if (ChinaDay > 30)
            {
                ChinaDay = 1;
                ChinaMonth++;
            }
            break;
        case 10:
            if (ChinaDay > 31)
            {
                ChinaDay = 1;
                ChinaMonth++;
            }
            break;
        case 11:
            if (ChinaDay > 30)
            {
                ChinaDay = 1;
                ChinaMonth++;
            }
            break;
        case 12:
            if (ChinaDay > 31)
            {
                ChinaDay = 1;
                ChinaMonth = 1;
                ChinaYear++;
            }
            break;
        default:
            break;
        }
    }

    ChinaTime.year = ChinaYear;
    ChinaTime.mon = ChinaMonth;
    ChinaTime.day = ChinaDay;
    ChinaTime.hour = ChinaHour;
    ChinaTime.min = ChinaMinutes;
    ChinaTime.sec = ChinaSeconds;
    return ChinaTime;
}

//十六进制字符串转Byte(u8数组)
void HexStrToByte(const char *source, unsigned char *dest, int sourceLen)
{
    short i;
    unsigned char highByte, lowByte;

    for (i = 0; i < sourceLen; i += 2)
    {
        highByte = toupper(source[i]);
        lowByte = toupper(source[i + 1]);

        if (highByte > 0x39)
            highByte -= 0x37;
        else
            highByte -= 0x30;

        if (lowByte > 0x39)
            lowByte -= 0x37;
        else
            lowByte -= 0x30;

        dest[i / 2] = (highByte << 4) | lowByte;
    }
	
    return;
}

//字符转换
void HexToStr(char *outhex, const char *inputstr, int hex_len)
{
    int i, len = 0;
    unsigned char tmp = 0;

    for (i = 0; i < hex_len; i++)
    {
        tmp = inputstr[i] >> 4;
        if (tmp > 9)
            *outhex++ = tmp + 0x37;
        else
            *outhex++ = tmp + '0';
        len++;
        tmp = inputstr[i] & 0xF;
        if (tmp > 9)
            *outhex++ = tmp + 0x37;
        else
            *outhex++ = tmp + '0';
        len++;
    }
}


int tran2uper(char* str)
{
	for(int i = 0; i <= Ql_strlen(str); i++)
	{
		if((str[i] >= 'a') && (str[i] <= 'z'))
			str[i] = Ql_toupper(str[i]);
		else
			str[i] = str[i];
	}
}

//hexString transform to ascString
int hexStr2ascStr(char* str)
{
	char outString[128] = "\0";
	char str1;
	for(int i = 0,p = 0; i < Ql_strlen(str); i+= 2,p++)
	{
		if((str[i+1] >= 'A') && (str[i+1] <= 'F'))
		{
			int f = 10 + str[i+1] - 'A';
			str1 = (str[i]-48)*16 + f;
		}
		else
		{
			str1 = (str[i]-48)*16 + (str[i+1]-48);
		}
		outString[p] = str1;
	}
	Ql_strcpy(str, outString);
}

//HexString out HexInt, eg: char string[128] = "EFA0010203040500000000000000000000B5\0"; ,out 0xEF, 0xA0 ...
int hexStr2hexInt(char* str, int* hex)
{
	//int outHex[1024];
	int temp = 0;
	for(int i = 0,p = 0; i < Ql_strlen(str); i+=2, p++)
	{
		if((str[i] >= 'A') && (str[i] <= 'F'))
		{
			if((str[i+1] >= 'A') && (str[i+1] <= 'F'))
			{
				temp = (10 + str[i] - 'A')*16 + (10 + str[i+1] - 'A');
			}
			else
			{
				temp = (10 + str[i] - 'A')*16 + (str[i+1] - 48);
			}
		}
		else if((str[i+1] >= 'A') && (str[i+1] <= 'F'))
		{
			temp = (str[i] - 48)*16 + (10 + str[i+1] - 'A');
		}
		else
		{
			temp = (str[i] - 48)*16 + (str[i+1] - 48);
		}
		//Ql_sprintf(temp, "%0X", temp);
		//outHex[p] = temp;
		hex[p] = temp;
	}

	//hex = outHex;
}

//get gps data in position, eg: str = "$GNRMC,020348.000,A,2232.4714,N,11356.9154,E,0.18,0.00,201118,,,A*71", position = 1, return is "$GNRMC"
char* get_part(char* str, int position)
{
        char *p, *q;
        char string[128];
        Ql_memset(string, 0x0, sizeof(string));

        Ql_strcpy(string, str);
        p = string;
        for(int i = 0; i < position; i++)
        {
                q = p;
                p = Ql_strstr(p, ",");
                *p = '\0';
                p++;
                if(i == (position - 1))
                {
                        return q;
                }
        }

}

char* get_gpspart(char* str, int start, int end)
{
        char *p;
        char string[128];
        Ql_memset(string, 0x0, sizeof(string));

        Ql_strcpy(string, str);
        p = string;

        for(int i = 0; i < end; i++)
        {
                p = Ql_strstr(p, ",");
                if(p == NULL)
                        return NULL;
                p++;
                if(i == (end - 1))
                        *--p = '\0';
        }

        p = string;
        for(int j = 0; j < start; j++)
        {
                p = Ql_strstr(p, ",");
                if(p == NULL)
                        return NULL;
                p++;
                if(j == (start - 1))
                        return p++;
        }

}


static bool res_status = FALSE;
static bool set_serv_param = FALSE;



//Int trainsform to String like "itoa" in lib. eg: in: 1234, out:"1234"
void int2str(u32 n, u8* str)
{
	char buf[10] = "";
	int i = 0;
	int len = 0;
	int temp = n<0 ? -n : n;

	if(str == NULL)
	{
		return;
	}
	while(temp)
	{
		buf[i++] = (temp%10) + '0';
		temp = temp/10;
	}
	
	len = n<0 ? ++i : i;
	str[i] = 0;
	while(1)
	{
		i--;
		if(buf[len-i-1] == 0)
		{
			break;
		}
		str[i] = buf[len-i-1];
	}
	if(i == 0)
	{
		str[i] = '-';
	}
}

//ASCII String trainsform to HexString, eg: in: "1234@" , out: "3132333440"

#define STR_MAX 512

int asc2hex(char asc[])
{

        char buf[STR_MAX],str[2*STR_MAX],p;
        Ql_memset(buf, 0x0, sizeof(buf));
        Ql_memset(str, 0x0, sizeof(str));
        Ql_strcpy(buf, asc);

        for(int i = 0; asc[i] != '\0'; i++){
                p = asc[i];

                Ql_sprintf(buf, "%X", p);
                for(int j = 0; buf[j] != '\0'; j++){
                        str[Ql_strlen(str)] = buf[j];
                }
        }
		Ql_strcpy(asc, str);
        return 0;
}

int outhex(char* str, u32 hexbit)
{
        u32 len = Ql_strlen(str);
        u8 strlength[10] = "\0";
        u8 string[128] = "\0";
		u8 format[10] = "\0";
        Ql_memset(string, 0x0, sizeof(string));

		Ql_sprintf(format, "%%0%dX", hexbit);
        Ql_sprintf(strlength, format, len);
        Ql_strcat(string, strlength);
        asc2hex(str);
        Ql_strcat(string, str);
        Ql_strcpy(str, string);
        return 0;
}

//初始化网络定时器
s32 Init_Network(void)
{
    s32 ret;
    //register
    //注网定时器
    ret = Ql_Timer_Register(LwM2M_TIMER_ID, Callback_Timer, NULL);
    APP_DEBUG("<-- Network Timer Register(%d) -->\r\n", ret);
    return ret;
}

//开始注网
s32 Start_Network(void)
{
    s32 ret;
    ret = Ql_Timer_Start(LwM2M_TIMER_ID, LwM2M_TIMER_PERIOD, TRUE);
    APP_DEBUG("<-- Network Timer Start(%d) -->\r\n", ret);


	//LED定时器
	ret =Ql_Timer_Start(Led_timer, 150, TRUE);
	APP_DEBUG("<-- LED Timer Start(%d) -->\r\n", ret);	
    return ret;
}

void Recv_Data_Handle(void)
{
    u8 *index,Dat[200],rst;
	int i;
    Ql_memset(Net_Recv_Buffer, 0x0, sizeof(Net_Recv_Buffer));

	led_blink_state = STATE_DATA_RECV;
    //index = &Net_Recv_Buffer[2];

    //Ql_strcpy(Net_Recv_Buffer, lwm2m_urc_param_ptr->recv_buffer);
    //hexStr2hexInt(lwm2m_urc_param_ptr->recv_buffer, Net_Recv_Buffer, Ql_strlen(lwm2m_urc_param_ptr->recv_buffer));

#if 0
	hexStr2hexInt(lwm2m_urc_param_ptr->recv_buffer, Net_Recv_Buffer);
	for( i = 0; i <= Ql_strlen(lwm2m_urc_param_ptr->recv_buffer)/2 - 1; i++)
	{
		Dat[i] = Net_Recv_Buffer[i]&0xFF;
		APP_DEBUG("<--data[%d] is %02X.-->\r\n",i, Dat[i]);
		
		
	}
#endif


#if  1
    if(1)
    {
    
		char recv_buf[1024] = "\0";
		
		Ql_memset(recv_buf,0,sizeof(recv_buf));
		Ql_strcpy(recv_buf, lwm2m_urc_param_ptr->recv_buffer);
	#if 0	
		hexStr2ascStr(recv_buf);
		tran2uper(recv_buf);
		APP_DEBUG("<-- recv_buf = %s\r\n",recv_buf);
	#endif	
		u32 hex[500];
		hexStr2hexInt(recv_buf, hex);
		for( i = 0; i <= Ql_strlen(recv_buf)/2 - 1; i++)
		{
			APP_DEBUG("<--data[%d] is %02X.-->\r\n",i, hex[i]);
			
			Dat[i] = hex[i]&0xFF;
		}
		
		rst = iMax35_MainProc(&giMax35Msg,Dat,i);

		for(i =0; i < giMax35Msg.DataLen; i++)
		{
			APP_DEBUG("<--data[%d] is %02X.-->\r\n",i,giMax35Msg.Data[i]);	
		}

	
		if(rst)
		{
			APP_DEBUG("<--Cmd is %04X.-->\r\n",giMax35Msg.Cmd);	
			APP_DEBUG("<--code is %02X.-->\r\n",giMax35Msg.ctrCode);
			NB_DataPro();
		}

		 //Ql_UART_Write(m_myUartPort, (u8 *)giMax35Msg.Data, giMax35Msg.DataLen); //从串口透传
    }


#endif


#if 0
  //  if (Net_Recv_Buffer[0] == SERVER_DOWN_CTRL_ID) // MessageId 后期可定义，通过MessageId辨别”服务“，从而判断下行与应答
    {
        APP_DEBUG("\r\nRecv Data Byte:");
        //Debug输出
        for (int i = 1; i < lwm2m_urc_param_ptr->recv_length; i++)
        {
            APP_DEBUG("0x%02X,", Net_Recv_Buffer[i]);
        }
        APP_DEBUG("\r\n");

        if (lwm2m_urc_param_ptr->recv_length > 12) //长度符合要求
        {
            switch (Net_Recv_Buffer[5])
            {
            case SERVER_DOWN_CTRL:
                //下发控制数据
                index = &Net_Recv_Buffer[9];                                                                    //索引到应用数据
                Ql_memcpy(Net_Recv_Ctrl_Buffer, index, lwm2m_urc_param_ptr->recv_length - 12);                  //recv_length为总长度，减去message（1byte），其他数据(11byte)即为长度
                Ql_UART_Write(m_myUartPort, (u8 *)Net_Recv_Ctrl_Buffer, lwm2m_urc_param_ptr->recv_length - 12); //从串口透传
                APP_DEBUG("\r\nSend Uart Byte:");
                //Debug输出
                for (int i = 1; i < lwm2m_urc_param_ptr->recv_length - 12; i++)
                {
                    APP_DEBUG("0x%02X,", Net_Recv_Ctrl_Buffer[i]);
                }
                APP_DEBUG("\r\n");
                break;
            default:
                break;
            }
        }
    }
#endif  
}

void Push_Data_Header(u8 Message_ctrl, u8 Message_sign, u16 Message_Len)
{
    u8 *index;
    index = &Lwm2m_Send_Buffer[0];
    *index = 0x68; //标识位
    index++;

    *index = DEV_TYPE; //设备类型
    index++;

    *index = (Message_Len >> 8) & 0xFF;
    index++;
    *index = Message_Len & 0xff;
    index++;

    *index = Message_ctrl;
    index++;

    *index = message_id;
    message_id++;
    index++;

    *index = (Message_sign >> 8) & 0xFF;
    index++;
    *index = Message_sign & 0xff;
    index++;

    *index = (NB_info.rsrp >> 8) & 0xFF;
    index++;
    *index = NB_info.rsrp & 0xff;
    index++;

    *index = (NB_info.snr >> 8) & 0xFF;
    index++;
    *index = NB_info.snr & 0xff;
    index++;

    *index = (NB_info.cellid >> 24) & 0xFF;
    index++;
    *index = (NB_info.cellid >> 16) & 0xFF;
    index++;
    *index = (NB_info.cellid >> 8) & 0xFF;
    index++;
    *index = (NB_info.cellid & 0xFF);
    index++;
    *index = NB_info.sc_ecl;
}

//获取网络时间
void Get_Net_Time(void)
{
    int i = 0;
    char *level = NULL;
    char SEC_BUF[3];
    char BUF_TMP[64];

    Ql_memset(SEC_BUF, 0, 3);
    Ql_memset(BUF_TMP, 0, 64);
    Ql_memset(strTime, 0, 60);
/*	
    do
    {
        RIL_GetTIME(strTime);
        Ql_Sleep(50);
    } while (strTime[0] == 0x00 && i < 10);
 */   
    Squeeze(strTime, '\r');
    Squeeze(strTime, '\n');

    strcpy(BUF_TMP, strTime);
    level = strtok((char *)BUF_TMP, ",");
    while (level != NULL)
    {
        char *level_2 = NULL;
        if (i == 0)
        {
            level_2 = strtok(level, "/");
            while (level_2 != NULL)
            {
                switch (i)
                {
                case 0:
                    NB_Time.year = atoi(level_2);
                    break;
                case 1:
                    NB_Time.mon = atoi(level_2);
                    break;
                case 2:
                    NB_Time.day = atoi(level_2);
                    break;
                }
                level_2 = strtok(NULL, "/");
                i++;
            }
        }
        level = strtok(NULL, ",");
        i++;
    }
    i = 0;
    level = strtok((char *)strTime, ",");
    while (level != NULL)
    {
        char *level_3 = NULL;
        if (i > 0)
        {
            level_3 = strtok(level, ":");
            while (level_3 != NULL)
            {
                switch (i)
                {
                case 1:
                    NB_Time.hour = atoi(level_3);
                    break;
                case 2:
                    NB_Time.min = atoi(level_3);
                    break;
                case 3:
                    strncpy(SEC_BUF, level_3, 2);
                    NB_Time.sec = atoi(SEC_BUF);
                    break;
                }
                level_3 = strtok(NULL, ":");
                i++;
            }
        }
        level = strtok(NULL, ",");
        i++;
    }
}

//获取设备信息
void Get_Device_Msg(void)
{
    APP_DEBUG("<-- Get Device Info -->\r\n");
    //获取网络时间
    Get_Net_Time();
    China_Time =  UTCToChina(NB_Time.year, NB_Time.mon, NB_Time.day, NB_Time.hour, NB_Time.min, NB_Time.sec);
    ST_Time st_time;
    st_time.year = China_Time.year;
    st_time.month = China_Time.mon;
    st_time.day = China_Time.day;
    st_time.hour = China_Time.hour;
    st_time.minute = China_Time.min;
    st_time.second = China_Time.sec;
    st_time.timezone = 8; // The range is(-11~12).one digit expresses an hour, for example: 8 indicates "GMT+8"
    Set_Local_Time(st_time);

    int i = 0;
    char *level = NULL;
    Ql_memset(strQENG, 0, 256);
    do
    {
        RIL_GetQENG(strQENG);
        Ql_Sleep(50);
        i++;
    } while (strQENG[0] == 0x00 && i < 10);

    i = 0;
    level = strtok((char *)strQENG, ",");
    while (level != NULL)
    {
        switch (i)
        {
        case 0:
            strcpy((char *)RecvQeng_Buff.sc_earfcn, level);
            break;
        case 1:
            strcpy((char *)RecvQeng_Buff.sc_earfcn_offset, level);
            break;
        case 2:
            strcpy((char *)RecvQeng_Buff.sc_pci, level);
            break;
        case 3:
            strcpy((char *)RecvQeng_Buff.sc_cellid, level);
            break;
        case 4:
            strcpy((char *)RecvQeng_Buff.sc_rsrp, level);
            break;
        case 5:
            strcpy((char *)RecvQeng_Buff.sc_rsrq, level);
            break;
        case 6:
            strcpy((char *)RecvQeng_Buff.sc_rssi, level);
            break;
        case 7:
            strcpy((char *)RecvQeng_Buff.sc_snr, level);
            break;
        case 8:
            strcpy((char *)RecvQeng_Buff.sc_band, level);
            break;
        case 9:
            strcpy((char *)RecvQeng_Buff.sc_tac, level);
            break;
        case 10:
            strcpy((char *)RecvQeng_Buff.sc_ecl, level);
            break;
        case 11:
            strcpy((char *)RecvQeng_Buff.sc_tx_pwr, level);
            break;
        }
        level = strtok(NULL, ",");
        i++;
    }
    i = 0;
    do
    {
        RIL_GetIMEI(strImei);
        Ql_Sleep(50);
        i++;
    } while (strImei[0] == 0x00 && i < 10);

    i = 0;
    do
    {
        RIL_GetIMSI(strImsi);
        Ql_Sleep(50);
        i++;
    } while (strImsi[0] == 0x00 && i < 10);

    i = 0;
    do
    {
        RIL_GetCCID(strQCCID);
        Ql_Sleep(50);
        i++;
    } while (strQCCID[0] == 0x00 && i < 10);

    memcpy(g_nb_conn.imei, strImei, 15);

    Squeeze(strImsi, '\r');
    Squeeze(strImsi, '\n');

    Squeeze(strQCCID, '\r');
    Squeeze(strQCCID, '\n');

    APP_DEBUG("<-- QCCID ,Value=%s -->\r\n", strQCCID);

    memcpy(g_nb_conn.imsi, strImsi, 15);

    memcpy(g_nb_conn.ccid, strQCCID, 20);

    g_nb_conn.rsrp = atoi(RecvQeng_Buff.sc_rsrq);
    //Ql_memcpy(g_nb_conn.rsrp, RecvQeng_Buff.sc_rsrq,2);
    g_nb_conn.snr = atoi(RecvQeng_Buff.sc_snr);
    APP_DEBUG("<-- QENG ,Value=%s,%s,%s -->\r\n", RecvQeng_Buff.sc_rsrq, RecvQeng_Buff.sc_snr, RecvQeng_Buff.sc_ecl);
    APP_DEBUG("<-- NB_info.rsrp ,Value=%d -->\r\n", g_nb_conn.rsrp);
    APP_DEBUG("<-- NB_info.snr ,Value=%d -->\r\n", g_nb_conn.snr);

    u8 Cellid_Temp[4] = {0x00, 0x00, 0x00, 0x00}, Cellid_String[10] = "0";

    Squeeze(RecvQeng_Buff.sc_cellid, '\"');

    strcat((char *)Cellid_String, RecvQeng_Buff.sc_cellid);

    APP_DEBUG("<-- Cellid ID:%s. -->\r\n", Cellid_String);

    HexStrToByte(Cellid_String, Cellid_Temp, 8);

    APP_DEBUG("<-- Cellid Hex:0x%02X,0x%02X,0x%02X,0x%02X -->\r\n", Cellid_Temp[0], Cellid_Temp[1], Cellid_Temp[2], Cellid_Temp[3]);

    stdcn_u8s_to_u32(Cellid_Temp, g_nb_conn.cellid);

    g_nb_conn.sc_ecl = atoi(RecvQeng_Buff.sc_ecl);

    APP_DEBUG("<-- NB_info.sc_ecl ,Value=%d -->\r\n", g_nb_conn.sc_ecl);

    APP_DEBUG("<-- Cellid ID , Dump Value=%x -->\r\n", g_nb_conn.cellid);
}

void Push_Data_Body(u8 Message_ctrl, u8 Message_sign, u16 Message_Len)
{
}

//格式化数据
void Format_Data(u8 *Build_Temp, u16 Send_Len)
{
    char Send_Len_Buf[10];
	u8 Send_Data_Hex[1000];
	
	
	

    Ql_memset(Send_Data_Hex, 0, sizeof(Send_Data_Hex));
    Ql_memset(Comp_Temp, 0, sizeof(Comp_Temp));

    Ql_memset(Send_Len_Buf, 0, 10);

    HexToStr(Send_Data_Hex, Build_Temp, Send_Len);
#if 0
    Ql_sprintf(Send_Len_Buf, "%0.4X", Send_Len);
    Ql_strcat(Comp_Temp, "00");

    Ql_strcat(Comp_Temp, Send_Len_Buf);
#endif	
    Ql_strcat(Comp_Temp, Send_Data_Hex);

    //APP_DEBUG(Comp_Temp);

    Send_Len = Ql_strlen(Comp_Temp);

    Lwm2m_Send_Len = Send_Len / 2;
}

//定时器回调
static void Callback_Timer(u32 timerId, void *param)
{
    s32 ret;
    if (LwM2M_TIMER_ID == timerId)
    {
        //APP_DEBUG("<--...........m_lwm2m_state=%d..................-->\r\n",m_lwm2m_state);
        switch (m_lwm2m_state)
        {
        case STATE_NW_QUERY_STATE:
        {
            s32 cgreg = 0;
            ret = RIL_NW_GetEGPRSState(&cgreg);
            APP_DEBUG("<-- Network State:cgreg=%d -->\r\n", cgreg);
            if ((cgreg == NW_STAT_REGISTERED) || (cgreg == NW_STAT_REGISTERED_ROAMING))
            {
                m_lwm2m_state = STATE_LwM2M_SERV;
            }
            break;
        }

        case STATE_LwM2M_SERV:
        {
            ret = RIL_QLwM2M_Serv(m_SrvADDR, m_SrvPort);
            if (RIL_AT_SUCCESS == ret)
            {
                APP_DEBUG("<-- configure address and port successfully -->\r\n");
                m_lwm2m_state = STATE_LwM2M_CONF;
            }
            else
            {
                APP_DEBUG("<-- configure address and port failure, error=%d. -->\r\n", ret);
            }
            break;
        }

        case STATE_LwM2M_CONF:
        {
            Ql_memset(strImei, 0x0, sizeof(strImei));

            ret = RIL_GetIMEI(strImei);
            APP_DEBUG("<-- IMEI:%s,ret=%d -->\r\n", strImei, ret);

            ret = RIL_QLwM2M_Conf(strImei);
            if (RIL_AT_SUCCESS == ret)
            {
                APP_DEBUG("<-- Configure Parameters successfully -->\r\n");
                m_lwm2m_state = STATE_LwM2M_ADDOBJ;
            }
            else
            {
                APP_DEBUG("<-- Configure Parameters failure,error=%d. -->\r\n", ret);
            }
            break;
        }
        case STATE_LwM2M_ADDOBJ:
        {
            lwm2m_send_param_t.obj_id = 19; //Object id. The max object id number is 65535
            lwm2m_send_param_t.ins_id = 0;  //Instance id,uplink
            lwm2m_send_param_t.res_num = 0; //Resources id number

            ret = RIL_QLwM2M_Addobj(lwm2m_send_param_t.obj_id, lwm2m_send_param_t.ins_id, lwm2m_send_param_t.res_num, res_id);
            if (RIL_AT_SUCCESS != ret)
            {
                APP_DEBUG("<-- Add object failure,ret=%d. -->\r\n", ret);
            }

            lwm2m_send_param_t.ins_id = 1; //Instance id,downlink
            ret = RIL_QLwM2M_Addobj(lwm2m_send_param_t.obj_id, lwm2m_send_param_t.ins_id, lwm2m_send_param_t.res_num, res_id);
            if (RIL_AT_SUCCESS == ret)
            {
                m_lwm2m_state = STATE_LwM2M_CFG;
                APP_DEBUG("<-- Add object successfully -->\r\n");

                break;
            }
            else if (ret < 0)
            {
                APP_DEBUG("<-- Add object failure,ret=%d. -->\r\n", ret);
            }
            break;
        }
        case STATE_LwM2M_CFG:
        {
        	Ql_Timer_Stop(Led_timer);

            ret = RIL_QLwM2M_Cfg(LWM2M_DATA_FORMAT_HEX, LWM2M_DATA_FORMAT_HEX);
            if (RIL_AT_SUCCESS == ret)
            {
                APP_DEBUG("<-- Configure Optional Parameters successfully -->\r\n");
                m_lwm2m_state = STATE_LwM2M_OPEN;
            }
            else
            {
                APP_DEBUG("<-- Configure Optional Parameters failure,error=%d. -->\r\n", ret);
            }
            break;
        }
        case STATE_LwM2M_OPEN:
        {
            Ql_Timer_Stop(LwM2M_TIMER_ID);
            ret = RIL_QLwM2M_Open(lwm2m_access_mode);
            if (RIL_AT_SUCCESS == ret)
            {
                APP_DEBUG("<-- Open a Register Request successfully -->\r\n");
            }
            else
            {
                APP_DEBUG("<-- Open a Register Request failure,error=%d. -->\r\n", ret);
            }

            break;
        }

        case STATE_LwM2M_SEND:
        {

			led_blink_state = STATE_APP_RUN;
            Ql_Timer_Start(Led_timer, 1000, TRUE);
            //关闭定时器
            Ql_Timer_Stop(LwM2M_TIMER_ID);
            //获取设备信息
            Get_Device_Msg();
            //打包注网信息
			
			
            PowerOnRegInfo();

			g_rand_time = GetRandTime();

            //Debug输出
            for (int i = 0; i < Data_Buffer_Len; i++)
            {
                APP_DEBUG("0x%02X,", SendMsgBuf[i]);
            }
            APP_DEBUG("\r\n");

            Send_Msg_to_Server();

            if (ret == 0)
            {
                //发送注网消息成功，成功入网
                APP_DEBUG("\r\n<-- Send data Successfully. -->\r\n");
                APP_DEBUG("\r\n<-- Stop the Timer. -->\r\n");
                m_lwm2m_state = STATE_TOTAL_NUM;

                //开启Timer定时器，每五秒一次
                ret = Ql_Timer_Start(Stack_timer, Timer_Count_5s, TRUE);
                if (ret < 0)
                {
                    APP_DEBUG("\r\n<-- failed!! stack timer,Ql_Timer_Start,ret=%d -->\r\n", ret);
                }
                APP_DEBUG("\r\n<-- stack timer,Ql_Timer_Start(Id=%d,Interval=%d),ret=%d -->\r\n", Stack_timer, Timer_Count_5s, ret);

                if (lwm2m_send_param_t.lwm2m_send_mode == LWM2M_SEND_MODE_CON)
                {
                    APP_DEBUG("\r\n<-- Sent to the Server. -->\r\n");
                }
            }
            else
            {
                //发送注网信息失败，重开定时器，再次尝试
                m_lwm2m_state = STATE_LwM2M_SEND;
                Ql_Timer_Start(LwM2M_TIMER_ID, LwM2M_TIMER_PERIOD, TRUE);
                APP_DEBUG("\r\n<-- Send data Failure. -->\r\n");
            }
            if (lwm2m_send_param_t.buffer != NULL)
            {
                Ql_MEM_Free(lwm2m_send_param_t.buffer);
                lwm2m_send_param_t.buffer = NULL;
            }

            break;
        }
#if 0 //Direct push mode does not need this case
			case STATE_LwM2M_RD:
            {
    			Ql_memset(m_recv_buf,0,RECV_BUFFER_LEN);

				ret = RIL_QLwM2M_RD(lwm2m_urc_param_ptr->recv_length,&recv_actual_length,&recv_remain_length,m_recv_buf);
				if (RIL_AT_SUCCESS == ret)
				{
                   if(recv_actual_length == 0)
                   {
     	              APP_DEBUG("<--The buffer has been read empty\r\n");
     				  return;
     			   }
     			   APP_DEBUG("<-- read data successfully,recv data(%s)\r\n",m_recv_buf);
    			   if(recv_remain_length == 0)
    			   {
    					m_lwm2m_state = STATE_LwM2M_SEND;
    			   }
    			   else
    			   {
    				    m_lwm2m_state = STATE_LwM2M_RD;//continue read buffer until the reamin length returns zero.
    			   }	
				}
				else 
     			{
                   m_lwm2m_state = STATE_LwM2M_CLOSE;
     			}
				Ql_Timer_Stop(LwM2M_TIMER_ID);
				Ql_Timer_Start(LwM2M_TIMER_ID, LwM2M_TIMER_PERIOD, FALSE);
				break;
            }
#endif
        case STATE_LwM2M_CLOSE:
        {
            ret = RIL_QLwM2M_Close();
            if (RIL_AT_SUCCESS == ret)
            {
                APP_DEBUG("<-- Send a Deregister Request successfully\r\n");
                m_lwm2m_state = STATE_TOTAL_NUM;
            }
            else
            {
                APP_DEBUG("<--Send a Deregister Request failure,error=%d.-->\r\n", ret);
            }
            break;
        }
        case STATE_TOTAL_NUM:
        {

            APP_DEBUG("<-- Waiting Stop -->\r\n", ret);
            m_lwm2m_state = STATE_TOTAL_NUM;
        }
        default:
            break;
        }
    }
}

s32 Send_Msg_to_Server(void)
{
    s32 ret;
    //格式化数据

	//Debug输出
	APP_DEBUG("\r\nSendData:   Sendlen: %d\r\n ",Data_Buffer_Len);
	for (int i = 0; i < Data_Buffer_Len; i++)
	{
		APP_DEBUG("%02X ", SendMsgBuf[i]);
	}
	APP_DEBUG("\r\n");


	
    Format_Data(SendMsgBuf, Data_Buffer_Len);


	

    lwm2m_send_param_t.send_length = Lwm2m_Send_Len;

    lwm2m_send_param_t.buffer = (u8 *)Ql_MEM_Alloc(sizeof(u8) * 1200);

    Ql_memset(lwm2m_send_param_t.buffer, 0, 1200);

    Ql_strcpy(lwm2m_send_param_t.buffer, Comp_Temp);

    lwm2m_send_param_t.lwm2m_send_mode = LWM2M_SEND_MODE_NON;

    ret = RIL_QLwM2M_Send(&lwm2m_send_param_t);

//	Ql_MEM_Free(lwm2m_send_param_t.buffer);


	led_blink_state = STATE_DATA_POST;
    return ret;
}
