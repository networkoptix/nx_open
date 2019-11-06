
#ifndef __PSE_BUDGET_H__
#define __PSE_BUDGET_H__

#define SRP400S_CHIP		1
#define SRP800S_CHIP		2
#define SRP1600S_CHIP		4

#define SRP400S_PORT		4
#define SRP800S_PORT		8
#define SRP1600S_PORT		16


#define PORT_MAX_PER_CHIP	4
#define PSE_OFF		0
#define PSE_ON		1

#define PSE_OK		0
#define PSE_FAIL	-1

#define PSE_CHIP_POWER_BUDGET	144 	
#define PSE_POWER_BUDGET		50.0f 	
#define PSE_POWER_BUDGET_MARGIN	8.0f 	
#define PSE_MAX_PORT_POWER		30.0f
#define PSE_TURN_ON_PERIOD		3		
#define PSE_OVER_BUDGET_TIMEOUT	5		
#define POWER_GOOD_TIME_CONFIRM	10		

#define MAX_COUNT	10

typedef enum
{
	PORT_PRI_HIGH = 0,
	PORT_PRI_MID  = 1,
	PORT_PRI_LOW  = 2
}PSE_Port_Priority;

enum Command_t
{
	CMD_NONE,
	CMD_PORT_ON,
	CMD_PORT_OFF
};

typedef struct _portCmdValue_t
{
	unsigned int port;
	unsigned int enable;
}portCmdValue_t;

typedef struct _portValue_t
{
	unsigned int m_port;
	unsigned int m_value;
}portValut_t;


typedef struct _portWattValue_t
{
	unsigned int m_port;
	unsigned int m_volt_lsb;
	unsigned int m_volt_msb;
	unsigned int m_curr_lsb;
	unsigned int m_curr_msb;
}portWattValue_t;

typedef struct _portEnableStatus_t
{
	unsigned int m_port;
	unsigned int m_enable;
}portEnableStatus_t;

typedef struct _portVoltageValue_t
{
	unsigned int m_port;
	unsigned int m_lsb;
	unsigned int m_msb;
}portVoltageValue_t;

typedef struct _portCurrentValue_t
{
	unsigned int m_port;
	unsigned int m_lsb;
	unsigned int m_msb;
}portCurrentValue_t;

typedef struct _chipBudgetValue_t
{
	unsigned int m_chip;
	unsigned int m_value;
}chipBudgetValue_t;

typedef struct _portLimitValue_t
{
	unsigned int m_port;
	unsigned int m_value;
}portLimitValue_t;

typedef struct _portGoodStatusValue_t
{
	unsigned int m_port;
	unsigned int m_value;
}portGoodStatusValue_t;

typedef struct _portOverCurrentTimeOutValue_t
{
	unsigned int m_port;
	unsigned int m_value;
}portOverCurrentTimeOutValue_t;

typedef struct _portStatusValue_t
{
	unsigned int m_port;
	unsigned int m_value;
}portStatusValue_t;

typedef struct _portClassValue_t
{
	unsigned int m_port;
	unsigned int m_value;
}portClassValue_t;

typedef struct _portSupplyEventValue_t
{
	unsigned int m_port;
	unsigned int m_value;
}portSupplyEventValue_t;

typedef enum
{
	PSE_CHIP_0,
	PSE_CHIP_1,
	PSE_CHIP_2,
	PSE_CHIP_3,
	PSE_CHIP_MAX
}PSE_Chip_Num;

typedef enum
{
	PSE_PORT_0,
	PSE_PORT_1,
	PSE_PORT_2,
	PSE_PORT_3,
	PSE_PORT_4,
	PSE_PORT_5,
	PSE_PORT_6,
	PSE_PORT_7,
	PSE_PORT_MAX
}PSE_Port_Num;


typedef enum
{
    NO_FAULT,
    UV_OV_FAULT,    /**< Under Voltage/Over Voltage fault */
    THERMAL_SHUTDOWN_FAULT, /**< thermal shutdown fault */
    OVERLOAD_CURRENT,       /**< overload current > 50ms fault */
    LOAD_DISCONNECT,                /**< load disconnect */
    RESERVED1_FAULT,        /**< reserved for future */
    RESERVED2_FAULT,        /**< reserved for future */
    RESERVED3_FAULT,        /**< reserved for future */
    OVER_TOTAL_POWER       
}PSE_Port_FAULT_INFO;


typedef enum
{
	POWER_DISABLED,
	POWER_SEARCHING,
	POWER_DELIVERY,
}PSE_Port_Power_Delivery_State;

typedef enum
{
	NOT_USE_PSE_PORT,
	USE_PSE_PORT
}PSE_Port_Power_Config_State;

typedef struct
{
	PSE_Port_Num m_port_num;
	PSE_Port_Power_Delivery_State m_power_delivery_state;
	PSE_Port_Power_Config_State m_power_config_state;
	float m_power;
	PSE_Port_FAULT_INFO m_fault_info;
}PSE_Port_Status_t;

typedef struct
{
	PSE_Port_Status_t mAllPSEPortStatus[PSE_PORT_MAX];
}All_PSE_Port_Status_t;

typedef struct
{
	unsigned int m_forced_off_cnt;
	float m_total_power;
	float m_used_power;
	PSE_Port_Power_Delivery_State m_power_stat;
    unsigned short m_over_budget_stat;
	long m_over_budget_time;
	long m_last_power_on_time;
}PSE_Chip_Status_t;

typedef struct
{
	unsigned short m_forced_off;
	unsigned short m_over_port_budget;
	long m_on_time;
	long m_over_time;
}PSE_Manager_Status_t;

typedef struct 
{
	All_PSE_Port_Status_t mPSEPortStatus;
	PSE_Manager_Status_t mPowerManagerStatus[PSE_PORT_MAX];
	PSE_Chip_Status_t mPSEChipStatus[PSE_CHIP_MAX];

	unsigned int m_pse_port_max;
	unsigned int m_pse_chip_max;
	unsigned int m_pse_chip_sel_port;

	PSE_Port_Power_Config_State mLastPowerConfigState[PSE_PORT_MAX];
}PSE_Budget_t;

unsigned int IIC_device_addr[PSE_CHIP_MAX] = {0x20, 0x21, 0x22, 0x24};

void get_time(long *uptime);
int pse_port_set(int port, int enable);
float get_port_voltage(int port);
float get_port_current(int port);
float get_port_power(int port);
int get_pg_status(int port);
int get_recent_powered_port(void);
int get_highest_powered_port(void);
int get_port_power_delivery_state(int port);
int get_pe_status(int port);
void check_power_config_state(int port);
int get_board_id(int fs);
int pse_budget_init(void);
void power_manager(unsigned char chip_num, float free_power, unsigned int force_off_port_count);
void check_status_body(unsigned char chip_num);

#endif 

