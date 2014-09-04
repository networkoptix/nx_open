//
// Copyright (c) 2009 ITE Technologies, Inc. All rights reserved.
// 
// Date:
//    2013/08/09
//
// Module Name:
//    DTVAPI.h
//
//Abstract:
//    ITE Linux API header file.
//

#ifndef _DEMOD_DTVAPI_
#define _DEMOD_DTVAPI_

#define DTVAPI_VERSION 0x13081201
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include "it930x/type.h"
#include "it930x/iocontrol.h"
#include "it930x/error.h"

#define DTVEXPORT 

//
// The type defination of Priority.
//
typedef enum {
    DTVPriority_HIGH = 0,			// DVB-T and DVB-H - identifies high-priority stream.
    DTVPriority_LOW				// DVB-T and DVB-H - identifies low-priority stream.
} DTVPriority;

//
// The type defination of IpVersion.
//
typedef enum {
	DTVIpVersion_IPV4 = 0,		// The IP version if IPv4.
	DTVIpVersion_IPV6 = 1			// The IP version if IPv6.
} DTVIpVersion;

//
// The type defination of Target.
//
typedef enum {
	DTVSectionType_MPE = 0,		// Stands for MPE data.
	DTVSectionType_SIPSI,		// Stands for SI/PSI table, but don't have to specify table ID.
	DTVSectionType_TABLE			// Stands for SI/PSI table.
} DTVSectionType;

//
// The type defination of FrameRow.
//
typedef enum {
	DTVFrameRow_256 = 0,			// There should be 256 rows for each column in MPE-FEC frame.
	DTVFrameRow_512,				// There should be 512 rows for each column in MPE-FEC frame.
	DTVFrameRow_768,				// There should be 768 rows for each column in MPE-FEC frame.
	DTVFrameRow_1024				// There should be 1024 rows for each column in MPE-FEC frame.
} DTVFrameRow;

//
// The type defination of Pid.
//
// as sectionType = SectionType_SIPSI: only value is valid.
// as sectionType = SectionType_TABLE: both value and table is valid.
// as sectionType = SectionType_MPE: except table all other fields is valid.
//
typedef struct {
	Byte table;					// The table ID. Which is used to filter specific SI/PSI table.
	Byte duration;				// The maximum burst duration. It can be specify to 0xFF if user don't know the exact value.
	DTVFrameRow frameRow;		// The frame row of MPE-FEC. It means the exact number of rows for each column in MPE-FEC frame.
	DTVSectionType sectionType;	// The section type of pid. See the defination of SectionType.
	DTVPriority priority;		// The priority of MPE data. Only valid when sectionType is set to SectionType_MPE.
	DTVIpVersion version;		// The IP version of MPE data. Only valid when sectionType is set to SectionType_MPE.
	Bool cache;					// True: MPE data will be cached in device's buffer. Fasle: MPE will be transfer to host.
	Word value;					// The 13 bits Packet ID.
} DTVPid, *PDTVPid;

//
// The type defination of TransmissionMode.
//
typedef enum {
    DTVTransmissionMode_2K = 0,	// OFDM frame consists of 2048 different carriers (2K FFT mode).
    DTVTransmissionMode_8K = 1,	// OFDM frame consists of 8192 different carriers (8K FFT mode).
    DTVTransmissionMode_4K = 2	// OFDM frame consists of 4096 different carriers (4K FFT mode).
} DTVTransmissionMode;

//
// The type defination of Constellation.
//
typedef enum {
    DTVConstellation_QPSK = 0,	// Signal uses QPSK constellation.
    DTVConstellation_16QAM,		// Signal uses 16QAM constellation.
    DTVConstellation_64QAM		// Signal uses 64QAM constellation.
} DTVConstellation;

//
// The type defination of Interval.
//
typedef enum {
    DTVInterval_1_OVER_32 = 0,	// Guard interval is 1/32 of symbol length.
    DTVInterval_1_OVER_16,		// Guard interval is 1/16 of symbol length.
    DTVInterval_1_OVER_8,		// Guard interval is 1/8 of symbol length.
    DTVInterval_1_OVER_4			// Guard interval is 1/4 of symbol length.
} DTVInterval;

//
// The type defination of CodeRate.
//
typedef enum {
    DTVCodeRate_1_OVER_2 = 0,	// Signal uses FEC coding ratio of 1/2.
    DTVCodeRate_2_OVER_3,		// Signal uses FEC coding ratio of 2/3.
    DTVCodeRate_3_OVER_4,		// Signal uses FEC coding ratio of 3/4.
    DTVCodeRate_5_OVER_6,		// Signal uses FEC coding ratio of 5/6.
    DTVCodeRate_7_OVER_8,		// Signal uses FEC coding ratio of 7/8.
    DTVCodeRate_NONE				// None, NXT doesn't have this one.
} DTVCodeRate;

//
// TPS Hierarchy and Alpha value.
//
typedef enum {
    DTVHierarchy_NONE = 0,		// Signal is non-hierarchical.
    DTVHierarchy_ALPHA_1,		// Signalling format uses alpha of 1.
    DTVHierarchy_ALPHA_2,		// Signalling format uses alpha of 2.
    DTVHierarchy_ALPHA_4			// Signalling format uses alpha of 4.
} DTVHierarchy;

//
// The type defination of Bandwidth.
//
typedef enum {
    DTVBandwidth_6M = 0,			// Signal bandwidth is 6MHz.
    DTVBandwidth_7M,				// Signal bandwidth is 7MHz.
    DTVBandwidth_8M,				// Signal bandwidth is 8MHz.
    DTVBandwidth_5M				// Signal bandwidth is 5MHz.
} DTVBandwidth;

//
// The defination of ChannelInformation.
//
typedef struct {
    Dword frequency;								// Channel frequency in KHz.
    DTVTransmissionMode transmissionMode;		// Number of carriers used for OFDM signal.
    DTVConstellation constellation;				// Constellation scheme (FFT mode) in use.
    DTVInterval interval;						// Fraction of symbol length used as guard (Guard Interval).
    DTVPriority priority;						// The priority of stream.
    DTVCodeRate highCodeRate;					// FEC coding ratio of high-priority stream.
    DTVCodeRate lowCodeRate;						// FEC coding ratio of low-priority stream.
    DTVHierarchy hierarchy;						// Hierarchy levels of OFDM signal.
    DTVBandwidth bandwidth;
} DTVChannelTPSInfo, *PDTVChannelTPSInfo;

//
//The type defination of statistic
//
typedef struct{
	Dword postVitErrorCount;		// ErrorBitCount.
	Dword postVitBitCount;		// TotalBitCount.
	Word abortCount;				// Number of abort RSD packet.
	Word signalQuality;			// Signal quality (0 - 100).
	Word signalStrength;			// Signal strength (0 - 100).
	Bool signalPresented;		// TPS lock.
	Bool signalLocked;			// MPEG lock.
	Byte frameErrorCount;		// Frame Error Ratio (error ratio before MPE-FEC) = frameErrorRate / 128.
	Byte mpefecFrameErrorCount;	// MPE-FEC Frame Error Ratio (error ratio after MPE-FEC) = mpefecFrameErrorCount / 128.
}DTVStatistic, *PDTVStatistic;

//
// The type defination of handle type.
//
typedef enum {
    OMEGA = 0,
    ENDEAVOUR,
    SAMBA
} HandleType;

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Open device.
//
//	PARAMETERS:
//		handle_type	- One of device type
//						OMEGA		: it913x
//						ENDEAVOUR 	: it930x
//						SAMBA		: it9175
//		handle_number
// 
//	RETURNS:
//		handle if no error, negative value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT int DTV_DeviceOpen(
	IN	HandleType handle_type,
	IN	Byte handle_number);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Clean up & Power off device.
//
//	PARAMETERS:
//		handle
// 
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_DeviceClose(
	IN	int handle);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Specify the bandwidth of channel and tune the channel to the specific frequency.
//
//	PARAMETERS:
//		handle
//		frequency	- Channel frequnecy in KHz.
//		bandwidth	- Channel bandwidth in KHz or MHz (if the bandwidth input is less than 10,
//					it will be multiplied by 1000 (and coverted to KHz) automatically).
// 
//  RETURNS:
//      0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_AcquireChannel(
	IN	int handle,
	IN	Dword frequency,
	IN	Word bandwidth);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Check if TPS and MPEG2 module both are locked.
//
//	PARAMETERS:
//		handle
//		is_lock		- return 1 if channel is locked.
//
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_IsLocked(
	IN	int handle,
	OUT	Bool *is_lock);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Get post-vit ber, signal quality, signal strength.
//
//	PARAMETERS:
//		handle
//		dtv_statisic	- return signal & channel statistic
//	RETURNS:
//		0 if no error , non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_GetStatistic(
	IN	int handle,
	OUT	PDTVStatistic dtv_statisic);

//-----------------------------------------------------------------------------
//	PURPOSE:
//	Set the counting range for Pre-Viterbi and Post-Viterbi.
//
//	PARAMETERS:
//		handle
//		bySuperFrameCount	- The count of super frame for Pre-Viterbi (Ex. 1, 2).
//		wPacketUnit		- The count of packet unit for Post-Viterbi (Ex, 1000, 10000, 20000).
//
//  RETURNS:
//      0 if no error , non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_SetStatisticRange(
	IN	int handle,
	IN	Byte bySuperFrameCount,
	IN	Word wPacketUnitCount);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Get channel TPS(Transmission Parameter Signaling) information.
//
//	PARAMETERS:
//		handle
//		pChannelTPSInfo - Return the structure that store all TPS information.
//
//	RETURNS:
//		0 if no error , non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_GetChannelTPSInfo(
	IN	int handle,
	OUT	PDTVChannelTPSInfo pChannelTPSInfo);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Enable Pid Table, for DVB-T mode.
//
//	PARAMETERS:
//		handle
//
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_EnablePIDTable(
	IN	int handle);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Disable Pid Table, for DVB-T mode.
//
//	PARAMETERS:
//		handle
// 
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_DisablePIDTable(
	IN	int handle);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Add Pid number, for DVB-T mode.
//
//	PARAMETERS:
//		handle
//		byIndex 		- 0 ~ 31.
//		wProgId 		- pid number.
//
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_AddPID(
	IN	int handle,
	IN	Byte byIndex,
	IN	Word wProgId);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Remove Pid number, for DVB-T mode.
//
//	PARAMETERS:
//		handle
//		byIndex		 - 0 ~ 31.
//		wProgId		 - pid number.
//
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_RemovePID(
	IN	int handle,
	IN 	Byte byIndex,
	IN 	Word wProgId);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Clear all the PID's set previously by DTV_AddPID(), for DVB-T mode.
//
//	PARAMETERS:
//		handle
//
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_ResetPIDTable(
	IN	int handle);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Start to Capture data from device, for DVB-T/TDMB/FM mode.
//		If the the channle is locked properly, the drievr starts to receive TS data and
//		store it in the ring buffer.
//
//	PARAMETERS:
//		handle
// 
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
Dword DTV_StartCapture(
	IN	int handle);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Stop to Capture data from device, for DVB-T/TDMB/FM mode.
//
//	PARAMETERS:
//		handle
// 
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_StopCapture(
	IN	int handle);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Control Power Saving. The function can be called by application for power saving while idle mode.
//
//	PARAMETERS:
//		handle
//		byCtrl 		- 1: Power Up, 0: Power Down.
//						Power Up :  Resume  device to normal state.
//						Power Down : Suspend device to hibernation state.
// 
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_ControlPowerSaving(
	IN	int handle,
	IN	Byte byCtrl);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Get Driver & API Version.
//
//	PARAMETERS:
//		handle
//		pDriverInfo	- Return driver information with DTVDriverInfo structure.
// 
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
Dword DTV_GetDriverInformation(
	IN	int handle,
	OUT	PDemodDriverInfo pDriverInfo);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Get data from driver, for DVB-T/TDMB/FM mode.
//
//	PARAMETERS:
//		handle
//		buffer			- Data buffer point.
//		read_length	- read data length.
// 
//	RETURNS:
//		Buffer filled length actually.
//-----------------------------------------------------------------------------
DTVEXPORT int DTV_GetData(
	IN	int handle,
	IN	int read_length,
	OUT	Byte *buffer);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Read EEPROM data.
//
//	PARAMETERS:
//		handle
//		wRegAddr		- Register address.
//		pbyData		- Register value.
// 
//	RETURNS:
//		0 if no error, non-zero value otherwise.
// -----------------------------------------------------------------------------
DTVEXPORT Dword DTV_ReadEEPROM(
	IN	int handle,
	IN	Word wRegAddr,
	OUT	Byte *pbyData);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Read Write register.
//
//	PARAMETERS:
//		handle
//		RW_type		- 0: read, 1: write
//		processor_type- 0: LINK, 1: OFDM
//		reg_addr		- Register address.
//		data			- Register value.
//
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_RegisterControl(
	IN	int handle,
	IN	int RW_type,
	IN	int processor_type,
	IN	Word reg_addr,
	IN	OUT Byte *data);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Read Rx device ID.
//
//	PARAMETERS:
//		handle
//		rx_device_id	- Return ID.
//
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_GetDeviceID(
	IN	int handle,
    OUT	Word *rx_device_id);

///--------------------------------------------ENDEAVOUR--------------------------------------------///

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Read Write Endeavour register.
//
//	PARAMETERS:
//		handle
//		RW_type		- 0: read, 1: write
//		processor_type- 0: LINK, 1: OFDM
//		reg_addr		- Register address.
//		data			- Register value.
//
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_EndeavourRegisterControl(
	IN	int handle,
	IN	int RW_type,
	IN	int processor_type,
	IN	Word reg_addr,
	IN	OUT Byte *data);

//-----------------------------------------------------------------------------
//	PURPOSE:
//		Set Endeavour PID Filter.
//
//	PARAMETERS:
//		handle
//		oldPID
//		newPID
//		tableLen
//
//	RETURNS:
//		0 if no error, non-zero value otherwise.
//-----------------------------------------------------------------------------
DTVEXPORT Dword DTV_NULLPacket_Pilter(
	IN int handle,
	IN PPIDFilter PIDfilter);
	
//-----------------------------------------------------------------------------
//	PURPOSE:
//		Write EEPROM data.
//
//	PARAMETERS:
//		handle
//		pbyData		- Register value.
// 
//	RETURNS:
//		0 if no error, non-zero value otherwise.
// -----------------------------------------------------------------------------
DTVEXPORT Dword DTV_WriteEEPROM(
	IN	int handle,
	IN	Byte *pbyData);

/*
DTVEXPORT Dword DTV_EndeavourSetPIDFilter(
	IN	int handle,
	IN	Word *oldPID,
	IN	Word *newPID,
	IN	Word tableLen);
*/
#endif 
