/* DTVAPI.cpp : Defines the entry point for the DLL application. */

#include "DTVAPI.h"

int DTV_DeviceOpen(
	IN	HandleType handle_type,
	IN	Byte handle_number)
{
	int handle = 0, ret = 0;
	char *handle_path = "";
	ret += 0;
	switch(handle_type){
		case OMEGA:
			ret = asprintf(&handle_path, "/dev/usb-it913x%d", handle_number);
			break;
		
		case ENDEAVOUR:
			ret = asprintf(&handle_path, "/dev/usb-it930x%d", handle_number);
			break;
		
		case SAMBA:
			ret = asprintf(&handle_path, "/dev/usb-it9175%d", handle_number);
			break;
		
		default:
			printf("Handle type error[%d]\n", handle_type);
			return -1;
	}	
	
	handle = open(handle_path, O_RDWR);
	
	if(handle > 0){
		printf("Open %s success\n", handle_path);
	}
	else{
		printf("Open %s fail\n", handle_path);
		return -1;
	}
	
	return handle;
}

Dword DTV_DeviceClose(
	IN	int handle)
{
    Dword result = ERR_NO_ERROR;

    if(handle > 0){
		DTV_ResetPIDTable(handle);
		DTV_DisablePIDTable(handle);
		DTV_StopCapture(handle);
		close(handle);
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_ControlPowerSaving(
	IN	int handle,
    IN	Byte byCtrl)
{
	Dword result = ERR_NO_ERROR;
	ControlPowerSavingRequest request;
	
	if(handle > 0){
		request.control = byCtrl;
		if(ioctl(handle, IOCTL_ITE_DEMOD_CONTROLPOWERSAVING, (void *)&request))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = request.error;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_AcquireChannel(
	IN	int handle,
    IN	Dword frequency,	// Channel Frequency (KHz)
    IN	Word bandwidth)	// Channel Bandwidth (KHz)
{
	Dword result = ERR_NO_ERROR;
	AcquireChannelRequest request;
	
	/* For support KHz or MHz */
	if(bandwidth < 10)
		bandwidth *= 1000;
	
	if(handle > 0){
		request.frequency = frequency;
		request.bandwidth = bandwidth;
		if(ioctl(handle, IOCTL_ITE_DEMOD_ACQUIRECHANNEL, (int *)&request))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = request.error;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_IsLocked(
	IN	int handle,
	OUT	Bool *is_lock)
{
	Dword result = ERR_NO_ERROR;
	IsLockedRequest request;	
	Bool pislock;
	
	if(handle > 0){
		//request.locked = is_lock;
		request.locked = &pislock; //xx
		
		if(ioctl(handle, IOCTL_ITE_DEMOD_ISLOCKED, (double *)&request))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = request.error;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	*is_lock = request.locked;
	
	return result;
}

Dword DTV_EnablePIDTable(
	IN	int handle)
{
	Dword result = ERR_NO_ERROR;
	ControlPidFilterRequest request;
	
	if(handle > 0){
		request.control = 1;
		if(ioctl(handle, IOCTL_ITE_DEMOD_CONTROLPIDFILTER, (void *)&request))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = request.error;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_DisablePIDTable(
	IN	int handle)
{
	Dword result = ERR_NO_ERROR;
	ControlPidFilterRequest request;
	
	if(handle > 0){
		request.control = 0;
		if(ioctl(handle, IOCTL_ITE_DEMOD_CONTROLPIDFILTER, (void *)&request))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = request.error;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_AddPID(
	IN	int handle,
	IN	Byte byIndex,            // 0 ~ 31
	IN	Word wProgId)            // pid number
{	
	Dword result = ERR_NO_ERROR;
	AddPidAtRequest request;
	Pid pid;
	memset(&pid, 0, sizeof(pid));
	
	pid.value = (Word)wProgId;
	
	if(handle > 0){
		request.pid = pid;
		request.index = byIndex;
		if(ioctl(handle, IOCTL_ITE_DEMOD_ADDPIDAT, (void *)&request))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = request.error;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_RemovePID(
	IN	int handle,
    IN	Byte byIndex,            // 0 ~ 31
	IN	Word wProgId)            // pid number
{	
	Dword result = ERR_NO_ERROR;
	RemovePidAtRequest request;
	Pid pid;
	memset(&pid, 0, sizeof(pid));    
	
	pid.value = (Word)wProgId;
	
	if(handle > 0){
		request.pid = pid;
		request.index = byIndex;
		if(ioctl(handle, IOCTL_ITE_DEMOD_REMOVEPIDAT, (void *)&request))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = request.error;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_ResetPIDTable(
	IN	int handle)
{
	Dword result = ERR_NO_ERROR;
	ResetPidRequest request;
	
	if(handle > 0){
		if(ioctl(handle, IOCTL_ITE_DEMOD_RESETPID, (void *)&request))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = request.error;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_GetStatistic(
	IN	int handle,
	OUT	PDTVStatistic dtv_statisic)
{
	Dword result = ERR_NO_ERROR;
	GetStatisticRequest get_signal_statistic;
	Statistic signal_statistic;
	GetChannelStatisticRequest get_channel_statistic;
	ChannelStatistic channel_statistic;
	
	channel_statistic.abortCount = 0;
	channel_statistic.postVitBitCount = 0;
	channel_statistic.postVitErrorCount = 0;
	signal_statistic.signalLocked = 0;
	signal_statistic.signalPresented = 0;
	signal_statistic.signalStrength = 0;
	signal_statistic.signalQuality = 0;
	
	get_signal_statistic.statistic = signal_statistic;
	get_channel_statistic.channelStatistic = channel_statistic;
	
	if(handle > 0){
		/* Signal */
		if(ioctl(handle, IOCTL_ITE_DEMOD_GETSTATISTIC, (void *)&get_signal_statistic)){
			result = ERR_CANT_FIND_USB_DEV;
			goto EXIT;
		}
		else{
			result = get_signal_statistic.error;
		}
			
		signal_statistic = get_signal_statistic.statistic;			
	
		/* Channel */
		if(ioctl(handle, IOCTL_ITE_DEMOD_GETCHANNELSTATISTIC, (void *)&get_channel_statistic)){
			result = ERR_CANT_FIND_USB_DEV;
			goto EXIT;
		}
		else{
			result = get_channel_statistic.error;
		}
		
		channel_statistic = get_channel_statistic.channelStatistic;

		dtv_statisic->signalQuality		= signal_statistic.signalQuality;
		dtv_statisic->signalStrength 	= signal_statistic.signalStrength;
		dtv_statisic->signalPresented 	= signal_statistic.signalPresented;
		dtv_statisic->signalLocked 		= signal_statistic.signalLocked;
		dtv_statisic->postVitErrorCount = channel_statistic.postVitErrorCount;
		dtv_statisic->postVitBitCount 	= channel_statistic.postVitBitCount;
		dtv_statisic->abortCount 		= channel_statistic.abortCount;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}

EXIT:
	return result;
}

Dword DTV_GetChannelTPSInfo(
	IN	int handle,
	OUT	PDTVChannelTPSInfo pChannelTPSInfo)
{
	Dword result = ERR_NO_ERROR;
	GetChannelModulationRequest request;
	
	if(handle > 0){
		request.channelModulation = (ChannelModulation *)pChannelTPSInfo;
		if(ioctl(handle, IOCTL_ITE_DEMOD_GETCHANNELMODULATION, (void *)&request))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = request.error;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_SetStatisticRange(
	IN	int handle,
	IN	Byte bySuperFrameCount,
	IN	Word wPacketUnitCount)
{
	Dword result = ERR_NO_ERROR;
	SetStatisticRangeRequest request;
	
	if(handle > 0){
		request.superFrameCount = bySuperFrameCount;
		request.packetUnit = wPacketUnitCount;
		if(ioctl(handle, IOCTL_ITE_DEMOD_SETSTATISTICRANGE, (void *)&request))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = request.error;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_StartCapture(
	IN	int handle)
{
    Dword result = ERR_NO_ERROR;
	
	if(handle > 0){
		if(ioctl(handle, IOCTL_ITE_DEMOD_STARTCAPTURE))
			result = ERR_CANT_FIND_USB_DEV;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_StopCapture(
	IN	int handle)
{
	Dword result = ERR_NO_ERROR;
	
	if(handle > 0){
		if(ioctl(handle, IOCTL_ITE_DEMOD_STOPCAPTURE))
			result = ERR_CANT_FIND_USB_DEV;
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_GetDriverInformation(
	IN	int handle,
	OUT	PDemodDriverInfo driver_info)
{
	Dword result = ERR_NO_ERROR;
	
	if(handle > 0){
		if(ioctl(handle, IOCTL_ITE_DEMOD_GETDRIVERINFO, (void *)driver_info))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = driver_info->error;
	}
	else {
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

int DTV_GetData(
	IN	int handle,
    IN	int read_length,
    OUT	Byte *buffer)
{
	int return_length = 0;
	
	if(handle > 0)
		return_length = read(handle, buffer, read_length);
	else
		return_length = -1;
	
    return return_length;
}

Dword DTV_ReadEEPROM(
	IN	int handle,
	IN	Word wRegAddr,
	OUT	Byte *pbyData)
{
	Dword result = ERR_NO_ERROR;
	ReadEepromValuesRequest request;
	
	if(handle > 0){
		request.registerAddress = wRegAddr;
		request.bufferLength = 1;
		if(ioctl(handle, IOCTL_ITE_DEMOD_READEEPROMVALUES, (void *)&request)){
			result = ERR_CANT_FIND_USB_DEV;
		}
		else{
			*pbyData = request.buffer[0];
			result = request.error;
		}
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_RegisterControl(
	IN	int handle,
	IN	int RW_type,
	IN	int processor_type,
	IN	Word reg_addr,
	IN	OUT Byte *data)
{
	Dword result = ERR_NO_ERROR;
	RegistersRequest request;
	
	if(processor_type == 1)
		request.processor = Processor_OFDM;
	else	
		request.processor = Processor_LINK;
	
	request.registerAddress = reg_addr;
	request.bufferLength = 1;
	
	if(handle > 0){
		switch(RW_type){
			case 0: //read
				if(ioctl(handle, IOCTL_ITE_DEMOD_READREGISTERS, (void *)&request)){
					result = ERR_CANT_FIND_USB_DEV;
				}
				else{
					*data = request.buffer[0];
					result = request.error;
				}
				break;

			case 1://write				
				memcpy(request.buffer, data, 1);
				if(ioctl(handle, IOCTL_ITE_DEMOD_WRITEREGISTERS, (void *)&request))
					result = ERR_CANT_FIND_USB_DEV;
				else
					result = request.error;
				
				break;
				
			default:
				result = ERR_NOT_IMPLEMENTED;
				break;
		}
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_GetDeviceID(
	IN	int handle,
	OUT	Word *rx_device_id)
{
	Dword result = ERR_NO_ERROR;
	
	if(handle > 0){
		if(ioctl(handle, IOCTL_ITE_DEMOD_GETRXDEVICEID, (void *)rx_device_id))
			result = ERR_CANT_FIND_USB_DEV;
	}
	else
		result = ERR_USB_INVALID_HANDLE;
	
	return result;
}

Dword DTV_EndeavourRegisterControl(
	IN	int handle,
	IN	int RW_type,
	IN	int processor_type,
	IN	Word reg_addr,
	IN	OUT Byte *data)
{
    Dword result = ERR_NO_ERROR;
	RegistersRequest request;
	
	if(processor_type == 1)
		request.processor = Processor_OFDM;
	else	
		request.processor = Processor_LINK;
	
	request.registerAddress = reg_addr;
	request.bufferLength = 1;
	
	if(handle > 0){
		switch(RW_type){
			case 0: //read
				if(ioctl(handle, IOCTL_ITE_ENDEAVOUR_READREGISTERS, (void *)&request)){
					result = ERR_CANT_FIND_USB_DEV;
				}
				else{
					*data = request.buffer[0];
					result = request.error;
				}
				break;

			case 1://write				
				memcpy(request.buffer, data, 1);
				if(ioctl(handle, IOCTL_ITE_ENDEAVOUR_WRITEREGISTERS, (void *)&request))
					result = ERR_CANT_FIND_USB_DEV;
				else
					result = request.error;

				break;
				
			default:
				result = ERR_NOT_IMPLEMENTED;
				break;
		}
	}
	else{
		result = ERR_USB_INVALID_HANDLE;
	}
	
	return result;
}

Dword DTV_NULLPacket_Pilter(
	IN int handle,
	IN	PPIDFilter PIDfilter)
{
	Dword result = ERR_NO_ERROR;
	
	if(handle > 0){
		if(ioctl(handle, IOCTL_ITE_ENDEAVOUR_SETPIDFILTER, (void *)PIDfilter))
			result = ERR_CANT_FIND_USB_DEV;
		else
			result = PIDfilter->error;
	}
	else {
		result = ERR_USB_INVALID_HANDLE;
	}

	return result;
}

Dword DTV_WriteEEPROM(
	IN	int handle,
	IN	Byte *pbyData)
{
	Dword result = ERR_NO_ERROR;
	WriteEepromValuesRequest request;
	int checksum = 0, i = 0;

	//printf("waddr = 0x%x /n", wRegAddr);

	for(i = 2; i < 256; i++) {
		checksum += pbyData[i];
	}

	pbyData[0] = (checksum >> 8);
	pbyData[1] = (checksum & 0xFF);

	//printf("checksum = %d \n", checksum);
	//printf("pbyData[0] = %d \n", pbyData[0]);
	//printf("pbyData[1] = %d \n", pbyData[1]);

	if(handle > 0) {
		request.bufferLength = 1;

		for(i = 0; i < 256; i++) {
			request.buffer[i] = pbyData[i];
		}

		if(ioctl(handle, IOCTL_ITE_DEMOD_WRITEEEPROMVALUES, (void *)&request)) {
			result = ERR_CANT_FIND_USB_DEV;
		} else {
			result = request.error;
		}
	} else {
		result = ERR_USB_INVALID_HANDLE;
	}

	return result;
}
