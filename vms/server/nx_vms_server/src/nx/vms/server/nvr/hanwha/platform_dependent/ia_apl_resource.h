#ifndef __IA_RESOURCE__
#define __IA_RESOURCE__

#define ALARMIN_MAX		4
#define ALARMOUT_MAX	2

#define RES_SET_ALARMOUT0       0x01
#define RES_SET_ALARMOUT1       0x02


#define RES_GET_ALARMIN0		0x11
#define RES_GET_ALARMIN1		0x12
#define RES_GET_ALARMIN2		0x13
#define RES_GET_ALARMIN3		0x14
#define RES_GET_BOARDID			0x20
#define RES_SET_BUZZER          0x30
#define RES_SET_BACKUP_LED      0x32
#define RES_SET_ALARM_LED       0x33
#define RES_SET_REC_LED         0x34
#define RES_GET_FAN_ALM         0x43

#endif
