#include "BuildMsg.h"
#include "network.h"
#include "uart.h"

#define BUILDMSG_GLOBALS

#define WY_BUG 1

#define BIN2BCD(val) ((((val) / 10) << 4) + (val) % 10)
#define envBufListLen 24


iApp_Msg g_iAppMsg;

u8 g_ctr_down_flg = 0;
u8 g_ctr_up_data_len=0;
u8 g_ctr_up_data[200];


u16 SN = 0x01; //流水号

#define BIG_DATA_MSG 0x00
#define ENV_DATA_MSG 0x01
#define POWER_DATA_MSG 0x02

u8 SendMsgBuf[1024] = {0};


u16 cal_crc(u8 *ptr, u16 len)
{
	u16 crc;
	u16 i;
	crc = 0;
	while (len-- != 0)
	{
		for (i = 0x80; i != 0; i /= 2)
		{
			if ((crc & 0x8000) != 0)
			{
				crc *= 2;
				crc ^= 0x1021;
			}
			else
				crc *= 2;
			if ((*ptr & i) != 0)
				crc ^= 0x1021;
		}
		ptr++;
	}
	return (crc);
}

enum _stdcn_flag
{
	STDCN_FLG_NONE = 0,
	STDCN_FLG_REG_REQ,
	STDCN_FLG_REG_OK,
	STDCN_FLG_LOGIN,
	STDCN_FLG_LOGIN_OK,

};


//挂机设备
const u8 PutDev[32] = {0x20,0x1c,0x12,0x00,0x24,0x00,0x08,0x10,0x02,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40};

//柜机设备
const u8 CabDev[32] = {0x20,0x1c,0x12,0x00,0x24,0x00,0x08,0x10,0x03,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40};

//商用设备
const u8 BusDev[32] = {0x20,0x1c,0x12,0x00,0x24,0x00,0x08,0x10,0x0d,0x82,0x01,0x51,0x80,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40};

//#define uPlus_ID  PutDev  

#define stdcn_pack_dat_max 500
//INT8U   SendMsgBuf[600];

/*----------------------------------------------------------------------------------*/
#define stdcn_u16_to_u8s(p, var) \
	{                              \
		p[0] = (var >> 8) & 0xFF;    \
		p[1] = (var & 0xFF);         \
	}
#define stdcn_u32_to_u8s(p, var) \
	{                              \
		p[0] = (var >> 24) & 0xFF;   \
		p[1] = (var >> 16) & 0xFF;   \
		p[2] = (var >> 8) & 0xFF;    \
		p[3] = (var & 0xFF);         \
	}
#define stdcn_u8s_to_u16(p, var) \
	{                              \
		var = p[0] << 8 | p[1];      \
	}
#define stdcn_u8s_to_u32(p, var)                           \
	{                                                        \
		var = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3] << 0; \
	}

#define stdcn_BIN2BCD(val) ((((val) / 10) << 4) + (val) % 10)
#define stdcn_STR2BCD(p, var) \
	{                           \
		var = p[0] << 4 + p[1];   \
	}

void s16_to_u8s(u8 * p ,s16 src)
{
	p[0] = (u8)((s16)(src >> 8) & 0xff);
	p[1] = (u8)(src & 0xff);
}

void stdcn_var_init(void)
{
}

#define DOWN_CTL_INFO 0x0201
#define DOWN_TIME_INFO 0x0202
#define DOWN_SET_INFO 0x0203
#define DOWN_FOTA_INFO 0x0204

#define READ_STATUS_INFO 0x0301

#define AIR_TYPE 0x50
#define DEV_TYPE AIR_TYPE

#define UPDATA_MSG_CODE 0x99
#define UPDATA_ACK_CODE 0x09

#define DOWN_READ_MSG_CODE 0x01
#define DOWN_READ_OK_CODE 0x81

#define DOWN_WIRTE_OK_CODE 0x84
#define DOWN_WIRTE_FAIL_CODE 0xC4

NB_CONNECT g_nb_conn;




u8 sn = 0;
void stdcn_header_assemble(u8 *pBuff, u16 len, u8 ctrCode, u16 cmd)
{
	u8 *p;

	if (NULL == pBuff)
		return;

	p = &pBuff[0];

	*p++ = 0x68; //标识位

	*p++ = DEV_TYPE;

	stdcn_u16_to_u8s(p, len);
	p += 2;

	*p++ = ctrCode;

	*p++ = sn++;

	stdcn_u16_to_u8s(p, cmd);
	p += 2;

	stdcn_u16_to_u8s( p, g_nb_conn.rsrp);
	p += 2;

	stdcn_u16_to_u8s(p, g_nb_conn.snr);
	p += 2;

	stdcn_u32_to_u8s(p, g_nb_conn.cellid);
	p += 4;

	*p++ = g_nb_conn.sc_ecl;
}

//上报大数据
void Report_Big_Data(u16 up_type)
{
	u8 *p,*pDat;
	u16 len = 0, i;
	Ql_memset(SendMsgBuf, 0x0, sizeof(SendMsgBuf));

	u16 cnt = 0;
	//大数据
	pDat = &SendMsgBuf[17];
	*pDat++ = BIG_DATA_MSG;
	*pDat++ = 0;
	if (Main_Board_Ver == Ver_218)
	{
		Ql_memcpy(pDat, g_iAppMsg.BigDat, 42);
		len = 44;
	}else
	{
		len = 2;
		stdcn_u16_to_u8s(pDat, v100_message.TEMP);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.HHON);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.MMON);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.HHOFF);
		pDat += 2;
		len  += 2;		
		stdcn_u16_to_u8s(pDat, v100_message.MMOFF);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.MODE);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.WIND);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.SOLIDH);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.WORDA);
		pDat += 2;
		len  += 2;	
		stdcn_u16_to_u8s(pDat, v100_message.WORDB);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.HUMSD);
		pDat += 2;
		len  += 2;	
		stdcn_u16_to_u8s(pDat, v100_message.STEMP);
		pDat += 2;
		len  += 2;			

	}

	stdcn_header_assemble(SendMsgBuf, len, UPDATA_MSG_CODE, up_type);
	len += 17;

	for (i = 0; i < len; i++)
	{
		SendMsgBuf[len] += SendMsgBuf[i];
	}
	len += 1;
	SendMsgBuf[len] += 0x16;
	len += 1;

	Data_Buffer_Len = len;
}

//上报大数据
//void Report_Condition_Data(u16 up_type)
void Build_Big_Data(u8 *pDat, u16 *Len)
{
	u16 len, i, cnt = 0;

	*Len = 0;

	//大数据数据
	
	*pDat++ = BIG_DATA_MSG;
	*pDat++ = 0;
	if (Main_Board_Ver == Ver_218)
	{
		Ql_memcpy(pDat, g_iAppMsg.BigDat, 42);
		len = 44;
	}else
	{
		len = 2;
		stdcn_u16_to_u8s(pDat, v100_message.TEMP);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.HHON);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.MMON);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.HHOFF);
		pDat += 2;
		len  += 2;		
		stdcn_u16_to_u8s(pDat, v100_message.MMOFF);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.MODE);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.WIND);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.SOLIDH);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.WORDA);
		pDat += 2;
		len  += 2;	
		stdcn_u16_to_u8s(pDat, v100_message.WORDB);
		pDat += 2;
		len  += 2;
		stdcn_u16_to_u8s(pDat, v100_message.HUMSD);
		pDat += 2;
		len  += 2;	
		stdcn_u16_to_u8s(pDat, v100_message.STEMP);
		pDat += 2;
		len  += 2;			

	}

	*Len = len;	

}

BuildAppMsg(u8 *pDat, u16 *Len)
{
	u16 len, i, cnt = 0;

	*Len = 0;


	
	*pDat++ = BIG_DATA_MSG;
		*pDat++ = 0;
		if (Main_Board_Ver == Ver_218)
		{
			Ql_memcpy(pDat, g_iAppMsg.BigDat, 42);
			len = 44;
		}else
		{
			len = 2;
			stdcn_u16_to_u8s(pDat, v100_message.TEMP);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.HHON);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.MMON);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.HHOFF);
			pDat += 2;
			len  += 2;		
			stdcn_u16_to_u8s(pDat, v100_message.MMOFF);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.MODE);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.WIND);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.SOLIDH);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.WORDA);
			pDat += 2;
			len  += 2;	
			stdcn_u16_to_u8s(pDat, v100_message.WORDB);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.HUMSD);
			pDat += 2;
			len  += 2;	
			stdcn_u16_to_u8s(pDat, v100_message.STEMP);
			pDat += 2;
			len  += 2;			
	
		}

	//环境数据
	
	*pDat++ = ENV_DATA_MSG;

	for (i = 0; i < envBufListLen; i++)
	{
		if (g_iAppMsg.env_buf_pack[i].flg == 1)
		{
			cnt++;
		}
	}
	*pDat++ = cnt ; //2
	*pDat++ = g_iAppMsg.env_buf_pack[0].Hours;
	*pDat++ = g_iAppMsg.env_buf_pack[0].Mins;
	for (i = 0; i < cnt; i++)
	{
		*pDat++ = g_iAppMsg.env_buf_pack[i].temp;
		*pDat++ = g_iAppMsg.env_buf_pack[i].HR;
		stdcn_u16_to_u8s(pDat, g_iAppMsg.env_buf_pack[i].PM25);
		pDat += 2;
		stdcn_u16_to_u8s(pDat, g_iAppMsg.env_buf_pack[i].HCHO);
		pDat += 2;
		stdcn_u16_to_u8s(pDat, g_iAppMsg.env_buf_pack[i].AQI);
		pDat += 2;
		stdcn_u16_to_u8s(pDat, g_iAppMsg.env_buf_pack[i].CO2);
		pDat += 2;
	}
	*Len += cnt * 10 + 4;
	//电量数据
	//*pDat++ = POWER_DATA_MSG;
	//*pDat++ = 4;
	//stdcn_u32_to_u8s(pDat, g_iAppMsg.PowerDat);
	//len += 6;

	for( i = 0; i < cnt; i++)
	{
		g_iAppMsg.env_buf_pack[i].flg = 0;
	}	
	
}

BuildAppMsg2(u8 *pDat, u16 *Len)
{
	u16 len, i, cnt = 0;

	*Len = 0;
	
	*pDat++ = BIG_DATA_MSG;
		*pDat++ = 0;
		if (Main_Board_Ver == Ver_218)
		{
			Ql_memcpy(pDat, g_iAppMsg.BigDat, 42);
			len = 44;
		}else
		{
			len = 2;
			stdcn_u16_to_u8s(pDat, v100_message.TEMP);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.HHON);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.MMON);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.HHOFF);
			pDat += 2;
			len  += 2;		
			stdcn_u16_to_u8s(pDat, v100_message.MMOFF);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.MODE);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.WIND);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.SOLIDH);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.WORDA);
			pDat += 2;
			len  += 2;	
			stdcn_u16_to_u8s(pDat, v100_message.WORDB);
			pDat += 2;
			len  += 2;
			stdcn_u16_to_u8s(pDat, v100_message.HUMSD);
			pDat += 2;
			len  += 2;	
			stdcn_u16_to_u8s(pDat, v100_message.STEMP);
			pDat += 2;
			len  += 2;			
	
		}
	
	
	//环境数据
	
	*pDat++ = ENV_DATA_MSG;

	for (i = 0; i < envBufListLen; i++)
	{
		if (g_iAppMsg.env_buf_pack[i].flg == 1)
		{
			cnt++;
		}
	}
	*pDat++ = cnt * 10 + 2; //2
	*pDat++ = g_iAppMsg.env_buf_pack[0].Hours;
	*pDat++ = g_iAppMsg.env_buf_pack[0].Mins;
	for (i = 0; i < cnt; i++)
	{
		*pDat++ = g_iAppMsg.env_buf_pack[i].temp;
		*pDat++ = g_iAppMsg.env_buf_pack[i].HR;
		stdcn_u16_to_u8s(pDat, g_iAppMsg.env_buf_pack[i].PM25);
		pDat += 2;
		stdcn_u16_to_u8s(pDat, g_iAppMsg.env_buf_pack[i].HCHO);
		pDat += 2;
		stdcn_u16_to_u8s(pDat, g_iAppMsg.env_buf_pack[i].AQI);
		pDat += 2;
		stdcn_u16_to_u8s(pDat, g_iAppMsg.env_buf_pack[i].CO2);
		pDat += 2;
	}
	*Len += cnt * 10 + 4;
	//电量数据
	*pDat++ = POWER_DATA_MSG;
	*pDat++ = 4;
	stdcn_u32_to_u8s(pDat, (u32)(g_iAppMsg.PowerDat/1000));
	*Len += 6;

	for( i = 0; i < cnt; i++)
	{
		g_iAppMsg.env_buf_pack[i].flg = 0;
	}

	g_iAppMsg.PowerDat = 0.0;
	
}
void SendMsg(u8 *pDat, u16 len)
{
}

//上电上报数据
void PowerOnRegInfo(void)
{
	u8 *p;
	u16 len = 0, i;
	Ql_memset(SendMsgBuf, 0x0, sizeof(SendMsgBuf));
	p = &SendMsgBuf[17];
	memcpy(p, g_nb_conn.imei, 15);
	p += 15;
	len += 15;
	memcpy(p, g_nb_conn.imsi, 15);
	p += 15;
	len += 15;
	memcpy(p, g_nb_conn.ccid, 20);
	p += 20;
	len += 20;
	memcpy(p, PutDev, 32);
	p += 32;
	len += 32;

	stdcn_header_assemble(SendMsgBuf, len+4, UPDATA_MSG_CODE, UPDAT_POWER_ON_MSG);
	len += 17;

	for (i = 0; i < len; i++)
	{
		SendMsgBuf[len] += SendMsgBuf[i];
	}
	len += 1;
	SendMsgBuf[len] += 0x16;
	len += 1;

	Data_Buffer_Len = len;
	SendMsg(SendMsgBuf, len);
}

void UpdataCurveInfo(void) //上报曲线数据
{
	u8 *p;
	u16 len = 0, i;
	Ql_memset(SendMsgBuf, 0x0, sizeof(SendMsgBuf));
	BuildAppMsg(&SendMsgBuf[17], &len);
	stdcn_header_assemble(SendMsgBuf, len+4, UPDATA_MSG_CODE, UPDAT_CURVE_MSG);
	//stdcn_header_assemble(SendMsgBuf, 0, UPDATA_MSG_CODE, UPDAT_CURVE_MSG);
	len += 17;

	for (i = 0; i < len; i++)
	{
		SendMsgBuf[len] += SendMsgBuf[i];
	}
	len += 1;
	SendMsgBuf[len] += 0x16;
	len += 1;

	Data_Buffer_Len = len;
//S	SendMsg(SendMsgBuf, len);
}

void UpdataWarningInfo(void) //上报报警数据 大数据
{

	u8 *p;
	u16 len = 0, i;
	Ql_memset(SendMsgBuf, 0x0, sizeof(SendMsgBuf));
	Build_Big_Data(&SendMsgBuf[17], &len);
	stdcn_header_assemble(SendMsgBuf, len+4, UPDATA_MSG_CODE, UPDAT_WARNING_INFO);
	len += 17;

	for (i = 0; i < len; i++)
	{
		SendMsgBuf[len] += SendMsgBuf[i];
	}
	len += 1;
	SendMsgBuf[len] += 0x16;
	len += 1;

	Data_Buffer_Len = len;
	SendMsg(SendMsgBuf, len);
}

void UpdataPowerInfo(void) //上报电量数据
{

	u8 *p;
	u16 len = 0, i;

	BuildAppMsg2(&SendMsgBuf[17], &len);
	stdcn_header_assemble(SendMsgBuf, len+4, UPDATA_MSG_CODE, UPDAT_POWER_MSG);

	len += 17;

	for (i = 0; i < len; i++)
	{
		SendMsgBuf[len] += SendMsgBuf[i];
	}
	len += 1;
	SendMsgBuf[len] += 0x16;
	len += 1;

	Data_Buffer_Len = len;
	SendMsg(SendMsgBuf, len);
}

void UpdataAirStatusInfo(void) //上报空调状态数据
{

	u8 *p;
	u16 len = 0, i;

	Build_Big_Data(&SendMsgBuf[17], &len);
	stdcn_header_assemble(SendMsgBuf, len+4, UPDATA_MSG_CODE, UPDAT_STATUS_CHANTE);
	len += 17;

	for (i = 0; i < len; i++)
	{
		SendMsgBuf[len] += SendMsgBuf[i];
	}
	len += 1;
	SendMsgBuf[len] += 0x16;
	len += 1;

	Data_Buffer_Len = len;
	SendMsg(SendMsgBuf, len);
}

void UpdataLimitdInfo(void) //阈值超限上报
{

	u8 *p;
	u16 len = 0, i;

	Build_Big_Data(&SendMsgBuf[17], &len);
	stdcn_header_assemble(SendMsgBuf, len, UPDATA_MSG_CODE, UPDAT_LIMITED_MSG);
	len += 17;

	for (i = 0; i < len; i++)
	{
		SendMsgBuf[len] += SendMsgBuf[i];
	}
	len += 1;
	SendMsgBuf[len] += 0x16;
	len += 1;

	Data_Buffer_Len = len;
	SendMsg(SendMsgBuf, len);
}



iMax35_MSG giMax35Msg;

typedef struct _iNB_Set_MSG_
{

	u8 Tmep_LimMax;
	u8 Tmep_LimMin;
	u8 HR_LimMax;
	u8 HR_LimMin;
	u16 PM25_LimMax;
	u16 PM25_LimMin;
	u16 HCHO_LimMax;
	u16 HCHO_LimMin;
	u16 AQI_LimMax;
	u16 AQI_LimMin;
	u16 CO2_LimMax;
	u16 CO2_LimMin;
	u8 FotaVer[2];
	u8 time; //离散时间

} iSet_MSG;

iSet_MSG giSetMsg;

#ifdef __cplusplus
extern "C"
{
#endif

	void iMax35_Init(iMax35_MSG *pMsg);



	void iMax35_Init(iMax35_MSG *pMsg)
	{
		pMsg->RxState = iMax35_STATE_HEAD;
	}

	u8 iMax35_MainProc(iMax35_MSG *pMsg,u8 *pDat,u16 len)
	{
		u8 rx_data;
		u8 rxNaviMsgNew = 0;

		while (!rxNaviMsgNew)
		{
			//    	if (!ShellKbHit())
			//            break;
	        while(len != 0 )
	        {
	        	
			    rx_data = *pDat++;
				
				APP_DEBUG("dat:%02X,status: %d,len = %d\r\n,", rx_data,pMsg->RxState,len);
				switch (pMsg->RxState)
				{
				case iMax35_STATE_HEAD:
					if (rx_data == iMax35_PACK_HEADER)
					{
						pMsg->RxState = iMax35_STATE_TYPE;
						pMsg->Cmd = 0x00;
					}
					
					break;
				case iMax35_STATE_TYPE:
					if (rx_data == iMax35_PACK_TYPE)
					{
						pMsg->RxState = iMax35_STATE_LEN1;
						pMsg->Cmd = 0x00;
					}
					else
					{
						pMsg->RxState = iMax35_STATE_HEAD;
					}
					break;
				case iMax35_STATE_LEN1:
					pMsg->DataLen = rx_data << 8;
					
					pMsg->RxState = iMax35_STATE_LEN2;
					
					break;

				case iMax35_STATE_LEN2:

					pMsg->DataLen += rx_data;
					
					pMsg->RxLen = 0;
					//            pMsg->Check   = rx_data;
					pMsg->RxState = iMax35_STATE_CODE;
					APP_DEBUG("datlen :%02X,status: %d\r\n",pMsg->DataLen,pMsg->RxState);
					break;
				case iMax35_STATE_CODE:
					pMsg->DataLen --;
					pMsg->ctrCode = rx_data;
					pMsg->RxState = iMax35_STATE_SN;
					break;
				case iMax35_STATE_SN:
					pMsg->sn = rx_data;
					pMsg->DataLen --;
					pMsg->RxState = iMax35_STATE_CMD1;
					break;
				case iMax35_STATE_CMD1:
					pMsg->DataLen --;
					pMsg->Cmd = rx_data << 8;
					pMsg->RxState = iMax35_STATE_CMD2;
					break;
				case iMax35_STATE_CMD2:
					pMsg->DataLen --;
					pMsg->Cmd += rx_data;
					//            pMsg->Check += rx_data;
					//            pMsg->DataLen--;
					
					pMsg->RxState = iMax35_STATE_DATA;
					if (pMsg->DataLen == 0)
					{
						pMsg->RxState = iMax35_STATE_CHECK;
					}
					break;

				case iMax35_STATE_DATA:
					pMsg->Data[pMsg->RxLen] = rx_data;
					pMsg->RxLen++;
					pMsg->Check += rx_data;
					if (pMsg->RxLen >= pMsg->DataLen)
					{
						pMsg->RxState = iMax35_STATE_CHECK;
					}
					break;
				case iMax35_STATE_CHECK:
					//if (rx_data == pMsg->Check)
					{
						/* 处理 NaviMsg*/
						//rxNaviMsgNew = 0xAA;
						pMsg->RxState = iMax35_STATE_END;
					}
					//pMsg->RxState = iMax35_STATE_HEAD;
					break;
				case iMax35_STATE_END:
					if (rx_data == iMax35_PACK_END)
					{
						/* 处理 NaviMsg*/
						rxNaviMsgNew = 0xAA;
					}
					pMsg->RxState = iMax35_STATE_HEAD;
					break;
				}
				len--;
	        }
		}

		return rxNaviMsgNew;
	}

	void SetFotaPra(u8 *pdat, u16 *len)
	{

		pdat[0] = 1;//g_nb_conn.ver[0];
		pdat[1] = 2;g_nb_conn.ver[1];

		*len = 2;
	}
	void stdcn_header_assemble_Ex(u8 *pBuff, u16 len, u8 ctrCode, u16 cmd)
	{
		u8 *p;
	
		if (NULL == pBuff)
			return;
	
		p = &pBuff[0];
	
		*p++ = 0x68; //标识位
	
		*p++ = DEV_TYPE;
	
		stdcn_u16_to_u8s(p, len);
		p += 2;
	
		*p++ = ctrCode;
	
		*p++ = giMax35Msg.sn;
	
		stdcn_u16_to_u8s(p, cmd);
		p += 2;
	
		stdcn_u16_to_u8s( p, g_nb_conn.rsrp);
		p += 2;
	
		stdcn_u16_to_u8s(p, g_nb_conn.snr);
		p += 2;
	
		stdcn_u32_to_u8s(p, g_nb_conn.cellid);
		p += 4;
	
		*p++ = g_nb_conn.sc_ecl;
	}

	void AnswerSatusMsg(void)
	{
		u8 *p;
		u16 len = 0, i;

		BuildAppMsg(&SendMsgBuf[17], &len);
		stdcn_header_assemble_Ex(SendMsgBuf, len+4, DOWN_READ_OK_CODE, READ_STATUS_INFO);
		len += 17;

		for (i = 0; i < len; i++)
		{
			SendMsgBuf[len] += SendMsgBuf[i];
		}
		len += 1;
		SendMsgBuf[len] += 0x16;
		len += 1;

		Data_Buffer_Len = len;
		Send_Msg_to_Server();
	}


	BuildCtrMsg(u8 *pDat, u16 *len)
	{
		Ql_memcpy(pDat, g_ctr_up_data, g_ctr_up_data_len);
		*len = g_ctr_up_data_len;


		APP_DEBUG("\r\g_ctr_up_data:   Sendlen: %d\r\n ",g_ctr_up_data_len);
			for (int i = 0; i <g_ctr_up_data_len; i++)
			{
				APP_DEBUG("%02X ", g_ctr_up_data[i]);
			}
			APP_DEBUG("\r\n");	
	}

	

	void AnswerCtrMsg(u8 rst)
	{

		u16 len = 0, i;
		if (rst)
		{
			BuildCtrMsg(&SendMsgBuf[17], &len);

			
			stdcn_header_assemble_Ex(SendMsgBuf, len+4, DOWN_WIRTE_OK_CODE, DOWN_CTL_INFO);

			

		}
		else
		{
			stdcn_header_assemble_Ex(SendMsgBuf, len+4, DOWN_WIRTE_FAIL_CODE, DOWN_CTL_INFO);
		}
		len += 17;

		for (i = 0; i < len; i++)
		{
			SendMsgBuf[len] += SendMsgBuf[i];
		}
		len += 1;
		SendMsgBuf[len] = 0x16;
		len += 1;

		Data_Buffer_Len = len;
		 
		SendMsg(SendMsgBuf, len);
	}

	void AnswerFOTAMsg(void)
	{

		u16 len = 0, i;

		SetFotaPra(&SendMsgBuf[17], &len);
		stdcn_header_assemble_Ex(SendMsgBuf, len+4, UPDATA_ACK_CODE, DOWN_FOTA_INFO);
		len += 17;

		for (i = 0; i < len; i++)
		{
			SendMsgBuf[len] += SendMsgBuf[i];
		}
		len += 1;
		SendMsgBuf[len] = 0x16;
		len += 1;

		Data_Buffer_Len = len;
		Send_Msg_to_Server();
		SendMsg(SendMsgBuf, len);
	}

	void AnswerSetMsg(void)
	{

		u16 len = 0, i;
		stdcn_header_assemble_Ex(SendMsgBuf, 0+4, DOWN_WIRTE_OK_CODE, DOWN_FOTA_INFO);
		len += 17;

		for (i = 0; i < len; i++)
		{
			SendMsgBuf[len] += SendMsgBuf[i];
		}
		len += 1;
		SendMsgBuf[len] = 0x16;
		len += 1;

		Data_Buffer_Len = len;
		Send_Msg_to_Server();
		SendMsg(SendMsgBuf, len);
	}

		void AnswerTIMEMsg(void)
	{

		u16 len = 0, i;
		stdcn_header_assemble_Ex(SendMsgBuf, 0+4, DOWN_WIRTE_OK_CODE, DOWN_TIME_INFO);
		len += 17;

		for (i = 0; i < len; i++)
		{
			SendMsgBuf[len] += SendMsgBuf[i];
		}
		len += 1;
		SendMsgBuf[len] = 0x16;
		len += 1;

		Data_Buffer_Len = len;
		Send_Msg_to_Server();
		SendMsg(SendMsgBuf, len);
	}

	void SetParaInit(void)
	{
		giSetMsg.Tmep_LimMax = 255;
		giSetMsg.Tmep_LimMin = 0;
		giSetMsg.HR_LimMax = 100;
		giSetMsg.HR_LimMin = 0;
		giSetMsg.PM25_LimMax = 4095;
		giSetMsg.PM25_LimMin = 0;
		giSetMsg.HCHO_LimMax = 65535;
		giSetMsg.HCHO_LimMin = 0;
		giSetMsg.AQI_LimMax = 1023;
		giSetMsg.AQI_LimMin = 0;
		giSetMsg.CO2_LimMax = 65535;
		giSetMsg.CO2_LimMin = 0;
	}

	void GetFotaPara(u8 *pdata, u16 rxLen)
	{
		giSetMsg.FotaVer[0] = pdata[0];
		giSetMsg.FotaVer[1] = pdata[1];

		giSetMsg.FotaVer[0] = 1;
		giSetMsg.FotaVer[1] = 2;

	}

	void GetTimePara(u8 *pdata, u16 rxLen)
	{
		giSetMsg.time = pdata[0];
	}

	void GetSetPara(u8 *pdata, u16 rxLen)
	{
		giSetMsg.Tmep_LimMax = *pdata++;
		giSetMsg.Tmep_LimMin = *pdata++;
		giSetMsg.HR_LimMax = *pdata++;
		giSetMsg.HR_LimMin = *pdata++;
		stdcn_u8s_to_u16(pdata, giSetMsg.PM25_LimMax);
		pdata += 2;
		stdcn_u8s_to_u16(pdata, giSetMsg.PM25_LimMin);
		pdata += 2;
		stdcn_u8s_to_u16(pdata, giSetMsg.HCHO_LimMax);
		pdata += 2;
		stdcn_u8s_to_u16(pdata, giSetMsg.HCHO_LimMin);
		pdata += 2;
		stdcn_u8s_to_u16(pdata, giSetMsg.AQI_LimMax);
		pdata += 2;
		stdcn_u8s_to_u16(pdata, giSetMsg.AQI_LimMin);
		pdata += 2;
		stdcn_u8s_to_u16(pdata, giSetMsg.CO2_LimMax);
		pdata += 2;
		stdcn_u8s_to_u16(pdata, giSetMsg.CO2_LimMin);
		pdata += 2;
	}

	void DownCtrinfo(u8 *pdata, u16 rxLen)
	{

		Ql_UART_Write(m_myUartPort, (u8 *)pdata, rxLen); //从串口透传
		
	}

#if 1	
//下行数据解析
void NB_DataPro(void)
{
  
		if(giMax35Msg.ctrCode == 0x01)    //数据读取
		{
			if(giMax35Msg.Cmd == READ_STATUS_INFO)  //
			{
			APP_DEBUG("read status msg\r\n");
			    
				AnswerSatusMsg();
			}	
				
				
		}else
    if(giMax35Msg.ctrCode == 0x04)	//下行写入
    {
			if(giMax35Msg.Cmd == DOWN_CTL_INFO)  //控制
			{
				APP_DEBUG("down ctr msg\r\n");
				DownCtrinfo(giMax35Msg.Data,giMax35Msg.DataLen);

				g_ctr_down_flg = 1;
				if(0)
				{
					//AnswerCtrMsg(1);
				}else
        		{
					//AnswerCtrMsg(0);
				}				
			}else
      if(giMax35Msg.Cmd == DOWN_FOTA_INFO)	
	 {
	 			APP_DEBUG("down FOTA msg\r\n");
				GetFotaPara(giMax35Msg.Data,giMax35Msg.DataLen);
				AnswerFOTAMsg();
	  }else
      if(giMax35Msg.Cmd == DOWN_TIME_INFO)	
	{
				APP_DEBUG("down TIME msg\r\n");
				GetTimePara(giMax35Msg.Data,giMax35Msg.DataLen);
				AnswerTIMEMsg();
	}else
      if(giMax35Msg.Cmd == DOWN_SET_INFO)	
			{
				APP_DEBUG("down SET msg\r\n");
				GetSetPara(giMax35Msg.Data,giMax35Msg.DataLen);
				AnswerSetMsg();
			}
		
		}			
		
	

}	
#endif

