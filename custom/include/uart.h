//包含头文件
#include "string.h"
#include "ql_stdlib.h"
#include "ql_uart.h"
#include "ql_error.h"

//串口调试相关宏定义
#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT UART_PORT2
#define DBG_BUF_LEN 512
static char DBG_BUFFER[DBG_BUF_LEN];
#define APP_DEBUG(FORMAT, ...)                                                                                       \
    {                                                                                                                \
        Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN);                                                                       \
        Ql_sprintf(DBG_BUFFER, FORMAT, ##__VA_ARGS__);                                                                                                                                                                        \
        Ql_UART_Write((Enum_SerialPort)(DEBUG_PORT), (u8 *)(DBG_BUFFER), Ql_strlen((const char *)(DBG_BUFFER)));     \
    }
#else
#define APP_DEBUG(FORMAT, ...)
#endif

//串口相关Buffer
// Define the UART port and the receive data buffer
static Enum_SerialPort m_myUartPort = UART_PORT0;
#define SERIAL_RX_BUFFER_LEN 512
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];
static u8 m_buffer[100];

//内部函数定义
void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void *customizedPara);
//static s32 ATResponse_Handler(char* line, u32 len, void* userData);
s32 Init_UART(void);

// ---数据解析相关定义---

//定义帧头内容
#define START_FRAME 0xff
//定义帧头长度
#define START_FRAME_LEN 2

//最大帧长
#define FRAME_MAX_LEN 111

//地址标识长度
#define ADDR_SIGN_LEN 6

//帧屏蔽位
#define FRAME_MASK_BIT 0x55

#define CHECK_CMD_LEN   2

//消息類型
#define MESSAGE_TYPE_CTRL 0x01 //控制帧
#define MESSAGE_TYPE_STATE 0x02 //状态帧
#define MESSAGE_TYPE_NOP 0x03 //无效帧
#define MESSAGE_TYPE_ALARM 0x04 //报警帧

#define MESSAGE_TYPE_CON_S1 0x62 //握手帧1
#define MESSAGE_TYPE_CON_S2 0x71 //握手帧2

//返回結果類型
#define MESSAGE_RET_TYPE 0x6d     //返回值
#define MESSAGE_BIG_RET_TYPE 0x7d //返回值，大數據查詢

#define MESSAGE_BIG_RET_LEN 14 //大數據長度

#define MESSAGE_SET_LEN 10 //设置段数据长度

typedef struct
{
    u8 MESSAGE_TYPE; //消息類型

    u8 SETTMP;      //温度
                    /*
                        16℃～30℃
                    */
    u8 SETUPDN;     //上下摆风
                    /*
                        即 SETUPDOWN = 00H 对应着“上下摆位置固定”
                        SETUPDOWN = 01H 对应着“健康气流（上吹）”
                        SETUPDOWN = 02H 对应着“上下摆位置一”
                        设定上下摆风 SETUPDOWN = 03H 对应着“健康气流（下吹）”
                        2 SETUPDOWN = 04H 对应着“上下摆位置二” SETUPDN(1byte)
                        ●
                        SETUPDOWN = 06H 对应着“上下摆位置三”
                        SETUPDOWN = 08H 对应着“上下摆位置四”
                        SETUPDOWN = 0AH 对应着“上下摆位置五”
                        SETUPDOWN = 0Ch 对应着“上下摆自动”（默认） ●
                        SETUPDOWN = 0Eh 对应着“上下摆自动 2”（特
                        殊机型专用） @1
                    */
    u8 SETMODE;     //设定模式，共 7 档
                    /*  即 SETMODE = 00H 对应着“智能/自动/舒适”
                        SETMODE = 01H 对应着“制冷”
                        SETMDWD(1byte) SETMODE = 02H 对应着“除湿”
                        SETMODE = 03H 对应着“健康除湿”
                        SETMODE = 04H 对应着“制热”
                        SETMODE = 05H 对应着“节能模式”（窗机专用）
                        SETMODE = 06H 对应着“送风
                        */
    u8 SETSPMODE;   //特殊模式设置
                    /*
                        SETSMODE = 00h 对应着“无特殊模式”
                        SETSMODE = 01h 对应着“老人特殊模式”
                        SETSMODE = 02h 对应着“儿童特殊模式”
                        SETSMODE = 03h 对应着“孕妇特殊模式”
                    */
    u8 SETSPEED;    //设定风速
                    /*
                        设定风速，共 4 档。
                        即 SETWIND = 01H 对应着“高风”
                        SETWIND = 02H 对应着“中风”
                        SETWIND = 03H 对应着“低风”
                        SETWIND = 04H 空不用
                        SETWIND = 05H 对应着“自动”
                        */
    u8 POWER_STATE; /*
                        A0：表示字 A 的第 0 位，（默认关机）
                        为“0”时，表示“关机”
                        为“1”时，表示“开机”
                    */
    u8 HEALTH_MODE; /*
                        A1：表示字 A 的第 1 位，（默认关闭）
                        为“0”时，表示“关闭健康（负离子）”
                        为“1”时，表示“开启健康（负离子）”
                    */
    u8 ELE_HEATING; /*  A2：表示字 A 的第 2 位，（默认关闭）
                        为“0”时，表示“关闭电加热”
                        为“1”时，表示“开启电加热” 
                    */
    u8 STRONG_MODE; /*
                        A3：表示字 A 的第 3 位，（默认关闭）
                        为“0”时，表示“关闭强力”
                        为“1”时，表示“开启强力”  
                    */
    u8 SET_MUTE;    /*
                        A4：表示字 A 的第 4 位，（默认关闭）
                        为“0”时，表示“关闭静音”
                        为“1”时，表示“开启静音”
                    */

    u8 SLEEP_MUTE; //靜眠

    u8 ELE_LOCK;         /*
                        A6：表示字 A 的第 6 位，
                        为“0”时，表示“关闭电子锁”
                        为“1”时，表示“开启电子锁”
                    */
    u8 AUTO_SLEEP_CURVE; /*
                        A7：表示字 A 的第 7 位，（云适应和睡眠曲线用）
                        本位默认回“0”
                    */
    u8 TEN_HEATING;      /*
                        A8：表示字 A 的第 8 位，
                        为“0”时，表示“关闭 10℃制热”
                        为“1”时，表示“开启 10℃制热”
                    */
    u8 SEN_DISPLAY;      /*
                        A9：表示字 A 的第 9 位，
                        为“0”时，表示“关闭屏显”
                        为“1”时，表示“开启屏显”     
                    */
    u8 ZERO_POT_FIVE;    /*
                        A10：表示字 A 的第 10 位，
                        为“0”时，表示“设定 0.5℃无”为“1”时，表示“设定 0.5℃有”    
                     */
    u8 SMART_MODE;       /*
                        A11：表示字 A 的第 11 位，
                        为“0”时，表示“设定智能无”
                        为“1”时，表示“设定智能有”
                    */
    u8 COMFORTABLE;      /*
                        A12：表示字 A 的第 12 位，
                        为“0”时，表示“设定舒适（PMV）无”
                        为“1”时，表示“设定舒适（PMV）有”    
                    */
    u8 CTMP_FTMP;        /*
                        A13：表示字 A 的第 13 位，
                        为“0”时，表示“摄氏显示” ●
                        为“1”时，表示“华氏显示”
                    */
    u8 SETHUMIDITY;      /*
                        设定湿度从 30%～90%，线性 61 档，（无湿度默认为
                        50%）
                        即 SETHUMIDITY = 00h，对应着 30%，
                        SETHUMIDITY = 0Dh，对应着 43%，
                        SETHUMIDITY = 3Ch，对应着 90%，
                    */

    u8 SETPERSON;
    /*
                        设定感人模式共 4 档。
                        即 SETPERSON = 00H 对应着“关闭感人功能”
                        SETPERSON = 01H 对应着“感人避让”
                        SETPERSON = 02H 对应着“感人跟随”
                        SETPERSON = 03H 对应着“感人”
                    */
    u8 SETLEFTRIGHT;
    /*
                        设定左右摆风共 8 档。
                        即 SETLEFTRIGHT = 00H 对应着“左右摆位置一 (固
                        定)”
                        SETLEFTRIGHT = 01H 对应着“左右摆位置二”
                        SETLEFTRIGHT = 02H 对应着“左右摆位置三”
                        SETLEFTRIGHT = 03H 对应着“左右摆位置四”
                        SETLEFTRIGHT = 04H 对应着“左右摆位置五”
                        SETLEFTRIGHT = 05H 对应着“左右摆位置六”
                        SETLEFTRIGHT = 06H 对应着“左右摆位置七”
                        SETLEFTRIGHT = 07H 对应着“左右摆位置八（自
                        ●
                        动）”

                    */
    u8 SETNEWWIND;
    /*
                        表示字 B 的第 0 位，
                        为“0”时，表示“关闭新风”
                        为“1”时，表示“开启新风”
                    */
    u8 SETADDHUM; /*
                        B1：表示字 B 的第 1 位，
                        为“0”时，表示“关闭加湿”
                        为“1”时，表示“开启加湿”
                    */
    u8 SETCLEANDUST;
    /*
                        B2：表示字 B 的第 2 位，
                        为“0”时，表示“关闭除 PM2.5”
                        为“1”时，表示“开启除 PM2.5”
                    */
    u8 SETCLEANGAS; /*
                        B3：表示字 B 的第 3 位，
                        为“0”时，表示“关闭除甲醛”
                        为“1”时，表示“开启除甲醛”
                    */
    u8 SETAUTOCLEAN;
    /*
                        B4：表示字 B 的第 4 位，
                        为“0”时，表示“关闭自清扫/自清洁”
                        为
                        “1”时，表示“开启自清扫/自清洁”
                        */
    u8 SETBACKLIGHT;
    /*
                    B5：表示字 B 的第 5 位，
                    为“0”时，表示“关闭情景灯光”
                    ● 为
                    “1”时，表示“开启情景灯光”
                    */
    u8 SETPOWERSAVE; /*
                    B6：表示字 B 的第 6 位，
                    为“0”时，表示“关闭省电/ECO”   
                    为“1”时，表示“开启省电/ECO”
                    */
    u8 SETCLEANTIME; /*
                    B8：表示字 B 的第 8 位 @8
                    为“0”时，表示“本机端净化累计时间未到，不
                    ●
                    需提醒用户更换滤网” 
                    */
    u8 ROOMTMP;
    /*
                    室内环境温度 = 返回值，范围：0～110，单位：
                    .5℃
                    即 TEMP = 10H，对应着 8℃，
                    TEMP = 1eH，对应着 15℃
                    */
    u8 ROOMHUM;
    /*
                    室内环境湿度 = 返回值，范围：0～100，单位：
                    1%，（无传感器默认 00h）
                    即 SETHUMIDITY = 10H，对应着 16%，
                    SETHUMIDITY = 4eH，对应着 78%，
                    */
    s8 OUTTEMP;
    /*
                    返回值，范围 0-255 单位：℃（无传感器默认 00h）
                    3FH 对应着-1℃
                    40H 对应着 0℃
                    41H 对应着 1℃c
                    */
    u8 AIRPM_TYPE;
    /*
                    Bit7：
                    为“0”时，表示“冷暖机型”
                    为“1”时，表示“单冷机型”
                    Bit6：预留，默认“0”
                    Bit5-Bit4：人感状态
                    00H 对应着“无此功能”
                    01H 对应着“无人”
                    02H 对应着“单人”
                    03H 对应着“多人”
                    Bit3-Bit2 空气质量（VOC 传感器）共 4 档。（无传
                    感器默认 00H）
                    即 AIRQUALITY = 00H 对应着“优”
                    AIRQUALITY = 01H 对应着“良”
                    AIRQUALITY = 02H 对应着“中”
                    AIRQUALITY = 03H 对应着“差”
                    Bit1-Bit0：室内 PM2.5 等级共 4 档。（无传感器默
                    认 00H）
                    即 PM2.5 = 00H 对应着“优”
                    PM2.5 = 01H 对应着“良”
                    PM2.5 = 02H 对应着“中”
                    PM2.5 = 03H 对应着“差”
                    备注：有数值的直接发送数值，不发送等级，等级
                    默认发“00”。
                    */
    u8 AIRPM_MAN; //人感

    u8 AIRPM_GAS; //空氣質量

    u8 AIRPM_ROOMPM; //室內PM2.5

    u8 ERROR_CODE;

    u8 CRT_CMD;
    /*
                    B7：（与“故障代码”配合使用）@2
                    为“0”时，表示“无故障确认”
                    为“1”时，表示“故障确认”
                    Bit6-Bit4：预留，默认发“00”
                    Bit3-Bit2：空调实际的运行模式，共 4 档。（苹果
                    Homekit 专用） @9
                    源 即 MODE = 00H 对应着“制冷”
                    MODE = 01H 对应着“除湿”
                    MODE = 02H 对应着“制热”
                    MODE = 03H 对应着“送风”
                    Bit1-Bit0：控制命令来源
                    00H 对应着“其他”
                    01H 对应着“遥控器”
                    02H 对应着“按键”
                    03H 对应着“网络”
                */
    u8 ERROR_CODE_ACK;
    /*
                    B7：（与“故障代码”配合使用）@2
                    为“0”时，表示“无故障确认”
                    为“1”时，表示“故障确认”

                */
    u8 AIR_RUN_MODE;
    /*
                    Bit3-Bit2：空调实际的运行模式，共 4 档。（苹果
                    Homekit 专用） @9
                     即 MODE = 00H 对应着“制冷”
                    MODE = 01H 对应着“除湿”
                    e）
                    MODE = 02H 对应着“制热”
                    MODE = 03H 对应着“送风”
                    */
    u16 BROAD_CLEANTIME;
    /*
                    累计时间，范围：0～65535，单位：小时
                    即 CLEAN = 0000H，对应着 0 小时，
                    CLEAN= 0001H，对应着 1 小时，
                    CLEAN= 0010H，对应着 16 小时
                    */
    u16 ROOM_PM_VALUE;
    /*室內PM
                       PM2.5 = 返回值，范围：0～4095，单位：1ug/m
                        0x0000 -----无 PM2.5 传感器
                        0x0001 -----1ug/m3
                        0x0002 -----2ug/m3
                        0x0FFF -----4095ug/m3   
                        */
    u16 OUT_PM_VALUE;
    /*室外PM
                       PM2.5 = 返回值，范围：0～4095，单位：1ug/m
                        0x0000 -----无 PM2.5 传感器
                        0x0001 -----1ug/m3
                        0x0002 -----2ug/m3
                        0x0FFF -----4095ug/m3      
                    */
    u16 GAS_VALUE;
    /*
                    甲醛含量 = 返回值，范围：0～65535，甲醛范围
                    为 1ug/m3～10000ug/m3
                    word) HCHO = 0000h，对应着无甲醛传感器
                    HCHO = 0001h，对应着 1ug/m3
                    HCHO = 000Eh，对应着 14ug/m3
                    */
    u16 VOC_VALUE;
    /*空氣
                    VOC= 返回值，范围：0～1023（10bit A/D）
                    VOC = 00 00H，对应着无 VOC 传感器
                    VOC = 00 01H，对应着 1
                    VOC= 00 02H，对应着 2
                    VOC= 03 FFH，对应着 1023
                    */
    u16 CO2_VALUE;
    /*CO2
                        CO2= 返回值，范围：0～10000PPM
                        CO2 = 0000h，对应着无 CO2 传感器
                        CO2 = 0001h，对应着 1PPM
                        CO2 = 000Eh●，对应着 14PPM
                    */

    u8 NOP; //预留位
    
    u8 Set_Message[MESSAGE_SET_LEN];

    //u8 Message_Puls[]; //附加信息，如果為大數據查詢;

} Message_Data;

//环境数据规则
typedef struct
{
    u8 ROOMTMP_VALUE_MAX;
    u8 ROOMTMP_VALUE_MIN;

    u8 ROOMHUM_VALUE_MAX;
    u8 ROOMHUM_VALUE_MIN;

    u16 ROOM_PM_VALUE_MAX;
    u16 ROOM_PM_VALUE_MIN;

    u16 GAS_VALUE_MAX;
    u16 GAS_VALUE_MIN;

    u16 VOC_VALUE_MAX;
    u16 VOC_VALUE_MIN;

    u16 CO2_VALUE_MAX;
    u16 CO2_VALUE_MIN;

} Condition_Data;

typedef struct
{

    u16 POWER;    /*
                    功率 = 返回值，范围：0～65535，单位：1W
                    即 POWER = 0047H，对应着 71W，
                    POWER = 004BH，对应着 75W，
                */
    u8 TUBE_TMP; /* 管盤內溫度
                    返回值，范围：0～255，单位：0.5℃
                    00H 对应着 -20℃
                    28H 对应着 0℃
                */
    u8 OUTROOM_PUT_TMP;
    /* 室外吐氣溫度
                    返回值，范围：0～255，单位：℃（无传感器默认
                    00h）
                    3FH 对应着-1℃
                    40H 对应着 0℃
                    41H 对应着 1℃
                    */
    u8 OUTROOM_TUBE_TMP;
    /* 室外管盤溫度
                    返回值，范围：0～255，单位：℃（无传感器默认
                    00h）
                    3FH 对应着-1℃
                    40H 对应着 0℃
                    41H 对应着 1℃
                    */
    u8 OUTROOM_IN_TMP;
    /*
                    室外吸氣溫度
                    返回值，范围：0～255，单位：℃（无传感器默认
                    00h）
                    3FH 对应着-1℃
                    40H 对应着 0℃
                    ●
                    41H 对应着 1℃
                    */
    u8 OUTROOM_CLEAN_ICE;
    /* 室外除霜溫度
                    返回值，范围：0～255，单位：℃（无传感器默认
                    00h）
                    3FH 对应着-1℃
                    40H 对应着 0℃
                    41H 对应着 1℃
                    */
    u8 MACHINE_FREQUENCY;
    /*
                    Bit7-Bit0：运行频率 范围 0-127HZ， 单位 1 HZ
                    （无压机频率默认 7FH）
                    00H =关机
                    20H =32HZ
                    40H =64HZ
                    ●
                    */
    u8 MACHINE_ELE;
    /*
                        Bit15-Bit9：预留
                        Bit8-Bit0：范围：0～511，压机电流，范围 0-50A
                        单位：0.1A（无压机电流默认 1FFH）
                        00—0A
                        01—0.1A
                        02—0.2A
                    */
    u8 OUTROOM_STATE;
    /*
                        bit1：bit0
                        00：压机关闭
                        01：压机开启
                        10：信息无法获得
                        Bit3：bit2
                        00：室内风机关闭
                        01：室内风机开启
                        10：信息无法获得
                        Bit5：bit4
                        ●
                        00：四通阀关闭
                        01：四通阀开启
                        10：信息无法获得
                        Bit7：bit6
                        00：室内电加热关闭
                        01：室内电加热开启
                        10：信息无法获得
                        Bit9：bit8
                        00：室外风机关闭
                        01：室外风机开启
                        10：信息无法获得
                        Bit11：bit10
                        00：空调在非除霜状态
                        01：空调正在除霜
                        10：信息无法获得
                        ●
                        Bit12-bit15：预留
                        */
    u8 ELE_VALVE;
    /*
                    电子膨胀阀开度， 实际开度，范围：0～4095（无
                    电子膨胀阀默认 0000h）
                    80H—128 步
                    */

} Message_Data_Puls;

typedef struct
{
	u8 MESSAGE_TYPE; //消息類型
	u16 TEMP;
	u16 HHON;
	u16 MMON;
	u16 HHOFF;
	u16 MMOFF;
	u16 MODE;
	u16 WIND;
	u16 SOLIDH;
	u16 WORDA;
	u16 WORDB;
	u16 HUMSD;
	u16 STEMP;

	u8 Set_Message[4];

}Message_Data_Ex;



typedef struct
{
    u8 message_length;            //消息长度
    u8 length;                    //帧长
    u8 buffer[FRAME_MAX_LEN - 1]; //有效负荷
    u8 addr_sign[ADDR_SIGN_LEN];  //地址标识
    u8 type;                      //帧类型
    Message_Data message;         //消息
    u8 check_byte;
} Frame_Data;

#define BIG_DATA_ALL_LEN    42

//规则检查
#define Check_Condition_Rules()     message.ROOMTMP > condition_rule.ROOMTMP_VALUE_MAX ||  message.ROOMTMP < condition_rule.ROOMTMP_VALUE_MIN || \
                                    message.ROOMHUM > condition_rule.ROOMHUM_VALUE_MAX ||  message.ROOMHUM < condition_rule.ROOMHUM_VALUE_MIN || \
                                    message.ROOM_PM_VALUE > condition_rule.ROOM_PM_VALUE_MAX ||  message.ROOM_PM_VALUE < condition_rule.ROOM_PM_VALUE_MIN || \
                                    message.GAS_VALUE > condition_rule.GAS_VALUE_MAX ||  message.GAS_VALUE < condition_rule.GAS_VALUE_MIN || \
                                    message.VOC_VALUE > condition_rule.VOC_VALUE_MAX ||  message.VOC_VALUE < condition_rule.VOC_VALUE_MIN || \
                                    message.CO2_VALUE > condition_rule.CO2_VALUE_MAX ||  message.CO2_VALUE < condition_rule.CO2_VALUE_MIN \




//发送指令
void Send_Check_Cmd(void);


extern Message_Data message;

extern Message_Data_Ex v100_message;

