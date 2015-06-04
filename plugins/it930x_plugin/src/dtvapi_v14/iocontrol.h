/**
 *
 * Copyright (c) 2006 Afa Corporation. All rights reserved. 
 *
 * Module Name:
 *   iocontrol.h
 *
 * Abstract:
 *   The structure and IO code for IO control call.
 *
 */

#ifndef __IOCONTROL_H__
#define __IOCONTROL_H__

#include "type.h"

/* Use 'k' as magic number */
#define AFA_IOC_MAGIC  'k'

typedef struct {
    Byte		chip;
    Processor	processor;
    Word		registerAddress;
    Byte		bufferLength;
    Byte		buffer[256];
    u32			error;
    Byte		reserved[16];
} RegistersRequest, *PRegistersRequest;

typedef struct {
    Byte	chip;
    Word	registerAddress;
    Byte	bufferLength;
    Byte	buffer[256];
    Dword	error;
    Byte	reserved[16];
} ReadEepromValuesRequest, *PReadEepromValuesRequest;

typedef struct {
    Byte				chip;
	ChannelModulation	*channelModulation;
    Dword				error;
    Byte				reserved[16];
} GetChannelModulationRequest, *PGetChannelModulationRequest;

typedef struct {
    Byte	chip;
    Word	bandwidth;
    u32		frequency;
    u32		error;
    Byte	reserved[16];
} AcquireChannelRequest, *PAcquireChannelRequest;

typedef struct {
    Byte	chip;
    u32		locked;
    u32		error;
    Byte	reserved[16];
} IsLockedRequest, *PIsLockedRequest;

typedef struct {
	Byte	chip;
	Byte	index;
    Pid		pid;
    u32		error;
    Byte	reserved[16];
} AddPidAtRequest, *PAddPidAtRequest;

typedef struct {
	Byte	chip;
	Byte	index;
    Pid		pid;
    u32		error;
    Byte	reserved[16];
} RemovePidAtRequest, *PRemovePidAtRequest;

typedef struct {
	Byte	chip;
    u32		error;
    Byte	reserved[16];
} ResetPidRequest, *PResetPidRequest;

typedef struct {
    Byte	chip;
    Byte	superFrameCount;
    Word	packetUnit;
    Dword	error;
    Byte	reserved[16];
} SetStatisticRangeRequest, *PSetStatisticRangeRequest;

typedef struct {
    Byte				chip;
    ChannelStatistic	channelStatistic;
    u32					error;
    Byte				reserved[16];
} GetChannelStatisticRequest, *PGetChannelStatisticRequest;

typedef struct {
    Byte		chip;
    Statistic	statistic;
    u32			error;
    Byte		reserved[16];
} GetStatisticRequest, *PGetStatisticRequest;

typedef struct {
    Byte	chip;
	Byte	control;
    u32		error;
    Byte	reserved[16];
} ControlPidFilterRequest, *PControlPidFilterRequest;

typedef struct {
    Byte	chip;
    Byte	control;
    Dword	error;
    Byte	reserved[16];
} ControlPowerSavingRequest, *PControlPowerSavingRequest;

typedef struct {
    Byte	DriverVersion[16];	/** XX.XX.XX.XX Ex., 1.2.3.4 **/
    Byte	APIVersion[32];		/** XX.XX.XXXXXXXX.XX Ex., 1.2.3.4	**/
    Byte	FWVersionLink[16];	/** XX.XX.XX.XX Ex., 1.2.3.4 **/
    Byte	FWVersionOFDM[16];	/** XX.XX.XX.XX Ex., 1.2.3.4 **/
    Byte	DateTime[24];			/** Ex.,"2004-12-20 18:30:00" or "DEC 20 2004 10:22:10" with compiler __DATE__ and __TIME__  definitions **/
    Byte	Company[8];			/** Ex.,"ITEtech" **/
    Byte	SupportHWInfo[32];	/** Ex.,"Jupiter DVBT/DVBH" **/
    u32		error;
    Byte	reserved[128];
} DemodDriverInfo, *PDemodDriverInfo;

typedef struct {
	Byte	chip;
    u32		error;
    Byte	reserved[16];
} StartCaptureRequest, *PStartCaptureRequest;

typedef struct {
	Byte	chip;
    u32		error;
    Byte	reserved[16];
} StopCaptureRequest, *PStopCaptureRequest;

typedef struct{
	Byte chip;
	Long strengthDbm;
	Dword error;
	Byte reserved[16];
} GetSignalStrengthDbmRequest, *PGetSignalStrengthDbmRequest;

typedef struct {
	Word	oldPID[64];
	Word	newPID[64];
	Word	tableLen;
	u32		error;
}PIDFilter, *PPIDFilter;

typedef struct {
	Byte			chip;
    u32				error;
    Byte			reserved[16];
} SetUartBaudrate, *PSetUartBaudrate;

typedef struct {
	Dword			len;
    Dword			cmd;
    Byte			reserved[16];
} SentUartData, *PSentUartData;

typedef struct {
    Byte	chip;
    Word	registerAddress;
    Byte	bufferLength;
    Byte	buffer[256];
    Dword	error;
    Byte	reserved[16];
} WriteEepromValuesRequest, *PWriteEepromValuesRequest;


/**
 * Demodulator API commands
 */
#define IOCTRL_ITE_GROUP_STANDARD	0x000
#define IOCTRL_ITE_GROUP_DVBT		0x100
#define IOCTRL_ITE_GROUP_DVBH		0x200
#define IOCTRL_ITE_GROUP_FM			0x300
#define IOCTRL_ITE_GROUP_TDMB		0x400
#define IOCTRL_ITE_GROUP_OTHER		0x500
#define IOCTRL_ITE_GROUP_ENDEAVOUR	0x600

/**
 * Demodulator Stream control API commands
 */

/***************************************************************************/
/*                             STANDARD                                    */
/***************************************************************************/

/**
 * Write a sequence of bytes to the contiguous registers in demodulator.
 * Paramters:   RegistersRequest struct
 */
#define IOCTL_ITE_DEMOD_WRITEREGISTERS \
	_IOW(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_STANDARD + 0x00, RegistersRequest)

/**
 * Write a sequence of bytes to the contiguous cells in the EEPROM.
 * Paramters:   WriteEepromValuesRequest struct
 */
#define IOCTL_ITE_DEMOD_WRITEEEPROMVALUES \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_STANDARD + 0x02, WriteEepromValuesRequest)

/**
 * Read a sequence of bytes from the contiguous registers in demodulator.
 * Paramters:   RegistersRequest struct
 */
#define IOCTL_ITE_DEMOD_READREGISTERS \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_STANDARD + 0x06, RegistersRequest)

/**
 * Read a sequence of bytes from the contiguous cells in the EEPROM.
 * Paramters:   ReadEepromValuesRequest struct
 */
#define IOCTL_ITE_DEMOD_READEEPROMVALUES \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_STANDARD + 0x08, ReadEepromValuesRequest)

/**
 * Set the output stream type of chip. Because the device could output in 
 * many stream type, therefore host have to choose one type before receive 
 * data.
 * Paramters:   GetChannelModulationRequest struct
 */
#define IOCTL_ITE_DEMOD_GETCHANNELMODULATION \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_STANDARD + 0x12, GetChannelModulationRequest)

/**
 * Specify the bandwidth of channel and tune the channel to the specific
 * frequency. Afterwards, host could use output parameter dvbH to determine
 * if there is a DVB-H signal.
 * In DVB-T mode, after calling this function output parameter dvbH should
 * be False and host could use output parameter "locked" to indicate if the 
 * TS is correct.
 * In DVB-H mode, after calling this function output parameter dvbH should
 * be True and host could use Jupiter_acquirePlatorm to get platform. 
 * Paramters:   AcquireChannelRequest struct
 */
#define IOCTL_ITE_DEMOD_ACQUIRECHANNEL \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_STANDARD + 0x14, AcquireChannelRequest)

/**
 * Get all the platforms found in current frequency.
 * Paramters:	IsLockedRequest struct
 */
#define IOCTL_ITE_DEMOD_ISLOCKED \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_STANDARD + 0x15, IsLockedRequest)

/**
 * Get the statistic values of demodulator, it includes Pre-Viterbi BER,
 * Post-Viterbi BER, Abort Count, Signal Presented Flag, Signal Locked Flag,
 * Signal Quality, Signal Strength, Delta-T for DVB-H time slicing.
 * Paramters:	SetStatisticRangeRequest struct
 */
#define IOCTL_ITE_DEMOD_SETSTATISTICRANGE \
	_IOW(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_STANDARD + 0x16, SetStatisticRangeRequest)

/**
 * Get the statistic values of demodulator, it includes Pre-Viterbi BER,
 * Post-Viterbi BER, Abort Count, Signal Presented Flag, Signal Locked Flag,
 * Signal Quality, Signal Strength, Delta-T for DVB-H time slicing.
 * Paramters:	GetStatisticRequest struct
 */
#define IOCTL_ITE_DEMOD_GETSTATISTIC \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_STANDARD + 0x18, GetStatisticRequest)

/**
 * Get the statistic values of demodulator, it includes Pre-Viterbi BER,
 * Post-Viterbi BER, Abort Count, Signal Presented Flag, Signal Locked Flag,
 * Signal Quality, Signal Strength, Delta-T for DVB-H time slicing.
 * Paramters:	GetChannelStatisticRequest struct
 */
#define IOCTL_ITE_DEMOD_GETCHANNELSTATISTIC \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_STANDARD + 0x19, GetChannelStatisticRequest)

/**
 *
 * Paramters:	ControlPowerSavingRequest struct
 */
#define IOCTL_ITE_DEMOD_CONTROLPOWERSAVING \
	_IOW(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_STANDARD + 0x1E, ControlPowerSavingRequest)


/***************************************************************************/
/*                                DVBT                                     */
/***************************************************************************/

/**
 * Reset PID from PID filter.
 * Paramters:	ResetPidRequest struct
 */
#define IOCTL_ITE_DEMOD_RESETPID \
	_IOW(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_DVBT + 0x02, ResetPidRequest)

/**
 * Enable PID filter.
 * Paramters:	ControlPidFilterRequest struct
 */
#define IOCTL_ITE_DEMOD_CONTROLPIDFILTER \
	_IOW(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_DVBT + 0x03, ControlPidFilterRequest)

/**
 * Add PID to PID filter.
 * Paramters:	AddPidAtRequest struct
 */
#define IOCTL_ITE_DEMOD_ADDPIDAT \
	_IOW(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_DVBT + 0x04, AddPidAtRequest)

/**
 * Remove PID from PID filter.
 * Paramters:	RemovePidAtRequest struct
 */
#define IOCTL_ITE_DEMOD_REMOVEPIDAT \
	_IOW(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_DVBT + 0x05, RemovePidAtRequest)


/***************************************************************************/
/*                                OTHER                                    */
/***************************************************************************/

/**
 * Get driver information.
 * Paramters:	DemodDriverInfo struct
 */
#define IOCTL_ITE_DEMOD_GETDRIVERINFO \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_OTHER + 0x00, DemodDriverInfo)

/**
 * Start capture data stream
 * Paramters: StartCaptureRequest struct
 */
#define IOCTL_ITE_DEMOD_STARTCAPTURE \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_OTHER + 0x01, StartCaptureRequest)

/**
 * Stop capture data stream
 * Paramters: StopCaptureRequest struct
 */
#define IOCTL_ITE_DEMOD_STOPCAPTURE \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_OTHER + 0x02, StopCaptureRequest)

/**
 * Get signail strength dbm
 * Paramters:	GetSignalStrengthDbmRequest struct
 */
#define IOCTL_ITE_DEMOD_GETSIGNALSTRENGTHDBM \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_OTHER + 0x07, GetSignalStrengthDbmRequest)
	
/**
 * Get Device ID for Return Channel System
 */
#define IOCTL_ITE_DEMOD_GETRXDEVICEID \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_OTHER + 0x08, Word)
	
/**
 * Copy return channel packets to virtual COM driver
 */
#define IOCTL_ITE_DEMOD_DEALWITH_RETURNCHANNELPID \
	_IOW(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_OTHER + 0x09, int)


/***************************************************************************/
/*                                Endeavour                                    */
/***************************************************************************/

/**
 * Write a sequence of bytes to the contiguous registers in endeavour.
 * Paramters:	RegistersRequest struct
 */
#define IOCTL_ITE_ENDEAVOUR_WRITEREGISTERS \
	_IOW(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_ENDEAVOUR + 0x00, RegistersRequest)

/**
 * Read a sequence of bytes from the contiguous registers in endeavour.
 * Paramters:	RegistersRequest struct
 */
#define IOCTL_ITE_ENDEAVOUR_READREGISTERS \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_ENDEAVOUR + 0x01, RegistersRequest)

/**
 * Set Endeavour PID Filter.
 * Paramters:	PIDFilter struct
 */
#define IOCTL_ITE_ENDEAVOUR_SETPIDFILTER \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_ENDEAVOUR + 0x02, PIDFilter)

/**
 * Endeavour Set UART Baudrate.
 * Paramters:	StartTransferRequest struct
 */
#define IOCTL_ITE_ENDEAVOUR_SETUARTBAUDRATE \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_ENDEAVOUR + 0x03, SetUartBaudrate)

/**
 * Endeavour Sent UART Data.
 * Paramters:	CmdRequest
 */	
#define IOCTL_ITE_ENDEAVOUR_SENTUARTDATA \
	_IOR(AFA_IOC_MAGIC, IOCTRL_ITE_GROUP_ENDEAVOUR + 0x04, SentUartData)


Dword DemodIOCTLFun(
	void *demodulator,
	Dword IOCTLCode,
	unsigned long pIOBuffer,
	Byte number);

#endif
