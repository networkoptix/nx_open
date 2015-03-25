#include "ret_chan_cmd.h"
#include "ret_chan_cmd_host.h"
#include "ret_chan_user_host.h"
#include "ret_chan_user.h"

#define Ref_BufferSize 64
#define RC_USER_DEFINE_INIT_VALUE 0


//*******************User Define Init********************
void User_init_DeviceID_Host(Word* TxID, Word* RxID)
{
#if	UART_TEST
	(*TxID) = 0x0000;
	(*RxID) = 0xFFFF;
#endif
}

void User_Host_init(RCHostInfo* deviceInfo)
{
#if	UART_TEST
	Byte i;
	Byte j;
	Rule_LineDetector* ptrRule_LineDetector = NULL;
	Rule_FieldDetector* ptrRule_FieldDetector = NULL;
	Rule_MotionDetector* ptrRule_MotionDetector = NULL;
	Rule_Counting* ptrRule_Counting = NULL;
	Rule_CellMotion* ptrRule_CellMotion = NULL;


	Byte byRefStringBuffer[Ref_BufferSize] = "NONE";
	Byte byActiveCellsBuffer[4] = {0xFF, 0xFF, 0xFF, 0xFF};

	deviceInfo->device.bIsDebug = False;

	//---------------------init_security-----------------------
	Cmd_StringSet(byRefStringBuffer, Ref_BufferSize, &deviceInfo->security.userName);
	Cmd_StringSet(byRefStringBuffer, Ref_BufferSize, &deviceInfo->security.password);
	//---------------------init_security-----------------------
	//---------------------init_NewTxDevice-----------------------
	deviceInfo->newTxDevice.deviceAddressID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->newTxDevice.IDType = RC_USER_DEFINE_INIT_VALUE;
	//---------------------init_NewTxDevice-----------------------
	//-------------init_TransmissionParameterCapabilities-----------
	deviceInfo->transmissionParameterCapabilities.bandwidthOptions = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.frequencyMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.frequencyMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.constellationOptions = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.FFTOptions = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.codeRateOptions = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.guardInterval = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.attenuationMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.attenuationMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.attenuationMin_signed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.attenuationMax_signed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.TPSCellIDMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.TPSCellIDMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.channelNumMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.channelNumMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.bandwidthStrapping = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.TVStandard = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.segmentationMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.oneSeg_Constellation = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameterCapabilities.oneSeg_CodeRate = RC_USER_DEFINE_INIT_VALUE;
	//-------------init_TransmissionParameterCapabilities-----------
	//---------------init_TransmissionParameter---------------------
	deviceInfo->transmissionParameter.bandwidth = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.frequency = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.constellation = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.FFT = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.codeRate =RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.interval = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.attenuation = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.attenuation_signed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.TPSCellID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.channelNum = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.bandwidthStrapping = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.TVStandard = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.segmentationMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.oneSeg_Constellation = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->transmissionParameter.oneSeg_CodeRate = RC_USER_DEFINE_INIT_VALUE;
	//---------------init_TransmissionParameter---------------------
	//------------------init_HwRegisterValues-----------------------
	deviceInfo->hwRegisterInfo.processor = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->hwRegisterInfo.registerAddress = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->hwRegisterInfo.valueListSize = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->hwRegisterInfo.registerValues = (Byte*) User_memory_allocate(0);
	//------------------init_HwRegisterValues-----------------------
	//-------------------init_AdvanceOptions-----------------------
	deviceInfo->advanceOptions.PTS_PCR_delayTime = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->advanceOptions.timeInterval = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->advanceOptions.skipNumber = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->advanceOptions.overFlowNumber = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->advanceOptions.overFlowSize = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->advanceOptions.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->advanceOptions.Rx_LatencyRecoverTimeInterval = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->advanceOptions.SIPSITableDuration = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->advanceOptions.frameRateAdjustment = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->advanceOptions.repeatPacketMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->advanceOptions.repeatPacketNum = 1;
	deviceInfo->advanceOptions.repeatPacketTimeInterval = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->advanceOptions.TS_TableDisable = RC_USER_DEFINE_INIT_VALUE;
	//-------------------init_AdvanceOptions-----------------------
	//-------------------init_TPSInfo-----------------------
	deviceInfo->tpsInfo.cellID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->tpsInfo.highCodeRate = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->tpsInfo.lowCodeRate = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->tpsInfo.transmissionMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->tpsInfo.constellation = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->tpsInfo.interval = RC_USER_DEFINE_INIT_VALUE;
	//-------------------init_TPSInfo-----------------------
	//-----------------------init_PSITable---------------------------
	deviceInfo->psiTable.ONID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->psiTable.NID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->psiTable.TSID = RC_USER_DEFINE_INIT_VALUE;
	for(i = 0;i<32;i++)
		deviceInfo->psiTable.networkName[i] = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->psiTable.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->psiTable.privateDataSpecifier = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->psiTable.NITVersion = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->psiTable.countryID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->psiTable.languageID = RC_USER_DEFINE_INIT_VALUE;
	//-----------------------init_PSITable---------------------------
	//--------------------init_NITLoacation-------------------------
	deviceInfo->nitLoacation.latitude = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->nitLoacation.longitude = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->nitLoacation.extentLatitude = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->nitLoacation.extentLongitude = RC_USER_DEFINE_INIT_VALUE;
	//--------------------init_NITLoacation-------------------------
	//------------------init_SdtServiceConfig------------------------
	deviceInfo->serviceConfig.serviceID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->serviceConfig.enable = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->serviceConfig.LCN = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->serviceConfig.serviceName);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->serviceConfig.provider);
	deviceInfo->serviceConfig.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->serviceConfig.IDAssignationMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->serviceConfig.ISDBT_RegionID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->serviceConfig.ISDBT_BroadcasterRegionID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->serviceConfig.ISDBT_RemoteControlKeyID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->serviceConfig.ISDBT_ServiceIDDataType_1 = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->serviceConfig.ISDBT_ServiceIDDataType_2 = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->serviceConfig.ISDBT_ServiceIDPartialReception = RC_USER_DEFINE_INIT_VALUE;
	//------------------init_SdtServiceConfig------------------------
	//-------------------init_CalibrationTable------------------------
	deviceInfo->calibrationTable.accessOption = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->calibrationTable.tableType = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->calibrationTable.tableData);
	//------------------init_CalibrationTable------------------------
	//------------------init_EITInfo------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->eitInfo.videoEncConfigToken);
	deviceInfo->eitInfo.listSize = 2;
	deviceInfo->eitInfo.eitInfoParam = (EITInfoParam*) User_memory_allocate( deviceInfo->eitInfo.listSize *sizeof(EITInfoParam));
	for( i = 0; i < deviceInfo->eitInfo.listSize; i ++)
	{
		deviceInfo->eitInfo.eitInfoParam[i].enable = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->eitInfo.eitInfoParam[i].startDate = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->eitInfo.eitInfoParam[i].startTime = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->eitInfo.eitInfoParam[i].duration = RC_USER_DEFINE_INIT_VALUE;
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->eitInfo.eitInfoParam[i].eventName);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->eitInfo.eitInfoParam[i].eventText);
	}
	//------------------init_EITInfo------------------------
	//-------------------init_DeviceCapability------------------------
	deviceInfo->deviceCapability.supportedFeatures = RC_USER_DEFINE_INIT_VALUE;
	//-------------------init_DeviceCapability------------------------
	//----------------------init_DeviceInfo---------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->manufactureInfo.manufactureName);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->manufactureInfo.modelName);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->manufactureInfo.firmwareVersion);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->manufactureInfo.serialNumber);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->manufactureInfo.hardwareId);
	//----------------------init_DeviceInfo---------------------------
	//-----------------------init_Hostname---------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->hostInfo.hostName);
	//-----------------------init_Hostname---------------------------
	//------------------init_SystemDateAndTime---------------------
	deviceInfo->systemTime.countryCode[0] = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.countryCode[1] = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.countryCode[2] = RC_USER_DEFINE_INIT_VALUE;

	deviceInfo->systemTime.countryRegionID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.daylightSavings = RC_USER_DEFINE_INIT_VALUE;

	deviceInfo->systemTime.timeZone = RC_USER_DEFINE_INIT_VALUE;

	deviceInfo->systemTime.UTCHour = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.UTCMinute = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.UTCSecond = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.UTCYear = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.UTCMonth = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.UTCDay = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.UTCMillisecond = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.timeAdjustmentMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.timeAdjustmentCriterionMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.timeAdjustmentCriterionMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->systemTime.timeSyncDuration = RC_USER_DEFINE_INIT_VALUE;
	//------------------init_SystemDateAndTime---------------------
	//-----------------------init_SystemLog--------------------------
	deviceInfo->systemLog.logType = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->systemLog.logData);
	//-----------------------init_SystemLog--------------------------
	//------------------------init_OSDInfo---------------------------
	deviceInfo->osdInfo.dateEnable = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.datePosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.dateFormat = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.timeEnable = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.timePosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.timeFormat = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.logoEnable = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.logoPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.logooption = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.detailInfoEnable = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.detailInfoPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.detailInfooption = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.textEnable = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->osdInfo.textPosition = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->osdInfo.text);
	//------------------------init_OSDInfo---------------------------
	//----------------------init_SystemDefault------------------------
	deviceInfo->systemDefault.factoryDefault = RC_USER_DEFINE_INIT_VALUE;
	//----------------------init_SystemDefault------------------------
	//-----------------init_SystemRebootMessage--------------------
	deviceInfo->systemReboot.rebootType = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->systemReboot.responseMessage);
	//-----------------init_SystemRebootMessage--------------------
	//-----------------init_SystemFirmware--------------------
	deviceInfo->systemFirmware.firmwareType = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->systemFirmware.data);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->systemFirmware.message);
	//-----------------init_SystemFirmware--------------------
	//----------------------init_DigitalInputs--------------------------
	deviceInfo->digitalInputsInfo.listSize = 2;
	deviceInfo->digitalInputsInfo.tokenList = (RCString*)User_memory_allocate( deviceInfo->digitalInputsInfo.listSize*sizeof(RCString));
	for( i = 0; i < deviceInfo->digitalInputsInfo.listSize; i ++)
	{
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->digitalInputsInfo.tokenList[i]);
	}
	//----------------------init_DigitalInputs--------------------------
	//-----------------------init_RelayOutputs------------------------
	deviceInfo->relayOutputs.listSize = 2;

	deviceInfo->relayOutputs.relayOutputsParam = (RelayOutputsParam*) User_memory_allocate( deviceInfo->relayOutputs.listSize * sizeof(RelayOutputsParam));

	for( i = 0; i < deviceInfo->relayOutputs.listSize; i ++)
	{
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->relayOutputs.relayOutputsParam[i].token);

		deviceInfo->relayOutputs.relayOutputsParam[i].mode = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->relayOutputs.relayOutputsParam[i].delayTime = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->relayOutputs.relayOutputsParam[i].idleState = RC_USER_DEFINE_INIT_VALUE;
	}
	//-----------------------init_RelayOutputs------------------------
	//------------------init_RelayOutputsSetParam-------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->relayOutputsSetParam.token);

	deviceInfo->relayOutputsSetParam.mode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->relayOutputsSetParam.delayTime = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->relayOutputsSetParam.idleState = RC_USER_DEFINE_INIT_VALUE;
	//------------------init_RelayOutputsSetParam-------------------
	//---------------------init_RelayOutputState----------------------
	deviceInfo->relayOutputState.logicalState = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->relayOutputState.token);
	//---------------------init_RelayOutputState----------------------
	//----------------------init_ImagingSettings-----------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->imageConfig.videoSourceToken);

	deviceInfo->imageConfig.backlightCompensationMode  = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.backlightCompensationLevel = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.brightness = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.colorSaturation = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.contrast = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposurePriority = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureWindowbottom  = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureWindowtop = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureWindowright = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureWindowleft = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.minExposureTime = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.maxExposureTime = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureMinGain = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureMaxGain = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureMinIris = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureMaxIris = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureTime = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureGain = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.exposureIris = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.autoFocusMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.focusDefaultSpeed = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.focusNearLimit = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.focusFarLimit = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.irCutFilterMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.sharpness = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.wideDynamicRangeMode= RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.wideDynamicRangeLevel = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.whiteBalanceMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.whiteBalanceCrGain = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.whiteBalanceCbGain = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.analogTVOutputStandard = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.imageStabilizationLevel = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.flickerControl = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.imageStabilizationMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.deNoiseMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.deNoiseStrength = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.backLightControlMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.backLightControlStrength = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfig.forcePersistence = RC_USER_DEFINE_INIT_VALUE;
	//----------------------init_ImagingSettings-----------------------
	//--------------------------init_Status----------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->focusStatusInfo.videoSourceToken);

	deviceInfo->focusStatusInfo.position = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->focusStatusInfo.moveStatus = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->focusStatusInfo.error);
	//--------------------------init_Status----------------------------
	//----------------------init_ImagingSettingsOption-----------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->imageConfigOption.videoSourceToken);

	deviceInfo->imageConfigOption.backlightCompensationMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.backlightCompensationLevelMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.backlightCompensationLevelMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.brightnessMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.brightnessMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.colorSaturationMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.colorSaturationMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.contrastMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.contrastMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposurePriority = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.minExposureTimeMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.minExposureTimeMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.maxExposureTimeMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.maxExposureTimeMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureMinGainMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureMinGainMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureMaxGainMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureMaxGainMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureMinIrisMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureMinIrisMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureMaxIrisMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureMaxIrisMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureTimeMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureTimeMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureGainMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureGainMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureIrisMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.exposureIrisMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.autoFocusMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.focusDefaultSpeedMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.focusDefaultSpeedMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.focusNearLimitMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.focusNearLimitMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.focusFarLimitMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.focusFarLimitMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.irCutFilterMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.sharpnessMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.sharpnessMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.wideDynamicRangeMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.wideDynamicRangeLevelMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.wideDynamicRangeLevelMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.whiteBalanceMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.whiteBalanceCrGainMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.whiteBalanceCrGainMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.whiteBalanceCbGainMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.whiteBalanceCbGainMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.imageStabilizationMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.imageStabilizationLevelMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.imageStabilizationLevelMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.flickerControl = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.analogTVOutputStandard = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.deNoiseMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.deNoiseStrengthMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.deNoiseStrengthMax = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.backLightControlMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.backLightControlStrengthMin = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->imageConfigOption.backLightControlStrengthMax = (float)RC_USER_DEFINE_INIT_VALUE;
	//----------------------init_ImagingSettingsOption-----------------------
	//--------------------------init_Move----------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->focusMoveInfo.videoSourceToken);

	deviceInfo->focusMoveInfo.absolutePosition = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->focusMoveInfo.absoluteSpeed = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->focusMoveInfo.relativeDistance = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->focusMoveInfo.relativeSpeed = (float)RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->focusMoveInfo.continuousSpeed = (float)RC_USER_DEFINE_INIT_VALUE;
	//--------------------------init_Move----------------------------
	//---------------------------init_Stop----------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->focusStopInfo.videoSourceToken);
	//---------------------------init_Stop----------------------------
	//---------------------------init_UserDefinedSettings----------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->userDefinedSettings.videoSourceToken);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->userDefinedSettings.uerDefinedData);
	//---------------------------init_UserDefinedSettings----------------------------
	//--------------------init_AudioEncConfig-----------------------
	deviceInfo->audioEncConfig.configListSize = 1;
	deviceInfo->audioEncConfig.configList = (AudioEncConfigParam*) User_memory_allocate( deviceInfo->audioEncConfig.configListSize * sizeof(AudioEncConfigParam));

	for( i = 0; i < deviceInfo->audioEncConfig.configListSize; i ++)
	{
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->audioEncConfig.configList[i].token);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->audioEncConfig.configList[i].name);
		deviceInfo->audioEncConfig.configList[i].useCount = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->audioEncConfig.configList[i].encoding = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->audioEncConfig.configList[i].bitrate = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->audioEncConfig.configList[i].sampleRate = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->audioEncConfig.configList[i].forcePersistence = RC_USER_DEFINE_INIT_VALUE;
	}
	//--------------------init_AudioEncConfig-----------------------
	//-------------------init_AudioSourceConfig---------------------
	deviceInfo->audioSrcConfig.configListSize = 1;
	deviceInfo->audioSrcConfig.audioSrcConfigList = (AudioSrcConfigParam*) User_memory_allocate( deviceInfo->audioSrcConfig.configListSize * sizeof(AudioSrcConfigParam));
	for( i = 0; i < deviceInfo->audioSrcConfig.configListSize; i ++)
	{
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->audioSrcConfig.audioSrcConfigList[i].name);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->audioSrcConfig.audioSrcConfigList[i].token);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->audioSrcConfig.audioSrcConfigList[i].sourceToken);
		deviceInfo->audioSrcConfig.audioSrcConfigList[i].useCount = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->audioSrcConfig.audioSrcConfigList[i].forcePersistence = RC_USER_DEFINE_INIT_VALUE;
	}
	//-------------------init_AudioSourceConfig---------------------
	//----------------------init_AudioSources------------------------
	deviceInfo->audioSources.audioSourcesListSize = 2;
	deviceInfo->audioSources.audioSourcesList = (AudioSourcesParam*) User_memory_allocate( deviceInfo->audioSources.audioSourcesListSize * sizeof(AudioSourcesParam));
	for( i = 0; i < deviceInfo->audioSources.audioSourcesListSize; i ++)
	{
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->audioSources.audioSourcesList[i].audioSourcesToken);
		deviceInfo->audioSources.audioSourcesList[i].channels = RC_USER_DEFINE_INIT_VALUE;
	}
	//----------------------init_AudioSources------------------------
	//---------------------init_GuaranteedEncs-----------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->guaranteedEncs.configurationToken);

	deviceInfo->guaranteedEncs.TotallNum = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->guaranteedEncs.JPEGNum = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->guaranteedEncs.H264Num = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->guaranteedEncs.MPEG4Num = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->guaranteedEncs.MPEG2Num = RC_USER_DEFINE_INIT_VALUE;
	//---------------------init_GuaranteedEncs-----------------------
	//-------------------------init_Profiles----------------------------
	deviceInfo->mediaProfiles.profilesListSize = 2;
	deviceInfo->mediaProfiles.mediaProfilesParam = (MediaProfilesParam*) User_memory_allocate( deviceInfo->mediaProfiles.profilesListSize * sizeof(MediaProfilesParam));

	for( i = 0; i < deviceInfo->mediaProfiles.profilesListSize; i ++)
	{
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->mediaProfiles.mediaProfilesParam[i].name);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->mediaProfiles.mediaProfilesParam[i].token);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcName);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcToken);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcSourceToken);
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcUseCount = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBounds_x = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBounds_y = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBoundsWidth = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBoundsHeight = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcRotateMode = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcRotateDegree = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcMirrorMode = RC_USER_DEFINE_INIT_VALUE;

		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcName);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcToken);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcSourceToken);
		deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcUseCount = RC_USER_DEFINE_INIT_VALUE;

		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncName);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncToken);
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncUseCount = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncEncodingMode = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncResolutionWidth = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncResolutionHeight = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncQuality = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncRateControlFrameRateLimit = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncRateControlEncodingInterval = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncRateControlBitrateLimit = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncRateControlType = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncGovLength = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncProfile = RC_USER_DEFINE_INIT_VALUE;

		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncName);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncToken);
		deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncUseCount = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncEncoding = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncBitrate = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncSampleRate = RC_USER_DEFINE_INIT_VALUE;

		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputName);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputToken);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputOutputToken);
		deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputUseCount = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputSendPrimacy = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputOutputLevel = RC_USER_DEFINE_INIT_VALUE;

		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecName);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecToken);
		deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecUseCount = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncConfigRateControlTargetBitrateLimit = RC_USER_DEFINE_INIT_VALUE;
	}
	deviceInfo->mediaProfiles.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	//-------------------------init_Profiles----------------------------
	//--------------------init_VideoEncConfig------------------------
	deviceInfo->videoEncConfig.configListSize = 1;
	deviceInfo->videoEncConfig.configList = (VideoEncConfigParam*) User_memory_allocate( deviceInfo->videoEncConfig.configListSize * sizeof(VideoEncConfigParam));
	for( i = 0; i < deviceInfo->videoEncConfig.configListSize; i ++)
	{
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoEncConfig.configList[i].name);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoEncConfig.configList[i].token);
		deviceInfo->videoEncConfig.configList[i].useCount = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].encoding = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].width = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].height = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].quality = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].frameRateLimit = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].encodingInterval = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].bitrateLimit = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].rateControlType = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].govLength = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].profile = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].forcePersistence = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].extensionFlag = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].targetBitrateLimit = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoEncConfig.configList[i].aspectRatio = RC_USER_DEFINE_INIT_VALUE;
	}
	//--------------------init_VideoEncConfig------------------------
	//--------------------init_VideoSrcConfig------------------------
	deviceInfo->videoSrcConfig.configListSize = 1;
	deviceInfo->videoSrcConfig.configList = (VideoSrcConfigParam*) User_memory_allocate( deviceInfo->videoSrcConfig.configListSize * sizeof(VideoSrcConfigParam));
	for( i = 0; i < deviceInfo->videoSrcConfig.configListSize; i ++)
	{
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoSrcConfig.configList[i].name);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoSrcConfig.configList[i].token);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoSrcConfig.configList[i].srcToken);
		deviceInfo->videoSrcConfig.configList[i].useCount = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrcConfig.configList[i].bounds_x = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrcConfig.configList[i].bounds_y = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrcConfig.configList[i].boundsWidth = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrcConfig.configList[i].boundsHeight = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrcConfig.configList[i].rotateMode = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrcConfig.configList[i].rotateDegree = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrcConfig.configList[i].mirrorMode = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrcConfig.configList[i].extensionFlag = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrcConfig.configList[i].maxFrameRate = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrcConfig.configList[i].forcePersistence = RC_USER_DEFINE_INIT_VALUE;
	}
	//--------------------init_VideoSrcConfig------------------------
	//------------------------init_VideoSrc---------------------------
	deviceInfo->videoSrc.videoSrcListSize = 1;
	deviceInfo->videoSrc.srcList = (VideoSrcParam*) User_memory_allocate( deviceInfo->videoSrc.videoSrcListSize * sizeof(VideoSrcParam));
	for( i = 0; i < deviceInfo->videoSrc.videoSrcListSize; i ++)
	{
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoSrc.srcList[i].token);
		deviceInfo->videoSrc.srcList[i].frameRate =RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrc.srcList[i].resolutionWidth = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoSrc.srcList[i].resolutionHeight = RC_USER_DEFINE_INIT_VALUE;
	}
	//------------------------init_VideoSrc---------------------------
	//------------------------init_VideoSrcConfigOptions---------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoSrcConfigOptions.videoSrcConfigToken);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoSrcConfigOptions.profileToken);

	deviceInfo->videoSrcConfigOptions.boundsRange_X_Min = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigOptions.boundsRange_X_Max = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigOptions.boundsRange_Y_Min = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigOptions.boundsRange_Y_Max = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigOptions.boundsRange_Width_Min = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigOptions.boundsRange_Width_Max = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigOptions.boundsRange_Height_Min = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigOptions.boundsRange_Heigh_Max = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize = 2;
	deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList = (RCString*) User_memory_allocate( deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize * sizeof(RCString));
	for( i = 0; i < deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize; i ++)
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList[i]);
	deviceInfo->videoSrcConfigOptions.rotateModeOptions = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigOptions.rotateDegreeMinOption = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigOptions. mirrorModeOptions = RC_USER_DEFINE_INIT_VALUE;

	//------------------------init_VideoSrcConfigOptions---------------------------
	//------------------------init_VideoEncConfigOptions---------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoEncConfigOptions.videoEncConfigToken);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoEncConfigOptions.profileToken);

	deviceInfo->videoEncConfigOptions.encodingOption = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize = 2;
	deviceInfo->videoEncConfigOptions.resolutionsAvailableList = ( ResolutionsAvailableList* ) User_memory_allocate( deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize * sizeof(ResolutionsAvailableList));
	for( i = 0; i < deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize; i ++)
	{
		deviceInfo->videoEncConfigOptions.resolutionsAvailableList[i].width = RC_USER_DEFINE_INIT_VALUE + i;;
		deviceInfo->videoEncConfigOptions.resolutionsAvailableList[i].height = RC_USER_DEFINE_INIT_VALUE + i;;
	}
	deviceInfo->videoEncConfigOptions.qualityMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.qualityMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.frameRateMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.frameRateMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.encodingIntervalMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.encodingIntervalMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.bitrateRangeMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.bitrateRangeMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.rateControlTypeOptions = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.govLengthRangeMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.govLengthRangeMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.profileOptions = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.targetBitrateRangeMin = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.targetBitrateRangeMax = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigOptions.aspectRatioAvailableListSize = 2;
	deviceInfo->videoEncConfigOptions.aspectRatioList = (Word*) User_memory_allocate( deviceInfo->videoEncConfigOptions.aspectRatioAvailableListSize * sizeof(Word));
	for( i = 0; i < deviceInfo->videoEncConfigOptions.aspectRatioAvailableListSize; i ++ )
	{
		deviceInfo->videoEncConfigOptions.aspectRatioList[i] = RC_USER_DEFINE_INIT_VALUE;
	}

	//------------------------init_VideoEncConfigOptions---------------------------
	//------------------------init_AudioSrcConfigOptions---------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->audioSrcConfigOptions.audioSrcConfigToken);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->audioSrcConfigOptions.profileToken);
	deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize = 2;
	deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList = (RCString*) User_memory_allocate(deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize * sizeof(RCString));
	for( i = 0; i < deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize; i ++)
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList[i]);
	//------------------------init_AudioSrcConfigOptions---------------------------
	//------------------------init_AudioEncConfigOptions---------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->audioEncConfigOptions.audioEncConfigToken);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->audioEncConfigOptions.profileToken);

	deviceInfo->audioEncConfigOptions.configListSize = 2;
	deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam = ( AudioEncConfigOptionsParam* ) User_memory_allocate( deviceInfo->audioEncConfigOptions.configListSize * sizeof(AudioEncConfigOptionsParam));
	for( i = 0; i < deviceInfo->audioEncConfigOptions.configListSize; i ++ )
	{
		deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].encodingOption = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].bitrateRangeMin = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].bitrateRangeMax = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableListSize = 2;
		deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableList = (Word*) User_memory_allocate( deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableListSize * sizeof(Word));
		for( j = 0; j < deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableListSize; j ++)
		{
			deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableList[j] = RC_USER_DEFINE_INIT_VALUE;
		}
	}
	//------------------------init_AudioEncConfigOptions---------------------------
	//----------------init_AudioEncConfigSetParam------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->audioEncConfigSetParam.name);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->audioEncConfigSetParam.token);

	deviceInfo->audioEncConfigSetParam.useCount =RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->audioEncConfigSetParam.encoding = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->audioEncConfigSetParam.bitrate = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->audioEncConfigSetParam.sampleRate = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->audioEncConfigSetParam.forcePersistence = RC_USER_DEFINE_INIT_VALUE;
	//----------------init_AudioEncConfigSetParam------------------
	//----------------init_AudioSrcConfigSetParam------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->audioSrcConfigSetParam.name);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->audioSrcConfigSetParam.token);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->audioSrcConfigSetParam.sourceToken);
	deviceInfo->audioSrcConfigSetParam.useCount = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->audioSrcConfigSetParam.forcePersistence = RC_USER_DEFINE_INIT_VALUE;
	//----------------init_AudioSrcConfigSetParam------------------
	//----------------init_VideoEncConfigSetParam------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoEncConfigSetParam.name);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoEncConfigSetParam.token);
	deviceInfo->videoEncConfigSetParam.useCount = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.encoding = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.width = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.height = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.quality = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.frameRateLimit = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.encodingInterval = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.bitrateLimit = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.rateControlType = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.govLength = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.profile = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.forcePersistence = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.targetBitrateLimit = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoEncConfigSetParam.aspectRatio = RC_USER_DEFINE_INIT_VALUE;
	//----------------init_VideoEncConfigSetParam------------------
	//----------------init_VideoSrcConfigSetParam------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoSrcConfigSetParam.name);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoSrcConfigSetParam.token);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoSrcConfigSetParam.srcToken);

	deviceInfo->videoSrcConfigSetParam.useCount = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigSetParam.bounds_x = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigSetParam.bounds_y = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigSetParam.boundsWidth = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigSetParam.boundsHeight = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigSetParam.rotateMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigSetParam.rotateDegree = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigSetParam.mirrorMode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigSetParam.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigSetParam.maxFrameRate = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoSrcConfigSetParam.forcePersistence = RC_USER_DEFINE_INIT_VALUE;
	//----------------init_VideoSrcConfigSetParam------------------
	//----------------init_VideoOSDConfig------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoOSDConfig.videoSrcToken);
	deviceInfo->videoOSDConfig.dateEnable = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.datePosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.dateFormat = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.timeEnable = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.timePosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.timeFormat = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.logoEnable = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.logoPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.logooption = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.detailInfoEnable = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.detailInfoPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.detailInfoOption = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.textEnable = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->videoOSDConfig.textPosition = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoOSDConfig.text);
	//----------------init_VideoOSDConfig------------------
	//------------------------init_VideoPrivateArea--------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoPrivateArea.videoSrcToken);
	deviceInfo->videoPrivateArea.polygonListSize = 2;
	deviceInfo->videoPrivateArea.privateAreaPolygon = ( PrivateAreaPolygon* ) User_memory_allocate( deviceInfo->videoPrivateArea.polygonListSize * sizeof(PrivateAreaPolygon));
	for( i = 0; i < deviceInfo->videoPrivateArea.polygonListSize; i ++)
	{
		deviceInfo->videoPrivateArea.privateAreaPolygon[i].polygon_x = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->videoPrivateArea.privateAreaPolygon[i].polygon_y = RC_USER_DEFINE_INIT_VALUE;
	}
	deviceInfo->videoPrivateArea.privateAreaEnable = RC_USER_DEFINE_INIT_VALUE;
	//------------------------init_VideoPrivateArea--------------------------
	//------------------------init_SyncPoint--------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->syncPoint.profileToken);
	//------------------------init_SyncPoint--------------------------
	//------------------------init_VideoSrcControl--------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->videoSrcControl.videoSrcToken);
	deviceInfo->videoSrcControl.controlCommand =  RC_USER_DEFINE_INIT_VALUE;
	//------------------------init_VideoSrcControl--------------------------
	//------------------------init_PTZConfig--------------------------
	deviceInfo->ptzConfig.PTZConfigListSize = 2;
	deviceInfo->ptzConfig.PTZConfigList = (PTZConfigParam*) User_memory_allocate( deviceInfo->ptzConfig.PTZConfigListSize * sizeof(PTZConfigParam));
	for(i = 0; i < deviceInfo->ptzConfig.PTZConfigListSize; i ++)
	{
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzConfig.PTZConfigList[i].name);
		deviceInfo->ptzConfig.PTZConfigList[i].useCount = RC_USER_DEFINE_INIT_VALUE;
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzConfig.PTZConfigList[i].token);
		deviceInfo->ptzConfig.PTZConfigList[i].defaultPanSpeed = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzConfig.PTZConfigList[i].defaultTiltSpeed = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzConfig.PTZConfigList[i].defaultZoomSpeed = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzConfig.PTZConfigList[i].defaultTimeout = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzConfig.PTZConfigList[i].panLimitMin = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzConfig.PTZConfigList[i].panLimitMax = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzConfig.PTZConfigList[i].tiltLimitMin = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzConfig.PTZConfigList[i].tiltLimitMax = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzConfig.PTZConfigList[i].zoomLimitMin = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzConfig.PTZConfigList[i].zoomLimitMax = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzConfig.PTZConfigList[i].eFlipMode = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzConfig.PTZConfigList[i].reverseMode = RC_USER_DEFINE_INIT_VALUE;
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken);
	}
	deviceInfo->ptzConfig.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	//------------------------init_PTZConfig--------------------------
	//------------------------init_PTZStatus--------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzStatus.token);
	deviceInfo->ptzStatus.panPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzStatus.tiltPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzStatus.zoomPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzStatus.panTiltMoveStatus  = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzStatus.zoomMoveStatus = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzStatus.error);
	deviceInfo->ptzStatus.UTCHour = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzStatus.UTCMinute = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzStatus.UTCSecond = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzStatus.UTCYear = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzStatus.UTCMonth = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzStatus.UTCDay = RC_USER_DEFINE_INIT_VALUE;
	//------------------------init_PTZStatus--------------------------
	//-----------------------init_PTZPresetsGet--------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzPresetsGet.token);
	deviceInfo->ptzPresetsGet.configListSize = 1;
	deviceInfo->ptzPresetsGet.configList = (PTZPresetsConfig*) User_memory_allocate( deviceInfo->ptzPresetsGet.configListSize * sizeof(PTZPresetsConfig));
	for( i = 0; i < deviceInfo->ptzPresetsGet.configListSize; i ++)
	{
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzPresetsGet.configList[i].presetName);
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzPresetsGet.configList[i].presetToken);
		deviceInfo->ptzPresetsGet.configList[i].panPosition = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzPresetsGet.configList[i].tiltPosition = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzPresetsGet.configList[i].zoomPosition = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzPresetsGet.configList[i].panSpeed = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzPresetsGet.configList[i].tiltSpeed = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->ptzPresetsGet.configList[i].zoomSpeed = RC_USER_DEFINE_INIT_VALUE;
	}
	//-----------------------init_PTZPresetsGet--------------------------
	//-----------------------init_PTZPresetsParam--------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzPresetsSet.token);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzPresetsSet.presetName);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzPresetsSet.presetToken);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzPresetsSet.presetToken_set);
	//-----------------------init_PTZPresetsParam--------------------------
	//-----------------------init_PTZGotoParam----------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzGotoParam.token);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzGotoParam.presetToken);
	deviceInfo->ptzGotoParam.panSpeed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzGotoParam.tiltSpeed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzGotoParam.zoomSpeed = RC_USER_DEFINE_INIT_VALUE;
	//-----------------------init_PTZGotoParam----------------------
	//---------------------init_PTZRemoveParam---------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzRemoveParam.token);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzRemoveParam.presetToken);
	//---------------------init_PTZRemoveParam---------------------
	//---------------------init_PTZAbsoluteMove---------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzAbsoluteMove.token);
	deviceInfo->ptzAbsoluteMove.panPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzAbsoluteMove.tiltPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzAbsoluteMove.zoomPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzAbsoluteMove.panSpeed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzAbsoluteMove.tiltSpeed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzAbsoluteMove.zoomSpeed = RC_USER_DEFINE_INIT_VALUE;
	//---------------------init_PTZAbsoluteMove---------------------
	//----------------------init_PTZRelativeMove---------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzRelativeMove.token);
	deviceInfo->ptzRelativeMove.panTranslation = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzRelativeMove.tiltTranslation = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzRelativeMove.zoomTranslation = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzRelativeMove.panSpeed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzRelativeMove.tiltSpeed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzRelativeMove.zoomSpeed = RC_USER_DEFINE_INIT_VALUE;
	//----------------------init_PTZRelativeMove---------------------
	//--------------------init_PTZContinuousMove-------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzContinuousMove.token);
	deviceInfo->ptzContinuousMove.panVelocity = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzContinuousMove.tiltVelocity = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzContinuousMove.zoomVelocity = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzContinuousMove.timeout = RC_USER_DEFINE_INIT_VALUE;
	//--------------------init_PTZContinuousMove-------------------
	//---------------------init_PTZHomePosition---------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzHomePosition.token);
	deviceInfo->ptzHomePosition.panSpeed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzHomePosition.tiltSpeed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzHomePosition.zoomSpeed = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzHomePosition.panPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzHomePosition.tiltPosition = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->ptzHomePosition.zoomPosition = RC_USER_DEFINE_INIT_VALUE;
	//---------------------init_PTZHomePosition---------------------
	//--------------------------init_PTZStop-------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ptzStop.token);
	deviceInfo->ptzStop.panTiltZoom = RC_USER_DEFINE_INIT_VALUE;
	//--------------------------init_PTZStop-------------------------
	//----------------------init_SupportedRules-----------------------
	deviceInfo->supportedRules.supportListSize = 5;
	deviceInfo->supportedRules.ruleType = (Byte*) User_memory_allocate( deviceInfo->supportedRules.supportListSize * sizeof(Byte));
	for( i = 0; i < deviceInfo->supportedRules.supportListSize; i ++)
		deviceInfo->supportedRules.ruleType[i] = i + 0x10;
	//----------------------init_SupportedRules-----------------------
	//------------------------init_DeleteRule--------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->deleteRule.ruleToken);
	//------------------------init_DeleteRule--------------------------
	//-------------------------init_totalRule--------------------------
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->totalRule.ruleName);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->totalRule.ruleToken);
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->totalRule.videoSrcToken);
	deviceInfo->totalRule.type = 0;
	deviceInfo->totalRule.rule = NULL;
	//-------------------------init_totalRule---------------------------
	//--------------------------init_ruleList----------------------------
	deviceInfo->ruleList.maxListSize = 10;
	deviceInfo->ruleList.ruleListSize = 5;
	deviceInfo->ruleList.totalRuleList = (TotalRule* )User_memory_allocate(deviceInfo->ruleList.maxListSize * sizeof(TotalRule));
	for( i = 0; i < deviceInfo->ruleList.maxListSize; i ++)
	{

		if( i < 5)
		{
			deviceInfo->ruleList.totalRuleList[i].type = i + 0x10;
		}
		else
		{
			deviceInfo->ruleList.totalRuleList[i].type = 0;
		}

		if((deviceInfo->ruleList.totalRuleList[i].type >= 0x10) && (deviceInfo->ruleList.totalRuleList[i].type <= 0x14))
		{
			Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ruleList.totalRuleList[i].ruleName);
			Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ruleList.totalRuleList[i].ruleToken);
			Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->ruleList.totalRuleList[i].videoSrcToken);
		}else{
			continue;
		}
		if(deviceInfo->ruleList.totalRuleList[i].type == 0x10 )
		{
			deviceInfo->ruleList.totalRuleList[i].rule = (Rule_LineDetector*) User_memory_allocate(sizeof(Rule_LineDetector));
			ptrRule_LineDetector = (Rule_LineDetector*) deviceInfo->ruleList.totalRuleList[i].rule;

			ptrRule_LineDetector->direction = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_LineDetector->polygonListSize = 2;
			ptrRule_LineDetector->detectPolygon = (DetectPolygon*) User_memory_allocate( ptrRule_LineDetector->polygonListSize * sizeof(DetectPolygon));

			for(j = 0; j < ptrRule_LineDetector->polygonListSize; j ++)
			{
				ptrRule_LineDetector->detectPolygon[j].polygon_x = RC_USER_DEFINE_INIT_VALUE;
				ptrRule_LineDetector->detectPolygon[j].polygon_y = RC_USER_DEFINE_INIT_VALUE;
			}
			ptrRule_LineDetector->metadataStreamSwitch = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_LineDetector = NULL;
		}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x11 )
		{
			deviceInfo->ruleList.totalRuleList[i].rule = (Rule_FieldDetector*) User_memory_allocate(sizeof(Rule_FieldDetector));
			ptrRule_FieldDetector = (Rule_FieldDetector*) deviceInfo->ruleList.totalRuleList[i].rule;

			ptrRule_FieldDetector->polygonListSize = 2;
			ptrRule_FieldDetector->detectPolygon = (DetectPolygon*) User_memory_allocate(ptrRule_FieldDetector->polygonListSize * sizeof(DetectPolygon));
			for(j = 0; j < ptrRule_FieldDetector->polygonListSize; j ++)
			{
				ptrRule_FieldDetector->detectPolygon[j].polygon_x = RC_USER_DEFINE_INIT_VALUE;
				ptrRule_FieldDetector->detectPolygon[j].polygon_y = RC_USER_DEFINE_INIT_VALUE;
			}
			ptrRule_FieldDetector->metadataStreamSwitch = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_FieldDetector = NULL;
		}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x12 )
		{
			deviceInfo->ruleList.totalRuleList[i].rule = (Rule_MotionDetector*) User_memory_allocate(sizeof(Rule_MotionDetector));
			ptrRule_MotionDetector = (Rule_MotionDetector*) deviceInfo->ruleList.totalRuleList[i].rule;

			ptrRule_MotionDetector->motionExpression = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_MotionDetector->polygonListSize = 2 ;
			ptrRule_MotionDetector->detectPolygon = (DetectPolygon*) User_memory_allocate(ptrRule_MotionDetector->polygonListSize * sizeof(DetectPolygon));

			for( j = 0; j < ptrRule_MotionDetector->polygonListSize; j++ )
			{
				ptrRule_MotionDetector->detectPolygon[j].polygon_x = RC_USER_DEFINE_INIT_VALUE;
				ptrRule_MotionDetector->detectPolygon[j].polygon_y = RC_USER_DEFINE_INIT_VALUE;
			}
			ptrRule_MotionDetector->metadataStreamSwitch = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_MotionDetector = NULL;
		}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x13 )
		{

			deviceInfo->ruleList.totalRuleList[i].rule = (Rule_Counting*) User_memory_allocate(sizeof(Rule_Counting));
			ptrRule_Counting = (Rule_Counting*) deviceInfo->ruleList.totalRuleList[i].rule;

			ptrRule_Counting->reportTimeInterval = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_Counting->resetTimeInterval = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_Counting->direction = RC_USER_DEFINE_INIT_VALUE;

			ptrRule_Counting->polygonListSize = 2;
			ptrRule_Counting->detectPolygon = (DetectPolygon*) User_memory_allocate(ptrRule_Counting->polygonListSize * sizeof(DetectPolygon));

			for( j = 0; j < ptrRule_Counting->polygonListSize; j++ )
			{
				ptrRule_Counting->detectPolygon[j].polygon_x = RC_USER_DEFINE_INIT_VALUE;
				ptrRule_Counting->detectPolygon[j].polygon_y = RC_USER_DEFINE_INIT_VALUE;
			}
			ptrRule_Counting->metadataStreamSwitch = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_Counting = NULL;
		}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x14 )
		{
			deviceInfo->ruleList.totalRuleList[i].rule = (Rule_CellMotion*) User_memory_allocate(sizeof(Rule_CellMotion));
			ptrRule_CellMotion = (Rule_CellMotion*) deviceInfo->ruleList.totalRuleList[i].rule;

			ptrRule_CellMotion->minCount = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_CellMotion->alarmOnDelay = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_CellMotion->alarmOffDelay = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_CellMotion->activeCellsSize = RC_USER_DEFINE_INIT_VALUE;
			Cmd_StringSet( byActiveCellsBuffer, sizeof(byActiveCellsBuffer),  &ptrRule_CellMotion->activeCells);
			ptrRule_CellMotion->sensitivity = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_CellMotion->layoutBounds_x = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_CellMotion->layoutBounds_y = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_CellMotion->layoutBounds_width = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_CellMotion->layoutBounds_height = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_CellMotion->layoutColumns = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_CellMotion->layoutRows = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_CellMotion->metadataStreamSwitch = RC_USER_DEFINE_INIT_VALUE;
			ptrRule_CellMotion = NULL;
		}
	}
	//--------------------------init_ruleList----------------------------
	//--------------------init_MetaDataStream----------------------
	deviceInfo->metadataStreamInfo.version = RETURN_CHANNEL_API_VERSION;
	deviceInfo->metadataStreamInfo.type = 0x01;

	deviceInfo->metadataStreamInfo.metadata_Device.deviceVendorID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->metadataStreamInfo.metadata_Device.deviceModelID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->metadataStreamInfo.metadata_Device.HWVersionCode = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->metadataStreamInfo.metadata_Device.SWVersionCode = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->metadataStreamInfo.metadata_Device.textInformation);
	deviceInfo->metadataStreamInfo.metadata_Device.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->metadataStreamInfo.metadata_Device.baudRate = RETURN_CHANNEL_BAUDRATE;

	deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize = 1;
	deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList = (Metadata_StreamParam*) User_memory_allocate( deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize * sizeof(Metadata_StreamParam));
	for( i = 0; i < deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize; i ++)
	{
		deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoPID = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoEncodingType = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoResolutionWidth = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoResolutionHeight = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoFrameRate = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoBitrate = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].audioPID = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].audioEncodingType = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].audioBitrate = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].audioSampleRate = RC_USER_DEFINE_INIT_VALUE;
		deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].PCRPID = RC_USER_DEFINE_INIT_VALUE;
		Cmd_StringSet( byRefStringBuffer, Ref_BufferSize,  &deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken);
	}
	deviceInfo->metadataStreamInfo.metadata_Stream.extensionFlag = RC_USER_DEFINE_INIT_VALUE;

	//******************init_metadata_Event******************
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);
	deviceInfo->metadataStreamInfo.metadata_Event.objectID = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->metadataStreamInfo.metadata_Event.UTCHour = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->metadataStreamInfo.metadata_Event.UTCMinute = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->metadataStreamInfo.metadata_Event.UTCSecond = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->metadataStreamInfo.metadata_Event.UTCYear = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->metadataStreamInfo.metadata_Event.UTCMonth = RC_USER_DEFINE_INIT_VALUE;
	deviceInfo->metadataStreamInfo.metadata_Event.UTCDay = RC_USER_DEFINE_INIT_VALUE;
	Cmd_StringSet( byRefStringBuffer, Ref_BufferSize, &deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken);
	//******************init_metadata_Event******************
	//--------------------init_MetaDataStream----------------------
	//--------------------init_MetadataSettings----------------------
	deviceInfo->metadataSettings.deviceInfoEnable = 1;
	deviceInfo->metadataSettings.deviceInfoPeriod = 1000;
	deviceInfo->metadataSettings.streamInfoEnable = 1;
	deviceInfo->metadataSettings.streamInfoPeriod = 1000;
	deviceInfo->metadataSettings.motionDetectorEnable = 1;
	deviceInfo->metadataSettings.motionDetectorPeriod = 1000;
	//--------------------init_MetadataSettings----------------------
#endif
}
//*******************User Define Init********************

//*******************User Define UnInit********************
void User_Host_uninit(RCHostInfo* deviceInfo)
{
#if	UART_TEST
	Byte i = 0;
	Rule_LineDetector* ptrRule_LineDetector = NULL;
	Rule_FieldDetector* ptrRule_FieldDetector = NULL;
	Rule_MotionDetector* ptrRule_MotionDetector = NULL;
	Rule_Counting* ptrRule_Counting = NULL;
	Rule_CellMotion* ptrRule_CellMotion = NULL;

	//---------------------uninit_security-----------------------
	Cmd_StringClear( &deviceInfo->security.userName);
	Cmd_StringClear( &deviceInfo->security.password);
	//---------------------uninit_security-----------------------
	//------------------uninit_HwRegisterValues-----------------------
	User_memory_free( deviceInfo->hwRegisterInfo.registerValues);
	//------------------uninit_HwRegisterValues-----------------------
	//------------------uninit_SdtServiceConfig------------------------
	Cmd_StringClear(&deviceInfo->serviceConfig.serviceName);
	Cmd_StringClear(&deviceInfo->serviceConfig.provider);
	//------------------uninit_SdtServiceConfig------------------------
    //-------------------uninit_CalibrationTable------------------------
    Cmd_StringClear(&deviceInfo->calibrationTable.tableData);
    //------------------init_CalibrationTable------------------------
	//-----------------uninit_EITInfo-----------------
	Cmd_StringClear(&deviceInfo->eitInfo.videoEncConfigToken);
	for( i = 0; i < deviceInfo->eitInfo.listSize; i ++)
	{
		Cmd_StringClear(&deviceInfo->eitInfo.eitInfoParam[i].eventName);
		Cmd_StringClear(&deviceInfo->eitInfo.eitInfoParam[i].eventText);
	}
	User_memory_free(deviceInfo->eitInfo.eitInfoParam);
	deviceInfo->eitInfo.eitInfoParam = NULL;
	deviceInfo->eitInfo.listSize = 0;
	//-----------------uninit_EITInfo-----------------
	//-----------------uninit_DeviceInfo-----------------
	Cmd_StringClear(&deviceInfo->manufactureInfo.manufactureName);
	Cmd_StringClear(&deviceInfo->manufactureInfo.modelName);
	Cmd_StringClear(&deviceInfo->manufactureInfo.firmwareVersion);
	Cmd_StringClear(&deviceInfo->manufactureInfo.serialNumber);
	Cmd_StringClear(&deviceInfo->manufactureInfo.hardwareId);
	//-----------------uninit_DeviceInfo-----------------
	//-----------------uninit_Hostname------------------
	Cmd_StringClear(&deviceInfo->hostInfo.hostName);
	//-----------------uninit_Hostname------------------
	//-----------------uninit_SystemLog-----------------
	Cmd_StringClear(&deviceInfo->systemLog.logData);
	//-----------------uninit_SystemLog-----------------
	//------------------uninit_OSDInfo------------------
	Cmd_StringClear(&deviceInfo->osdInfo.text);
	//------------------uninit_OSDInfo------------------
	//-----------uninit_SystemRebootMessage----------
	Cmd_StringClear(&deviceInfo->systemReboot.responseMessage);
	//-----------uninit_SystemRebootMessage----------
	//-----------------uninit_SystemFirmware--------------------
	Cmd_StringClear(&deviceInfo->systemFirmware.data);
	Cmd_StringClear(&deviceInfo->systemFirmware.message);
	//-----------------uninit_SystemFirmware--------------------
	//-----------------uninit_DigitalInputs----------------
	for( i = 0; i < deviceInfo->digitalInputsInfo.listSize; i ++)
	{
		Cmd_StringClear(&deviceInfo->digitalInputsInfo.tokenList[i]);
	}
	User_memory_free(deviceInfo->digitalInputsInfo.tokenList);
	deviceInfo->digitalInputsInfo.tokenList = NULL;
	deviceInfo->digitalInputsInfo.listSize = 0;
	//-----------------uninit_DigitalInputs----------------
	//-----------------uninit_RelayOutputs---------------
	for( i = 0; i < deviceInfo->relayOutputs.listSize; i ++)
	{
		Cmd_StringClear( &deviceInfo->relayOutputs.relayOutputsParam[i].token);
	}
	User_memory_free(deviceInfo->relayOutputs.relayOutputsParam);
	deviceInfo->relayOutputs.relayOutputsParam = NULL;
	deviceInfo->relayOutputs.listSize = 0;
	//-----------------uninit_RelayOutputs---------------
	//------------uninit_RelayOutputsSetParam----------
	Cmd_StringClear( &deviceInfo->relayOutputsSetParam.token);
	//------------uninit_RelayOutputsSetParam----------
	//---------------uninit_RelayOutputState-------------
	Cmd_StringClear( &deviceInfo->relayOutputState.token);
	//---------------uninit_RelayOutputState-------------
	//---------------uninit_ImagingSettings---------------
	Cmd_StringClear( &deviceInfo->imageConfig.videoSourceToken);
	//---------------uninit_ImagingSettings---------------
	//--------------------uninit_Status--------------------
	Cmd_StringClear( &deviceInfo->focusStatusInfo.videoSourceToken);
	Cmd_StringClear( &deviceInfo->focusStatusInfo.error);
	//--------------------uninit_Status--------------------
	//--------------------uninit_ImagingSettingsOption--------------------
	Cmd_StringClear( &deviceInfo->imageConfigOption.videoSourceToken);
	//--------------------uninit_ImagingSettingsOption--------------------
	//--------------------uninit_Move--------------------
	Cmd_StringClear( &deviceInfo->focusMoveInfo.videoSourceToken);
	//--------------------uninit_Move--------------------
	//---------------------------uninit_Stop----------------------------
	Cmd_StringClear( &deviceInfo->focusStopInfo.videoSourceToken);
	//---------------------------uninit_Stop----------------------------
	//---------------------------uninit_UserDefinedSettings----------------------------
	Cmd_StringClear( &deviceInfo->userDefinedSettings.videoSourceToken);
	Cmd_StringClear( &deviceInfo->userDefinedSettings.uerDefinedData);
	//---------------------------uninit_UserDefinedSettings----------------------------
	//--------------uninit_AudioEncConfig---------------
	for( i = 0; i < deviceInfo->audioEncConfig.configListSize; i ++)
	{
		Cmd_StringClear( &deviceInfo->audioEncConfig.configList[i].token);
		Cmd_StringClear( &deviceInfo->audioEncConfig.configList[i].name);
	}
	User_memory_free(deviceInfo->audioEncConfig.configList);
	deviceInfo->audioEncConfig.configList = NULL;
	deviceInfo->audioEncConfig.configListSize = 0;
	//--------------uninit_AudioEncConfig---------------
	//------------uninit_AudioSourceConfig--------------
	for( i = 0; i < deviceInfo->audioSrcConfig.configListSize; i ++)
	{
		Cmd_StringClear( &deviceInfo->audioSrcConfig.audioSrcConfigList[i].name);
		Cmd_StringClear( &deviceInfo->audioSrcConfig.audioSrcConfigList[i].token);
		Cmd_StringClear( &deviceInfo->audioSrcConfig.audioSrcConfigList[i].sourceToken);
	}
	User_memory_free(deviceInfo->audioSrcConfig.audioSrcConfigList);
	deviceInfo->audioSrcConfig.audioSrcConfigList = NULL;
	deviceInfo->audioSrcConfig.configListSize = 0;
	//------------uninit_AudioSourceConfig--------------
	//---------------uninit_AudioSources-----------------
	for( i = 0; i < deviceInfo->audioSources.audioSourcesListSize; i ++)
	{
		Cmd_StringClear( &deviceInfo->audioSources.audioSourcesList[i].audioSourcesToken);
	}
	User_memory_free(deviceInfo->audioSources.audioSourcesList);
	deviceInfo->audioSources.audioSourcesList = NULL;
	deviceInfo->audioSources.audioSourcesListSize = 0;
	//---------------uninit_AudioSources-----------------
	//---------------uninit_GuaranteedEncs-----------------
	Cmd_StringClear( &deviceInfo->guaranteedEncs.configurationToken);
	//---------------uninit_GuaranteedEncs-----------------
	//--------------------uninit_Profiles---------------------
	for( i = 0; i < deviceInfo->mediaProfiles.profilesListSize; i ++)
	{
		Cmd_StringClear( &deviceInfo->mediaProfiles.mediaProfilesParam[i].name);
		Cmd_StringClear( &deviceInfo->mediaProfiles.mediaProfilesParam[i].token);
		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcName);
		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcToken);
		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcSourceToken);

		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcName);
		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcToken);
		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcSourceToken);

		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncName);
		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncToken);

		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncName);
		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncToken);

		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputName);
		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputToken);
		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputOutputToken);

		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecName);
		Cmd_StringClear(  &deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecToken);
	}
	User_memory_free(deviceInfo->mediaProfiles.mediaProfilesParam);
	deviceInfo->mediaProfiles.mediaProfilesParam = NULL;
	deviceInfo->mediaProfiles.profilesListSize = 0;
	//--------------------uninit_Profiles---------------------
	//---------------uninit_VideoEncConfig-----------------
	for( i = 0; i < deviceInfo->videoEncConfig.configListSize; i ++)
	{
		Cmd_StringClear(  &deviceInfo->videoEncConfig.configList[i].name);
		Cmd_StringClear(  &deviceInfo->videoEncConfig.configList[i].token);
	}
	User_memory_free(deviceInfo->videoEncConfig.configList);
	deviceInfo->videoEncConfig.configList = NULL;
	deviceInfo->videoEncConfig.configListSize = 0;
	//---------------uninit_VideoEncConfig-----------------
	//---------------uninit_VideoSrcConfig-----------------
	for( i = 0; i < deviceInfo->videoSrcConfig.configListSize; i ++)
	{
		Cmd_StringClear(  &deviceInfo->videoSrcConfig.configList[i].name);
		Cmd_StringClear(  &deviceInfo->videoSrcConfig.configList[i].token);
		Cmd_StringClear(  &deviceInfo->videoSrcConfig.configList[i].srcToken);
	}
	User_memory_free(deviceInfo->videoSrcConfig.configList);
	deviceInfo->videoSrcConfig.configList = NULL;
	deviceInfo->videoSrcConfig.configListSize = 0;
	//---------------uninit_VideoSrcConfig-----------------
	//------------------uninit_VideoSrc---------------------
	for( i = 0; i < deviceInfo->videoSrc.videoSrcListSize; i ++)
	{
		Cmd_StringClear(  &deviceInfo->videoSrc.srcList[i].token);
	}
	User_memory_free(deviceInfo->videoSrc.srcList);
	deviceInfo->videoSrc.srcList = NULL;
	deviceInfo->videoSrc.videoSrcListSize = 0;
	//------------------uninit_VideoSrc---------------------
	//------------------------uninit_VideoSrcConfigOptions---------------------------
	Cmd_StringClear(  &deviceInfo->videoSrcConfigOptions.videoSrcConfigToken);
	Cmd_StringClear(  &deviceInfo->videoSrcConfigOptions.profileToken);
	for( i = 0; i < deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize; i ++)
		Cmd_StringClear( &deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList[i]);
	User_memory_free(deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList);
	deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList = NULL;
	deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize = 0;
	//------------------------uninit_VideoSrcConfigOptions---------------------------
	//------------------------uninit_VideoEncConfigOptions---------------------------
	Cmd_StringClear(  &deviceInfo->videoEncConfigOptions.videoEncConfigToken);
	Cmd_StringClear(  &deviceInfo->videoEncConfigOptions.profileToken);
	User_memory_free( deviceInfo->videoEncConfigOptions.resolutionsAvailableList);
	User_memory_free( deviceInfo->videoEncConfigOptions.aspectRatioList);
	deviceInfo->videoEncConfigOptions.resolutionsAvailableList = NULL;
	deviceInfo->videoEncConfigOptions.aspectRatioList = NULL;
	deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize = 0;
	deviceInfo->videoEncConfigOptions.aspectRatioAvailableListSize = 0;
	//------------------------uninit_VideoEncConfigOptions---------------------------
	//------------------------uninit_AudioSrcConfigOptions---------------------------
	Cmd_StringClear(  &deviceInfo->audioSrcConfigOptions.audioSrcConfigToken);
	Cmd_StringClear(  &deviceInfo->audioSrcConfigOptions.profileToken);
    for( i = 0; i < deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize; i ++)
        Cmd_StringClear(&deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList[i]);
	User_memory_free(deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList);
	deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList = NULL;
	deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize = 0;
	//------------------------uninit_AudioSrcConfigOptions---------------------------
	//------------------------uninit_AudioEncConfigOptions---------------------------
	Cmd_StringClear(  &deviceInfo->audioEncConfigOptions.audioEncConfigToken);
	Cmd_StringClear(  &deviceInfo->audioEncConfigOptions.profileToken);
	for( i = 0; i < deviceInfo->audioEncConfigOptions.configListSize; i ++ )
		User_memory_free(deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableList);
	User_memory_free(deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam);
	deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam = NULL;
	deviceInfo->audioEncConfigOptions.configListSize = 0;
	//------------------------uninit_AudioEncConfigOptions---------------------------
	//----------uninit_AudioEncConfigSetParam------------
	Cmd_StringClear(  &deviceInfo->audioEncConfigSetParam.name);
	Cmd_StringClear(  &deviceInfo->audioEncConfigSetParam.token);
	//----------uninit_AudioEncConfigSetParam------------
	//----------uninit_AudioSrcConfigSetParam------------
	Cmd_StringClear(  &deviceInfo->audioSrcConfigSetParam.name);
	Cmd_StringClear(  &deviceInfo->audioSrcConfigSetParam.token);
	Cmd_StringClear(  &deviceInfo->audioSrcConfigSetParam.sourceToken);
	//----------uninit_AudioSrcConfigSetParam------------
	//----------uninit_VideoEncConfigSetParam------------
	Cmd_StringClear(   &deviceInfo->videoEncConfigSetParam.name);
	Cmd_StringClear(  &deviceInfo->videoEncConfigSetParam.token);
	//----------uninit_VideoEncConfigSetParam------------
	//----------uninit_VideoSrcConfigSetParam------------
	Cmd_StringClear(  &deviceInfo->videoSrcConfigSetParam.name);
	Cmd_StringClear(  &deviceInfo->videoSrcConfigSetParam.token);
	Cmd_StringClear(   &deviceInfo->videoSrcConfigSetParam.srcToken);
	//----------uninit_VideoSrcConfigSetParam------------
	//----------------uninit_VideoOSDConfig------------------
	Cmd_StringClear(  &deviceInfo->videoOSDConfig.videoSrcToken);
	Cmd_StringClear(  &deviceInfo->videoOSDConfig.text);
	//----------------uninit_VideoOSDConfig------------------
	//------------------------uninit_VideoPrivateArea--------------------------
	Cmd_StringClear(  &deviceInfo->videoPrivateArea.videoSrcToken);
	User_memory_free(deviceInfo->videoPrivateArea.privateAreaPolygon);
	deviceInfo->videoPrivateArea.privateAreaPolygon = NULL;
	deviceInfo->videoPrivateArea.polygonListSize = 0;
	//------------------------uninit_VideoPrivateArea--------------------------
	//-------------------uninit_SyncPoint---------------------
	Cmd_StringClear( &deviceInfo->syncPoint.profileToken);
	//-------------------uninit_SyncPoint---------------------
	//------------------------uninit_VideoSrcControl--------------------------
	Cmd_StringClear( &deviceInfo->videoSrcControl.videoSrcToken);
	//------------------------uninit_VideoSrcControl--------------------------
	//-------------------uninit_PTZConfig--------------------
	for(i = 0; i < deviceInfo->ptzConfig.PTZConfigListSize; i ++)
	{
		Cmd_StringClear(&deviceInfo->ptzConfig.PTZConfigList[i].name);
		Cmd_StringClear(&deviceInfo->ptzConfig.PTZConfigList[i].token);
		Cmd_StringClear(&deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken);
	}
	User_memory_free(deviceInfo->ptzConfig.PTZConfigList);
	deviceInfo->ptzConfig.PTZConfigList = NULL;
	deviceInfo->ptzConfig.PTZConfigListSize = 0;
	//-------------------uninit_PTZConfig--------------------
	//-------------------uninit_PTZStatus---------------------
	Cmd_StringClear( &deviceInfo->ptzStatus.token);
	Cmd_StringClear( &deviceInfo->ptzStatus.error);
	//-------------------uninit_PTZStatus---------------------
	//------------------uninit_PTZPresetsGet---------------------
	for( i = 0; i < deviceInfo->ptzPresetsGet.configListSize; i ++)
	{
		Cmd_StringClear( &deviceInfo->ptzPresetsGet.configList[i].presetName);
		Cmd_StringClear( &deviceInfo->ptzPresetsGet.configList[i].presetToken);
	}
	User_memory_free(deviceInfo->ptzPresetsGet.configList);
	deviceInfo->ptzPresetsGet.configList = NULL;
	deviceInfo->ptzPresetsGet.configListSize = 0;
	Cmd_StringClear( &deviceInfo->ptzPresetsGet.token);
	//------------------uninit_PTZPresetsGet---------------------
	//-----------------------uninit_PTZPresetsParam--------------------------
	Cmd_StringClear( &deviceInfo->ptzPresetsSet.token);
	Cmd_StringClear( &deviceInfo->ptzPresetsSet.presetName);
	Cmd_StringClear( &deviceInfo->ptzPresetsSet.presetToken);
	Cmd_StringClear( &deviceInfo->ptzPresetsSet.presetToken_set);
	//-----------------------uninit_PTZPresetsParam--------------------------
	//----------------uninit_PTZGotoParam-------------------
	Cmd_StringClear(&deviceInfo->ptzGotoParam.token);
	Cmd_StringClear(&deviceInfo->ptzGotoParam.presetToken);
	//----------------uninit_PTZGotoParam-------------------
	//----------------uninit_PTZRemoveParam----------------
	Cmd_StringClear(&deviceInfo->ptzRemoveParam.token);
	Cmd_StringClear(&deviceInfo->ptzRemoveParam.presetToken);
	//----------------uninit_PTZRemoveParam----------------
	//----------------uninit_PTZAbsoluteMove----------------
	Cmd_StringClear(&deviceInfo->ptzAbsoluteMove.token);
	//----------------uninit_PTZAbsoluteMove----------------
	//----------------------uninit_PTZRelativeMove---------------------
	Cmd_StringClear(&deviceInfo->ptzRelativeMove.token);
	//----------------------uninit_PTZRelativeMove---------------------
	//----------------uninit_PTZContinuousMove--------------
	Cmd_StringClear(&deviceInfo->ptzContinuousMove.token);
	//----------------uninit_PTZContinuousMove--------------
	//-----------------uninit_PTZHomePosition----------------
	Cmd_StringClear( &deviceInfo->ptzHomePosition.token);
	//-----------------uninit_PTZHomePosition----------------
	//----------------------uninit_PTZStop---------------------
	Cmd_StringClear( &deviceInfo->ptzStop.token);
	//----------------------uninit_PTZStop---------------------
	//-----------------uninit_SupportedRules-----------------
	User_memory_free(deviceInfo->supportedRules.ruleType);
	//-----------------uninit_SupportedRules-----------------
	//------------------------init_DeleteRule--------------------------
	Cmd_StringClear(&deviceInfo->deleteRule.ruleToken);
	//------------------------init_DeleteRule--------------------------
	//--------------------uninit_totalRule----------------------
	Cmd_StringClear(   &deviceInfo->totalRule.ruleToken);
	Cmd_StringClear(   &deviceInfo->totalRule.ruleName);
	Cmd_StringClear(   &deviceInfo->totalRule.videoSrcToken);
	if(deviceInfo->totalRule.type == 0x10)
	{
		ptrRule_LineDetector = (Rule_LineDetector*) deviceInfo->totalRule.rule;
		User_memory_free( ptrRule_LineDetector->detectPolygon);
		User_memory_free( ptrRule_LineDetector);
		deviceInfo->totalRule.type = 0x00;
		deviceInfo->totalRule.rule = NULL;
		ptrRule_LineDetector = NULL;
	}else if(deviceInfo->totalRule.type == 0x11)
	{
		ptrRule_FieldDetector = (Rule_FieldDetector*) deviceInfo->totalRule.rule;
		User_memory_free( ptrRule_FieldDetector->detectPolygon);
		User_memory_free( ptrRule_FieldDetector);
		deviceInfo->totalRule.type = 0x00;
		deviceInfo->totalRule.rule = NULL;
		ptrRule_FieldDetector = NULL;
	}else if(deviceInfo->totalRule.type == 0x12)
	{
		ptrRule_MotionDetector = (Rule_MotionDetector*) deviceInfo->totalRule.rule;
		User_memory_free( ptrRule_MotionDetector->detectPolygon);
		User_memory_free( ptrRule_MotionDetector);
		deviceInfo->totalRule.type = 0x00;
		deviceInfo->totalRule.rule = NULL;
		ptrRule_MotionDetector = NULL;
	}else if(deviceInfo->totalRule.type == 0x13)
	{
		ptrRule_Counting = (Rule_Counting*) deviceInfo->totalRule.rule;
		User_memory_free( ptrRule_Counting->detectPolygon);
		User_memory_free( ptrRule_Counting);
		deviceInfo->totalRule.type = 0x00;
		deviceInfo->totalRule.rule = NULL;
		ptrRule_Counting = NULL;
	}else if(deviceInfo->totalRule.type == 0x14)
	{
		ptrRule_CellMotion = (Rule_CellMotion*) deviceInfo->totalRule.rule;
		Cmd_StringClear(&ptrRule_CellMotion->activeCells);
		User_memory_free( ptrRule_CellMotion);
		deviceInfo->totalRule.type = 0x00;
		deviceInfo->totalRule.rule = NULL;
		ptrRule_CellMotion = NULL;
	}
	//--------------------uninit_totalRule----------------------
	//---------------------uninit_ruleList-----------------------
	for( i = 0; i < deviceInfo->ruleList.maxListSize; i ++)
	{
		if((deviceInfo->ruleList.totalRuleList[i].type >= 0x10) && (deviceInfo->ruleList.totalRuleList[i].type <= 0x14))
		{
			Cmd_StringClear(&deviceInfo->ruleList.totalRuleList[i].ruleName);
			Cmd_StringClear(&deviceInfo->ruleList.totalRuleList[i].ruleToken);
			Cmd_StringClear(&deviceInfo->ruleList.totalRuleList[i].videoSrcToken);
		}else{
			continue;
		}

		if(deviceInfo->ruleList.totalRuleList[i].type == 0x10)
		{
			ptrRule_LineDetector = (Rule_LineDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
			User_memory_free( ptrRule_LineDetector->detectPolygon);
			ptrRule_LineDetector->detectPolygon = NULL;
			User_memory_free( ptrRule_LineDetector);
			deviceInfo->ruleList.totalRuleList[i].type = 0x00;
			deviceInfo->ruleList.totalRuleList[i].rule = NULL;
			ptrRule_LineDetector = NULL;
		}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x11)
		{
			ptrRule_FieldDetector = (Rule_FieldDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
			User_memory_free( ptrRule_FieldDetector->detectPolygon);
			ptrRule_FieldDetector->detectPolygon = NULL;
			User_memory_free( ptrRule_FieldDetector);
			deviceInfo->ruleList.totalRuleList[i].type = 0x00;
			deviceInfo->ruleList.totalRuleList[i].rule = NULL;
			ptrRule_FieldDetector = NULL;
		}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x12)
		{
			ptrRule_MotionDetector = (Rule_MotionDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
			User_memory_free( ptrRule_MotionDetector->detectPolygon);
			ptrRule_MotionDetector->detectPolygon = NULL;
			User_memory_free( ptrRule_MotionDetector);
			deviceInfo->ruleList.totalRuleList[i].type = 0x00;
			deviceInfo->ruleList.totalRuleList[i].rule = NULL;
			ptrRule_MotionDetector = NULL;
		}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x13)
		{
			ptrRule_Counting = (Rule_Counting*) deviceInfo->ruleList.totalRuleList[i].rule;
			User_memory_free( ptrRule_Counting->detectPolygon);
			ptrRule_Counting->detectPolygon = NULL;
			User_memory_free( ptrRule_Counting);
			deviceInfo->ruleList.totalRuleList[i].type = 0x00;
			deviceInfo->ruleList.totalRuleList[i].rule = NULL;
			ptrRule_Counting = NULL;
		}else if(deviceInfo->ruleList.totalRuleList[i].type == 0x14)
		{
			ptrRule_CellMotion = (Rule_CellMotion*) deviceInfo->ruleList.totalRuleList[i].rule;
			Cmd_StringClear(&ptrRule_CellMotion->activeCells);
			User_memory_free( ptrRule_CellMotion);
			deviceInfo->ruleList.totalRuleList[i].type = 0x00;
			deviceInfo->ruleList.totalRuleList[i].rule = NULL;
			ptrRule_CellMotion = NULL;
		}
	}
	User_memory_free(deviceInfo->ruleList.totalRuleList);
	deviceInfo->ruleList.totalRuleList = NULL;

	//-------------------uninit_ruleList----------------------
	//--------------uninit_MetaDataStream-----------------
	//*************uninit_metadata_Event*************
	Cmd_StringClear( &deviceInfo->metadataStreamInfo.metadata_Event.ruleToken);
	Cmd_StringClear( &deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken);
	//*************uninit_metadata_Event*************
	Cmd_StringClear(&deviceInfo->metadataStreamInfo.metadata_Device.textInformation);
	for( i = 0; i < deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize; i ++)
		Cmd_StringClear(&deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken);
	User_memory_free(deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList);
	deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList = NULL;
	deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize = 0;
	//--------------uninit_MetaDataStream-----------------
#endif
}
//*******************User Define UnInit********************

//*******************User Define Reply********************
//--------------------------General-------------------------------
void User_getSecurity(RCString /*userName*/, RCString /*password*/, Byte* valid)
{
#if	UART_TEST
    *valid = Valid;
#endif
}

void User_getGeneralReply(RCHostInfo* /*deviceInfo*/, Word command)
{
#if	UART_TEST
	printf("=== Cmd=0x%04x: ===\n\n", command);
	printf("\n >>>Set Command OK <<<!!!!\n\n");
	printf("=== Cmd=0x%04x: ===\n\n", command);
#endif
}

#if 0
void User_getErrorReply(Device device, ReturnValue returnValue, Word command)
{
#if	UART_TEST
	printf("=== Cmd=0x%04x: ===\n\n", command);

	if(returnValue.returnCode != 0)
	{
		switch(returnValue.returnCode)
		{
		case 0x01:
			switch(command)
			{
			case CMD_GetCapabilitiesOutput:
				printf("The requested service category is not supported by the device!\n");
				break;
			case CMD_GetSystemLogOutput:
				printf("There is no access log information available!\n");
				break;
			case CMD_SetHostnameOutput:
				printf("The requested hostname cannot be accepted by the device!\n");
				break;
			case CMD_SetRelayOutputStateOutput:
			case CMD_SetRelayOutputSettingsOutput:
				printf("Unknown relay token reference!\n");
				break;
			case CMD_GetImagingSettingsOutput:
			case CMD_IMG_GetStatusOutput:
			case CMD_SetImagingSettingsOutput:
			case CMD_IMG_MoveOutput:
			case CMD_IMG_StopOutput:
			case CMD_IMG_GetOptionsOutput:
			case CMD_GetUserDefinedSettingsOutput:
			case CMD_SetUserDefinedSettingsOutput:
			case CMD_SetVideoPrivateAreaOutput:
			case CMD_SetVideoSourceControlOutput:
			case CMD_GetMetadataSettingsOutput:
			case CMD_SetMetadataSettingsOutput:
				printf("The requested VideoSource does not exist!\n");
				break;
			case CMD_GetGuaranteedNumberOfVideoEncoderInstancesOutput:
				printf("The requested configuration indicated with ConfigurationToken does not exist!\n");
				break;
			case CMD_GetAudioSourcesOutput:
			case CMD_GetAudioSourceConfigurationsOutput:
			case CMD_GetAudioEncoderConfigurationsOutput:
			case CMD_GetAudioSourceConfigurationOptionsOutput:
			case CMD_GetAudioEncoderConfigurationOptionsOutput:
				printf("The device does not support audio!\n");
				break;
			case CMD_SetSynchronizationPointOutput:
				printf("The profile does not exist!\n");
				break;
			case CMD_SetVideoSourceConfigurationOutput:
			case CMD_SetVideoEncoderConfigurationOutput:
			case CMD_SetAudioSourceConfigurationOutput:
			case CMD_SetAudioEncoderConfigurationOutput:
			case CMD_SetHomePositionOutput:
				printf("The configuration does not exist!\n");
				break;
			case CMD_GetConfigurationsOutput:
			case CMD_PTZ_GetStatusOutput:
			case CMD_GetPresetsOutput:
			case CMD_GotoPresetOutput:
			case CMD_RemovePresetOutput:
			case CMD_RelativeMoveOutput:
			case CMD_GotoHomePositionOutput:
			case CMD_PTZ_StopOutput:
			case CMD_GetSupportedRulesOutput:
				printf("The requested configuration token does not exist!\n");
				break;
			case CMD_SetPresetOutput:
				printf("The requested name already exist for another preset!\n");
				break;
			case CMD_AbsoluteMoveOutput:
			case CMD_ContinuousMoveOutput:
				printf("Reference token does not exist!\n");
				break;
			case CMD_CreateRuleOutput:
			case CMD_ModifyRuleOutput:
			case CMD_DeleteRuleOutput:
				printf("Rule token already exist!\n");
				break;
			case CMD_UpgradeSystemFirmwareOutput:
				printf("env:Sender, ter:InvalidArgs, ter:InvalidFirmware!\n");
				break;
			default:
				break;
			}
			break;
		case 0x02:
			switch(command)
			{
			case CMD_GetSystemLogOutput:
				printf("There is no system log information available!\n");
				break;
			case CMD_SetRelayOutputSettingsOutput:
				printf("Monostable delay time not valid!\n");
				break;
			case CMD_GetImagingSettingsOutput:
			case CMD_IMG_GetStatusOutput:
			case CMD_SetImagingSettingsOutput:
			case CMD_IMG_MoveOutput:
			case CMD_IMG_StopOutput:
			case CMD_IMG_GetOptionsOutput:
			case CMD_GetUserDefinedSettingsOutput:
			case CMD_SetUserDefinedSettingsOutput:
			case CMD_SetVideoPrivateAreaOutput:
			case CMD_SetVideoSourceControlOutput:
			case CMD_GetMetadataSettingsOutput:
			case CMD_SetMetadataSettingsOutput:
				printf("The requested VideoSource does not support imaging settings!\n");
				break;
			case CMD_SetVideoSourceConfigurationOutput:
			case CMD_SetVideoEncoderConfigurationOutput:
			case CMD_SetAudioSourceConfigurationOutput:
			case CMD_SetAudioEncoderConfigurationOutput:
				printf("The configuration parameters are not possible to set!\n");
				break;
			case CMD_GetConfigurationsOutput:
			case CMD_GetPresetsOutput:
			case CMD_PTZ_StopOutput:
				printf("PTZ is not supported!\n");
				break;
			case CMD_PTZ_GetStatusOutput:
				printf("No PTZ status is available!\n");
				break;
			case CMD_GotoPresetOutput:
			case CMD_RemovePresetOutput:
				printf("The requested preset token does not exist.!\n");
				break;
			case CMD_SetPresetOutput:
				printf("The PresetName is either too long or contains invalid characters.!\n");
				break;
			case CMD_AbsoluteMoveOutput:
				printf("The requested position is out of bounds!\n");
				break;
			case CMD_RelativeMoveOutput:
				printf("The requested translation is out of bounds!\n");
				break;
			case CMD_ContinuousMoveOutput:
				printf("The specified timeout argument is not within the supported timeout range!\n");
				break;
			case CMD_SetHomePositionOutput:
				printf("The home position is fixed and cannot be overwritten!\n");
				break;
			case CMD_GotoHomePositionOutput:
				printf("No home position has been defined!\n");
				break;
			case CMD_CreateRuleOutput:
			case CMD_ModifyRuleOutput:
				printf("The suggested rule parameters is not valid!\n");
				break;
			case CMD_UpgradeSystemFirmwareOutput:
				printf("env:Receiver, ter:Action, ter:FirmwareUpgrade-Failed!\n");
				break;
			default:
				break;
			}
			break;
		case 0x03:
			switch(command)
			{
			case CMD_SetImagingSettingsOutput:
				printf("The requested settings are incorrect!\n");
				break;
			case CMD_SetVideoSourceConfigurationOutput:
			case CMD_SetVideoEncoderConfigurationOutput:
			case CMD_SetAudioSourceConfigurationOutput:
			case CMD_SetAudioEncoderConfigurationOutput:
				printf("The new settings conflicts with other uses of the configuration!\n");
				break;
			case CMD_PTZ_GetStatusOutput:
			case CMD_RemovePresetOutput:
			case CMD_SetHomePositionOutput:
			case CMD_GotoHomePositionOutput:
				printf("PTZ is not supported!\n");
				break;
			case CMD_GotoPresetOutput:
			case CMD_AbsoluteMoveOutput:
			case CMD_RelativeMoveOutput:
			case CMD_ContinuousMoveOutput:
				printf("The requested speed is out of bounds!\n");
				break;
			case CMD_SetPresetOutput:
				printf("Preset cannot be set while PTZ unit is moving.!\n");
				break;
			case CMD_CreateRuleOutput:
				printf("There is not enough space in the device to add the rules to the configuration!\n");
				break;
			default:
				break;
			}
			break;
		case 0x04:
			switch(command)
			{
			case CMD_SetAudioSourceConfigurationOutput:
			case CMD_SetAudioEncoderConfigurationOutput:
				printf("The device does not support audio!\n");
				break;
			case CMD_GotoPresetOutput:
			case CMD_AbsoluteMoveOutput:
			case CMD_RelativeMoveOutput:
			case CMD_ContinuousMoveOutput:
				printf("PTZ is not supported!\n");
				break;
			case CMD_SetPresetOutput:
				printf("Maximum number of Presets reached!\n");
				break;
			default:
				break;
			}
			break;
		case 0x05:
			switch(command)
			{
			case CMD_SetPresetOutput:
				printf("The requested configuration token does not exist!\n");
				break;
			default:
				break;
			}
			break;
		case 0x06:
			switch(command)
			{
			case CMD_SetPresetOutput:
				printf("The requested preset token does not exist!\n");
				break;
			default:
				break;
			}
			break;
		case 0x07:
			switch(command)
			{
			case CMD_SetPresetOutput:
				printf("PTZ is not supported!\n");
				break;
			default:
				break;
			}
			break;
		case 0xFB:
			printf("Username error!\n");
			break;
		case 0xFC:
			printf("Password error!\n");
			break;
		case 0xFD:
			printf("Command unsupported!\n");
			break;
		case 0xFE:
			printf("Command Cheksum error!\n");
			break;
		case 0xFF:
			printf("Command Fail!\n");
			break;
		default:
			break;
		}
	}
	printf("Cmd=0x%04x, returnCode=0x%02x\n",command,returnValue.returnCode);
	printf("=== Cmd=0x%04x: ===\n\n", command);
#endif
}
#endif

//--------------------------General-------------------------------

//-----------------------ccHDtv Service--------------------------
#if 0
void User_getDeviceAddressReply(RCHostInfo* deviceInfo, Word command)
{
	NewTxDevice* in_NewTxDevice = &deviceInfo->newTxDevice;

	printf("=== Cmd=0x%04x: User_getDeviceAddressReply ===\n\n", CMD_GetTxDeviceAddressIDOutput);
	printf("    deviceID	= %x\n",in_NewTxDevice->deviceAddressID);
	printf("=== Cmd=0x%04x: User_getDeviceAddressReply ===\n\n", CMD_GetTxDeviceAddressIDOutput);
}
#endif

void User_getTransmissionParameterReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	TransmissionParameter* in_TransmissionParameter = &deviceInfo->transmissionParameter;

	printf("=== Cmd=0x%04x: User_getTransmissionParameterReply ===\n\n", CMD_GetTransmissionParametersOutput);
	printf("    bandwidth			= 0x%x\n",in_TransmissionParameter->bandwidth);
	printf("    frequency			= %u\n",in_TransmissionParameter->frequency);
	printf("    constellation		= 0x%x\n",in_TransmissionParameter->constellation);
	printf("    FFT				= 0x%x\n",in_TransmissionParameter->FFT);
	printf("    codeRate			= 0x%x\n",in_TransmissionParameter->codeRate);
	printf("    interval			= 0x%x\n",in_TransmissionParameter->interval);
	printf("    attenuation			= 0x%02x\n",in_TransmissionParameter->attenuation);
	printf("    extensionFlag		= %u\n",in_TransmissionParameter->extensionFlag);
	if(in_TransmissionParameter->extensionFlag == 1)
	{
		printf("    attenuation_signed		= %d\n",in_TransmissionParameter->attenuation_signed);
		printf("    TPSCellID			= 0x%x\n",in_TransmissionParameter->TPSCellID);
		printf("    channelNum			= %u\n",in_TransmissionParameter->channelNum);
		printf("    bandwidthStrapping		= %u\n",in_TransmissionParameter->bandwidthStrapping);
		printf("    TVStandard			= %u\n",in_TransmissionParameter->TVStandard);
		printf("    segmentationMode		= %u\n",in_TransmissionParameter->segmentationMode);
		printf("    oneSeg_Constellation	= 0x%x\n",in_TransmissionParameter->oneSeg_Constellation);
		printf("    oneSeg_CodeRate		= 0x%x\n",in_TransmissionParameter->oneSeg_CodeRate);
	}else
	{
		printf("    No Extension Parameter!\n");
	}
	printf("=== Cmd=0x%04x: User_getTransmissionParameterReply ===\n\n", CMD_GetTransmissionParametersOutput);
#endif
}

void User_getHwRegisterValuesReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	HwRegisterInfo* in_HwRegisterInfo = &deviceInfo->hwRegisterInfo;
	Byte i = 0;

	printf("=== Cmd=0x%04x: User_getHwRegisterValuesReply ===\n\n", CMD_GetHwRegisterValuesOutput);
	printf("    valueListSize	= %u\n",in_HwRegisterInfo->valueListSize);

	printf("    registerValues	=\n");
	for( i = 0; i < in_HwRegisterInfo->valueListSize; i ++)
		printf("    %u : 0x%02x \n",i,in_HwRegisterInfo->registerValues[i]);
	printf("\n");
	printf("=== Cmd=0x%04x: User_getHwRegisterValuesReply ===\n\n", CMD_GetHwRegisterValuesOutput);
#endif
}

void User_getAdvanceOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	AdvanceOptions* in_AdvanceOptions = &deviceInfo->advanceOptions;

	printf("=== Cmd=0x%04x: User_getAdvanceOptionsReply ===\n\n", CMD_GetAdvanceOptionsOutput);
	printf("    PTS_PCR_delayTime			= %u\n",in_AdvanceOptions->PTS_PCR_delayTime);
	printf("    Rx_LatencyRecoverTimeInterval(hr)	= %u\n",in_AdvanceOptions->timeInterval);
	printf("    skipNumber				= %u\n",in_AdvanceOptions->skipNumber);
	printf("    overFlowNumber			= %u\n",in_AdvanceOptions->overFlowNumber);
	printf("    overFlowSize			= %u\n",in_AdvanceOptions->overFlowSize);
	printf("    extensionFlag			= %u\n",in_AdvanceOptions->extensionFlag);
	if(in_AdvanceOptions->extensionFlag == 1)
	{
		printf("    Rx_LatencyRecoverTimeInterval(min)	= %u\n",in_AdvanceOptions->Rx_LatencyRecoverTimeInterval);
		printf("    SIPSITableDuration			= %u\n",in_AdvanceOptions->SIPSITableDuration);
		printf("    frameRateAdjustment			= %d\n",in_AdvanceOptions->frameRateAdjustment);
		printf("    repeatPacketMode			= %u\n",in_AdvanceOptions->repeatPacketMode);
		printf("    repeatPacketNum			= %u\n",in_AdvanceOptions->repeatPacketNum);
		printf("    repeatPacketTimeInterval		= %u\n",in_AdvanceOptions->repeatPacketTimeInterval);
		printf("    TS_TableDisable			= 0x%x\n",in_AdvanceOptions->TS_TableDisable);
	}else
	{
		printf("    No Extension Parameter!\n");
	}
	printf("=== Cmd=0x%04x: User_getAdvanceOptionsReply ===\n\n", CMD_GetAdvanceOptionsOutput);
#endif
}

void User_getTPSInfoReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	TPSInfo* in_TPSInfo = &deviceInfo->tpsInfo;

	printf("=== Cmd=0x%04x: User_getTPSInfoReply ===\n\n", CMD_GetTPSInformationOutput);
	printf("    Cell ID		= 0x%x\n",in_TPSInfo->cellID);
	printf("    High Code Rate	= 0x%x\n",in_TPSInfo->highCodeRate);
	printf("    Low Code Rate	= 0x%x\n",in_TPSInfo->lowCodeRate);
	printf("    Transmission Mode	= 0x%x\n",in_TPSInfo->transmissionMode);
	printf("    Constellation	= 0x%x\n",in_TPSInfo->constellation);
	printf("    Interval		= 0x%x\n",in_TPSInfo->interval);
	printf("=== Cmd=0x%04x: User_getTPSInfoReply ===\n\n", CMD_GetTPSInformationOutput);
#endif
}

void User_getSiPsiTableReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	PSITable* in_PSITable = &deviceInfo->psiTable;

	printf("=== Cmd=0x%04x: User_getSiPsiTableReply ===\n\n", CMD_GetSiPsiTableOutput);
	printf("    ONID			= 0x%x\n",in_PSITable->ONID);
	printf("    NID				= 0x%x\n",in_PSITable->NID);
	printf("    TSID			= %u\n",in_PSITable->TSID);
	printf("    networkName			= %s\n",in_PSITable->networkName);
	printf("    extensionFlag		= %u\n",in_PSITable->extensionFlag);
	if(in_PSITable->extensionFlag == 1)
	{
		printf("    privateDataSpecifier	= 0x%08x\n",in_PSITable->privateDataSpecifier);
		printf("    NITVersion			= 0x%02x\n",in_PSITable->NITVersion);
		printf("    countryID			= %u\n",in_PSITable->countryID);
		printf("    languageID			= %u\n",in_PSITable->languageID);
	}else
	{
		printf("    No Extension Parameter!\n");
	}
	printf("=== Cmd=0x%04x: User_getSiPsiTableReply ===\n\n", CMD_GetSiPsiTableOutput);
#endif
}

void User_getNitLocationReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	NITLoacation* in_NITLoacation = &deviceInfo->nitLoacation;

	printf("=== Cmd=0x%04x: User_getNitLocationReply ===\n\n", CMD_GetNitLocationOutput);
	printf("    latitude		= %u\n",in_NITLoacation->latitude);
	printf("    longitude		= %u\n",in_NITLoacation->longitude);
	printf("    extentLatitude	= %u\n",in_NITLoacation->extentLatitude);
	printf("    extentLongitude	= %u\n",in_NITLoacation->extentLongitude);
	printf("=== Cmd=0x%04x: User_getNitLocationReply ===\n\n", CMD_GetNitLocationOutput);
#endif
}

void User_getSdtServiceReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	ServiceConfig* in_ServiceConfig = &deviceInfo->serviceConfig;

	printf("=== Cmd=0x%04x: User_getSdtServiceReply ===\n\n", CMD_GetSdtServiceOutput);
	printf("    serviceID				= %u\n",in_ServiceConfig->serviceID);
	printf("    enable				= %u\n",in_ServiceConfig->enable);
	printf("    LCN					= 0x%x\n",in_ServiceConfig->LCN);

	printf("    serviceName stringLength		= %u\n",in_ServiceConfig->serviceName.stringLength);
	printf("    serviceName stringData		= %s\n",in_ServiceConfig->serviceName.stringData);
	printf("    provider stringLength		= %u\n",in_ServiceConfig->provider.stringLength);
	printf("    provider stringData			= %s\n",in_ServiceConfig->provider.stringData);
	printf("    extensionFlag			= %u\n",in_ServiceConfig->extensionFlag);
	if(in_ServiceConfig->extensionFlag == 1)
	{
		printf("    IDAssignationMode			= %u\n",in_ServiceConfig->IDAssignationMode);
		printf("    ISDBT_RegionID			= %u\n",in_ServiceConfig->ISDBT_RegionID);
		printf("    LISDBT_BroadcasterRegionIDCN	= %u\n",in_ServiceConfig->ISDBT_BroadcasterRegionID);
		printf("    ISDBT_RemoteControlKeyID		= %u\n",in_ServiceConfig->ISDBT_RemoteControlKeyID);
		printf("    ISDBT_ServiceIDDataType_1		= 0x%04x\n",in_ServiceConfig->ISDBT_ServiceIDDataType_1);
		printf("    ISDBT_ServiceIDDataType_2		= 0x%04x\n",in_ServiceConfig->ISDBT_ServiceIDDataType_2);
		printf("    ISDBT_ServiceIDPartialReception	= 0x%04x\n",in_ServiceConfig->ISDBT_ServiceIDPartialReception);
	}else
	{
		printf("    No Extension Parameter!\n");
	}
	printf("=== Cmd=0x%04x: User_getSdtServiceReply ===\n\n", CMD_GetSdtServiceOutput);
#endif
}

void User_getEITInfoReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	EITInfo* in_EITInfo = &deviceInfo->eitInfo;
	Byte i;

	printf("=== Cmd=0x%04x: User_getEITInfoReply ===\n\n", CMD_GetEITInformationOutput);
	printf("    listSize			= %u\n",in_EITInfo->listSize);
	for( i = 0; i < in_EITInfo->listSize; i ++)
	{
		printf("---------------------------------- Index %u -------------------------------------\n",i);
		printf("    enable			= %u\n",in_EITInfo->eitInfoParam[i].enable);
		printf("    startDate			= 0x%08x\n",in_EITInfo->eitInfoParam[i].startDate);
		printf("    startTime			= 0x%08x\n",in_EITInfo->eitInfoParam[i].startTime);
		printf("    duration			= 0x%08x\n",in_EITInfo->eitInfoParam[i].duration);
		printf("    eventName stringLength	= %u\n",in_EITInfo->eitInfoParam[i].eventName.stringLength);
		printf("    eventName stringData	= %s\n",in_EITInfo->eitInfoParam[i].eventName.stringData);
		printf("    eventText stringLength	= %u\n",in_EITInfo->eitInfoParam[i].eventText.stringLength);
		printf("    eventText stringData	= %s\n",in_EITInfo->eitInfoParam[i].eventText.stringData);
		printf("---------------------------------- Index %u -------------------------------------\n",i);

	}
	printf("=== Cmd=0x%04x: User_getEITInfoReply ===\n\n", CMD_GetEITInformationOutput);
#endif
}

void User_getTransmissionParameterCapabilitiesReply(RCHostInfo* deviceInfo, Word /*command*/)
{
	TransmissionParameterCapabilities* in_TransmissionParameterCapabilities = &deviceInfo->transmissionParameterCapabilities;

	printf("=== Cmd=0x%04x: User_getTransmissionParameterCapabilitiesReply ===\n\n", CMD_GetTransmissionParameterCapabilitiesOutput);
	printf("    bandwidthOptions		= %u \n",in_TransmissionParameterCapabilities->bandwidthOptions);
	printf("    frequencyMin		= %u \n",in_TransmissionParameterCapabilities->frequencyMin);
	printf("    frequencyMax		= %u \n",in_TransmissionParameterCapabilities->frequencyMax);
	printf("    constellationOptions	= %u \n",in_TransmissionParameterCapabilities->constellationOptions);
	printf("    FFTOptions			= %u \n",in_TransmissionParameterCapabilities->FFTOptions);
	printf("    codeRateOptions		= %u \n",in_TransmissionParameterCapabilities->codeRateOptions);
	printf("    guardInterval		= %u \n",in_TransmissionParameterCapabilities->guardInterval);
	printf("    attenuationMin		= %u \n",in_TransmissionParameterCapabilities->attenuationMin);
	printf("    attenuationMax		= %u \n",in_TransmissionParameterCapabilities->attenuationMax);
	printf("    extensionFlag		= %u \n",in_TransmissionParameterCapabilities->extensionFlag);
	if(in_TransmissionParameterCapabilities->extensionFlag == 1)	// No Extension
	{
		printf("    attenuationMin_signed	= %d \n",in_TransmissionParameterCapabilities->attenuationMin_signed);
		printf("    attenuationMax_signed	= %d \n",in_TransmissionParameterCapabilities->attenuationMax_signed);
		printf("    TPSCellIDMin		= %u \n",in_TransmissionParameterCapabilities->TPSCellIDMin);
		printf("    TPSCellIDMax		= %u \n",in_TransmissionParameterCapabilities->TPSCellIDMax);
		printf("    channelNumMin		= %u \n",in_TransmissionParameterCapabilities->channelNumMin);
		printf("    channelNumMax		= %u \n",in_TransmissionParameterCapabilities->channelNumMax);
		printf("    bandwidthStrapping		= %u \n",in_TransmissionParameterCapabilities->bandwidthStrapping);
		printf("    TVStandard			= %u \n",in_TransmissionParameterCapabilities->TVStandard);
		printf("    segmentationMode		= %u \n",in_TransmissionParameterCapabilities->segmentationMode);
		printf("    oneSeg_Constellation	= %u \n",in_TransmissionParameterCapabilities->oneSeg_Constellation);
		printf("    oneSeg_CodeRate		= %u \n",in_TransmissionParameterCapabilities->oneSeg_CodeRate);
	}else
	{
		printf("    No Extension Parameter!\n");
	}
	printf("=== Cmd=0x%04x: User_getTransmissionParameterCapabilitiesReply ===\n\n", CMD_GetTransmissionParameterCapabilitiesOutput);
}
//-----------------------ccHDtv Service--------------------------

//-----------------Device Management Service-------------------
void User_getDeviceCapabilityReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	DeviceCapability* in_DeviceCapability = &deviceInfo->deviceCapability;

	printf("=== Cmd=0x%04x: User_getDeviceCapabilityReply ===\n\n", CMD_GetCapabilitiesOutput);
	printf("    supportedFeatures	= 0x%x\n",in_DeviceCapability->supportedFeatures);
	printf("=== Cmd=0x%04x: User_getDeviceCapabilityReply ===\n\n", CMD_GetCapabilitiesOutput);
#endif
}

void User_getDeviceInformationReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	ManufactureInfo* in_ManufactureInfo = &deviceInfo->manufactureInfo;

	printf("=== Cmd=0x%04x: User_getDeviceInformationReply ===\n\n", CMD_GetDeviceInformationOutput);
	printf("    manufactureName.stringLength	= %u\n",in_ManufactureInfo->manufactureName.stringLength);
	printf("    manufactureName.stringData		= %s\n",in_ManufactureInfo->manufactureName.stringData);

	printf("    modelName.stringLength		= %u\n",in_ManufactureInfo->modelName.stringLength);
	printf("    modelName.stringData		= %s\n",in_ManufactureInfo->modelName.stringData);

	printf("    firmwareVersion.stringLength	= %u\n",in_ManufactureInfo->firmwareVersion.stringLength);
	printf("    firmwareVersion.stringData		= %s\n",in_ManufactureInfo->firmwareVersion.stringData);

	printf("    serialNumber.stringLength		= %u\n",in_ManufactureInfo->serialNumber.stringLength);
	printf("    serialNumber.stringData		= %s\n",in_ManufactureInfo->serialNumber.stringData);

	printf("    hardwareId.stringLength		= %u\n",in_ManufactureInfo->hardwareId.stringLength);
	printf("    hardwareId.stringData		= %s\n",in_ManufactureInfo->hardwareId.stringData);
	printf("=== Cmd=0x%04x: User_getDeviceInformationReply ===\n\n", CMD_GetDeviceInformationOutput);
#endif
}

void User_getHostnameReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	HostInfo* in_HostInfo = &deviceInfo->hostInfo;

	printf("=== Cmd=0x%04x: User_getHostnameReply ===\n\n", CMD_GetHostnameOutput);
	printf("    hostName.stringLength	= %u\n",in_HostInfo->hostName.stringLength);
	printf("    hostName.stringData		= %s\n",in_HostInfo->hostName.stringData);
	printf("=== Cmd=0x%04x: User_getHostnameReply ===\n\n", CMD_GetHostnameOutput);
#endif
}

void User_getSystemDateAndTimeReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	SystemTime* in_SystemTime = &deviceInfo->systemTime;

	printf("=== Cmd=0x%04x: User_getSystemDateAndTimeReply ===\n\n", CMD_GetSystemDateAndTimeOutput);
	printf("    countryCode			= %c%c%c\n",in_SystemTime->countryCode[0],in_SystemTime->countryCode[1],in_SystemTime->countryCode[2]);
	printf("    countryRegionID		= %u\n",in_SystemTime->countryRegionID);
	printf("    daylightSavings		= %u\n",in_SystemTime->daylightSavings);
	printf("    timeZone			= 0x%02x\n",in_SystemTime->timeZone);
	printf("    UTCHour			= %u\n",in_SystemTime->UTCHour);
	printf("    UTCMinute			= %u\n",in_SystemTime->UTCMinute);
	printf("    UTCSecond			= %u\n",in_SystemTime->UTCSecond);
	printf("    UTCYear			= %u\n",in_SystemTime->UTCYear);
	printf("    UTCMonth			= %u\n",in_SystemTime->UTCMonth);
	printf("    UTCDay			= %u\n",in_SystemTime->UTCDay);
	printf("    extensionFlag		= %u\n",in_SystemTime->extensionFlag);
	if(in_SystemTime->extensionFlag == 1)
	{
		printf("    UTCMillisecond		= %u\n",in_SystemTime->UTCMillisecond);
		printf("    timeAdjustmentMode		= %u\n",in_SystemTime->timeAdjustmentMode);
		printf("    timeAdjustmentCriterionMax	= %u\n",in_SystemTime->timeAdjustmentCriterionMax);
		printf("    timeAdjustmentCriterionMin	= %u\n",in_SystemTime->timeAdjustmentCriterionMin);
		printf("    timeSyncDuration		= %u\n",in_SystemTime->timeSyncDuration);
	}else
	{
		printf("    No Extension Parameter!\n");
	}
	printf("=== Cmd=0x%04x: User_getSystemDateAndTimeReply ===\n\n", CMD_GetSystemDateAndTimeOutput);
#endif
}

void User_getSystemLogReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	SystemLog* in_SystemLog = &deviceInfo->systemLog;

	printf("=== Cmd=0x%04x: User_getSystemLogReply ===\n\n", CMD_GetSystemLogOutput);
	printf("    systemLog stringLength	= %u\n",in_SystemLog->logData.stringLength);
	printf("    systemLog stringData	= %s\n",in_SystemLog->logData.stringData);
	printf("=== Cmd=0x%04x: User_getSystemLogReply ===\n\n", CMD_GetSystemLogOutput);
#endif
}

void User_getOSDInfoReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	OSDInfo* in_OSDInfo = &deviceInfo->osdInfo;

	printf("=== Cmd=0x%04x: User_getOSDInfoReply ===\n\n", CMD_GetOSDInformationOutput);
	printf("    dateEnable		= %u\n",in_OSDInfo->dateEnable);
	printf("    datePosition	= %u\n",in_OSDInfo->datePosition);
	printf("    dateFormat		= %u\n",in_OSDInfo->dateFormat);
	printf("    timeEnable		= %u\n",in_OSDInfo->timeEnable);
	printf("    timePosition	= %u\n",in_OSDInfo->timePosition);
	printf("    dateFormat		= %u\n",in_OSDInfo->timeFormat);
	printf("    logoEnable		= %u\n",in_OSDInfo->logoEnable);
	printf("    logoPosition	= %u\n",in_OSDInfo->logoPosition);
	printf("    logooption		= %u\n",in_OSDInfo->logooption);
	printf("    detailInfoEnable	= %u\n",in_OSDInfo->detailInfoEnable);
	printf("    detailInfoPosition	= %u\n",in_OSDInfo->detailInfoPosition);
	printf("    detailInfooption	= %u\n",in_OSDInfo->detailInfooption);
	printf("    textEnable		= %u\n",in_OSDInfo->textEnable);
	printf("    textPosition	= %u\n",in_OSDInfo->textPosition);

	printf("    text stringLength	= %u\n",in_OSDInfo->text.stringLength);
	printf("    text stringData	= %s\n",in_OSDInfo->text.stringData);
	printf("=== Cmd=0x%04x: User_getOSDInfoReply ===\n\n", CMD_GetOSDInformationOutput);
#endif
}

void User_systemRebootReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	SystemReboot* in_SystemReboot = &deviceInfo->systemReboot;

	printf("=== Cmd=0x%04x: User_systemReboot ===\n\n", CMD_SystemRebootOutput);
	printf("    rebootMessage stringLength	= %u\n",in_SystemReboot->responseMessage.stringLength);
	printf("    rebootMessage stringData	= %s\n",in_SystemReboot->responseMessage.stringData);
	printf("=== Cmd=0x%04x: User_systemReboot ===\n\n", CMD_SystemRebootOutput);
#endif
}

void User_upgradeSystemFirmwareReply(RCHostInfo* deviceInfo, Word command)
{
#if	UART_TEST
	SystemFirmware* in_SystemFirmware = &deviceInfo->systemFirmware;

	printf("=== Cmd=0x%04x: User_upgradeSystemFirmwareReply ===\n\n", command);
	printf("    message stringLength	= %u\n",in_SystemFirmware->message.stringLength);
	printf("    message stringData		= %s\n",in_SystemFirmware->message.stringData);
	printf("=== Cmd=0x%04x: User_upgradeSystemFirmwareReply ===\n\n", command);
#endif
}

//-----------------Device Management Service-------------------

//---------------------Device_IO Service------------------------
void User_getDigitalInputsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	DigitalInputsInfo* in_DigitalInputsInfo = &deviceInfo->digitalInputsInfo;
	Byte i =0;

	printf("=== Cmd=0x%04x: User_getDigitalInputsReply ===\n\n", CMD_GetDigitalInputsOutput);
	printf("    listSize			= %u\n",in_DigitalInputsInfo->listSize);

	for( i = 0; i < in_DigitalInputsInfo->listSize; i ++)
	{
		printf("---------------------------------- Index %u -------------------------------------\n",i);
		printf("    upgradeMessage stringLength	= %u\n",in_DigitalInputsInfo->tokenList[i].stringLength);
		printf("    upgradeMessage stringData	= %s\n",in_DigitalInputsInfo->tokenList[i].stringData);
		printf("---------------------------------- Index %u -------------------------------------\n",i);
	}
	printf("=== Cmd=0x%04x: User_getDigitalInputsReply ===\n\n", CMD_GetDigitalInputsOutput);
#endif
}

void User_getRelayOutputsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	RelayOutputs* in_RelayOutputs = &deviceInfo->relayOutputs;
	Byte i =0;

	printf("=== Cmd=0x%04x: User_getRelayOutputsReply ===\n\n", CMD_GetRelayOutputsOutput);
	printf("    listSize		= %u\n",in_RelayOutputs->listSize);

	for( i = 0; i < in_RelayOutputs->listSize; i ++)
	{
		printf("---------------------------------- Index %u -------------------------------------\n",i);
		printf("    token stringLength	= %u\n", in_RelayOutputs->relayOutputsParam[i].token.stringLength);
		printf("    token stringData	= %s\n", in_RelayOutputs->relayOutputsParam[i].token.stringData);

		printf("    mode		= %u\n", in_RelayOutputs->relayOutputsParam[i].mode);
		printf("    delayTime		= %u\n", in_RelayOutputs->relayOutputsParam[i].delayTime);
		printf("    idleState		= %u\n", in_RelayOutputs->relayOutputsParam[i].idleState);
		printf("---------------------------------- Index %u -------------------------------------\n",i);
	}
	printf("=== Cmd=0x%04x: User_getRelayOutputsReply ===\n\n", CMD_GetRelayOutputsOutput);
#endif
}

//---------------------Device_IO Service------------------------

//-----------------------Imaging Service-------------------------
void User_getImagingSettingsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	ImageConfig* in_ImageConfig = &deviceInfo->imageConfig;

	printf("=== Cmd=0x%04x: User_getImagingSettingsReply ===\n\n", CMD_GetImagingSettingsOutput);
	printf("    backlightCompensationMode	= %u\n",in_ImageConfig->backlightCompensationMode);
	printf("    backlightCompensationLevel	= %f\n",in_ImageConfig->backlightCompensationLevel);
	printf("    brightness			= %f\n",in_ImageConfig->brightness);
	printf("    colorSaturation		= %f\n",in_ImageConfig->colorSaturation);
	printf("    contrast			= %f\n",in_ImageConfig->contrast);
	printf("    exposureMode		= %u\n",in_ImageConfig->exposureMode);
	printf("    exposurePriority		= %u\n",in_ImageConfig->exposurePriority);
	printf("    exposureWindowbottom	= %u\n",in_ImageConfig->exposureWindowbottom);
	printf("    exposureWindowtop		= %u\n",in_ImageConfig->exposureWindowtop);
	printf("    exposureWindowright		= %u\n",in_ImageConfig->exposureWindowright);
	printf("    exposureWindowleft		= %u\n",in_ImageConfig->exposureWindowleft);
	printf("    minExposureTime		= %u\n",in_ImageConfig->minExposureTime);
	printf("    maxExposureTime		= %u\n",in_ImageConfig->maxExposureTime);
	printf("    exposureMinGain		= %f\n",in_ImageConfig->exposureMinGain);
	printf("    exposureMaxGain		= %f\n",in_ImageConfig->exposureMaxGain);
	printf("    exposureMinIris		= %f\n",in_ImageConfig->exposureMinIris);
	printf("    exposureMaxIris		= %f\n",in_ImageConfig->exposureMaxIris);
	printf("    exposureTime		= %u\n",in_ImageConfig->exposureTime);
	printf("    exposureGain		= %f\n",in_ImageConfig->exposureGain);
	printf("    exposureIris		= %f\n",in_ImageConfig->exposureIris);
	printf("    autoFocusMode		= %u\n",in_ImageConfig->autoFocusMode);
	printf("    focusDefaultSpeed		= %f\n",in_ImageConfig->focusDefaultSpeed);
	printf("    focusNearLimit		= %f\n",in_ImageConfig->focusNearLimit);
	printf("    focusFarLimit		= %f\n",in_ImageConfig->focusFarLimit);
	printf("    irCutFilterMode		= %u\n",in_ImageConfig->irCutFilterMode);
	printf("    sharpness			= %f\n",in_ImageConfig->sharpness);
	printf("    wideDynamicRangeMode	= %u\n",in_ImageConfig->wideDynamicRangeMode);
	printf("    wideDynamicRangeLevel	= %f\n",in_ImageConfig->wideDynamicRangeLevel);
	printf("    whiteBalanceMode		= %u\n",in_ImageConfig->whiteBalanceMode);
	printf("    whiteBalanceCrGain		= %f\n",in_ImageConfig->whiteBalanceCrGain);
	printf("    whiteBalanceCbGain		= %f\n",in_ImageConfig->whiteBalanceCbGain);
	printf("    analogTVOutputStandard	= %u\n",in_ImageConfig->analogTVOutputStandard);
	printf("    imageStabilizationLevel	= %f\n",in_ImageConfig->imageStabilizationLevel);
	printf("    extensionFlag		= %u\n",in_ImageConfig->extensionFlag);
	if(in_ImageConfig->extensionFlag == 1)
	{
		printf("    flickerControl		= %u\n",in_ImageConfig->flickerControl);
		printf("    imageStabilizationMode	= %u\n",in_ImageConfig->imageStabilizationMode);
		printf("    deNoiseMode			= %u\n",in_ImageConfig->deNoiseMode);
		printf("    deNoiseStrength		= %f\n",in_ImageConfig->deNoiseStrength);
		printf("    backLightControlMode	= %u\n",in_ImageConfig->backLightControlMode);
		printf("    backLightControlStrength	= %f\n",in_ImageConfig->backLightControlStrength);
	}else
	{
		printf("    No Extension Member!\n");
	}

	printf("=== Cmd=0x%04x: User_getImagingSettingsReply ===\n\n", CMD_GetImagingSettingsOutput);
#endif
}

void User_getStatusReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	FocusStatusInfo* in_FocusStatusInfo = &deviceInfo->focusStatusInfo;

	printf("=== Cmd=0x%04x: User_getStatusReply ===\n\n", CMD_IMG_GetStatusOutput);
	printf("    position		= %u\n",in_FocusStatusInfo->position);
	printf("    moveStatus		= %u\n",in_FocusStatusInfo->moveStatus);
	printf("    error stringLength	= %u\n",in_FocusStatusInfo->error.stringLength);
	printf("    error stringData	= %s\n",in_FocusStatusInfo->error.stringData);
	printf("=== Cmd=0x%04x: User_getStatusReply ===\n\n", CMD_IMG_GetStatusOutput);
#endif
}

void User_getOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	ImageConfigOption* in_ImageConfigOption = &deviceInfo->imageConfigOption;

	printf("=== Cmd=0x%04x: User_getOptionsReply ===\n\n", CMD_IMG_GetOptionsOutput);
	printf("    backlightCompensationMode		= %u\n",in_ImageConfigOption->backlightCompensationMode);
	printf("    backlightCompensationLevelMin	= %f\n",in_ImageConfigOption->backlightCompensationLevelMin);
	printf("    backlightCompensationLevelMax	= %f\n",in_ImageConfigOption->backlightCompensationLevelMax);
	printf("    brightnessMin			= %f\n",in_ImageConfigOption->brightnessMin);
	printf("    brightnessMax			= %f\n",in_ImageConfigOption->brightnessMax);
	printf("    colorSaturationMin			= %f\n",in_ImageConfigOption->colorSaturationMin);
	printf("    colorSaturationMax			= %f\n",in_ImageConfigOption->colorSaturationMax);
	printf("    contrastMin				= %f\n",in_ImageConfigOption->contrastMin);
	printf("    contrastMax				= %f\n",in_ImageConfigOption->contrastMax);
	printf("    exposureMode			= %u\n",in_ImageConfigOption->exposureMode);
	printf("    exposurePriority			= %u\n",in_ImageConfigOption->exposurePriority);
	printf("    minExposureTimeMin			= %u\n",in_ImageConfigOption->minExposureTimeMin);
	printf("    minExposureTimeMax			= %u\n",in_ImageConfigOption->minExposureTimeMax);
	printf("    maxExposureTimeMin			= %u\n",in_ImageConfigOption->maxExposureTimeMin);
	printf("    maxExposureTimeMax			= %u\n",in_ImageConfigOption->maxExposureTimeMax);
	printf("    exposureMinGainMin			= %f\n",in_ImageConfigOption->exposureMinGainMin);
	printf("    exposureMinGainMax			= %f\n",in_ImageConfigOption->exposureMinGainMax);
	printf("    exposureMaxGainMin			= %f\n",in_ImageConfigOption->exposureMaxGainMin);
	printf("    exposureMaxGainMax			= %f\n",in_ImageConfigOption->exposureMaxGainMax);
	printf("    exposureMinIrisMin			= %f\n",in_ImageConfigOption->exposureMinIrisMin);
	printf("    exposureMinIrisMax			= %f\n",in_ImageConfigOption->exposureMinIrisMax);
	printf("    exposureMaxIrisMin			= %f\n",in_ImageConfigOption->exposureMaxIrisMin);
	printf("    exposureMaxIrisMax			= %f\n",in_ImageConfigOption->exposureMaxIrisMax);
	printf("    exposureTimeMin			= %u\n",in_ImageConfigOption->exposureTimeMin);
	printf("    exposureTimeMax			= %u\n",in_ImageConfigOption->exposureTimeMax);
	printf("    exposureGainMin			= %f\n",in_ImageConfigOption->exposureGainMin);
	printf("    exposureGainMax			= %f\n",in_ImageConfigOption->exposureGainMax);
	printf("    exposureIrisMin			= %f\n",in_ImageConfigOption->exposureIrisMin);
	printf("    exposureIrisMax			= %f\n",in_ImageConfigOption->exposureIrisMax);
	printf("    autoFocusMode			= %u\n",in_ImageConfigOption->autoFocusMode);
	printf("    focusDefaultSpeedMin		= %f\n",in_ImageConfigOption->focusDefaultSpeedMin);
	printf("    focusDefaultSpeedMax		= %f\n",in_ImageConfigOption->focusDefaultSpeedMax);
	printf("    focusNearLimitMin			= %f\n",in_ImageConfigOption->focusNearLimitMin);
	printf("    focusNearLimitMax			= %f\n",in_ImageConfigOption->focusNearLimitMax);
	printf("    focusFarLimitMin			= %f\n",in_ImageConfigOption->focusFarLimitMin);
	printf("    focusFarLimitMax			= %f\n",in_ImageConfigOption->focusFarLimitMax);
	printf("    irCutFilterMode			= %u\n",in_ImageConfigOption->irCutFilterMode);
	printf("    sharpnessMin			= %f\n",in_ImageConfigOption->sharpnessMin);
	printf("    sharpnessMax			= %f\n",in_ImageConfigOption->sharpnessMax);
	printf("    wideDynamicRangeMode		= %u\n",in_ImageConfigOption->wideDynamicRangeMode);
	printf("    wideDynamicRangeLevelMin		= %f\n",in_ImageConfigOption->wideDynamicRangeLevelMin);
	printf("    wideDynamicRangeLevelMax		= %f\n",in_ImageConfigOption->wideDynamicRangeLevelMax);
	printf("    whiteBalanceMode			= %u\n",in_ImageConfigOption->whiteBalanceMode);
	printf("    whiteBalanceCrGainMin		= %f\n",in_ImageConfigOption->whiteBalanceCrGainMin);
	printf("    whiteBalanceCrGainMax		= %f\n",in_ImageConfigOption->whiteBalanceCrGainMax);
	printf("    whiteBalanceCbGainMin		= %f\n",in_ImageConfigOption->whiteBalanceCbGainMin);
	printf("    whiteBalanceCbGainMax		= %f\n",in_ImageConfigOption->whiteBalanceCbGainMax);
	printf("    imageStabilizationMode		= 0x%x\n",in_ImageConfigOption->imageStabilizationMode);
	printf("    imageStabilizationLevelMin		= %f\n",in_ImageConfigOption->imageStabilizationLevelMin);
	printf("    imageStabilizationLevelMax		= %f\n",in_ImageConfigOption->imageStabilizationLevelMax);
	printf("    flickerControl			= %u\n",in_ImageConfigOption->flickerControl);
	printf("    analogTVOutputStandard		= 0x%x\n",in_ImageConfigOption->analogTVOutputStandard);
	printf("    deNoiseMode				= 0x%x\n",in_ImageConfigOption->deNoiseMode);
	printf("    deNoiseStrengthMin			= %f\n",in_ImageConfigOption->deNoiseStrengthMin);
	printf("    deNoiseStrengthMax			= %f\n",in_ImageConfigOption->deNoiseStrengthMax);
	printf("    backLightControlMode		= 0x%x\n",in_ImageConfigOption->backLightControlMode);
	printf("    backLightControlStrengthMin		= %f\n",in_ImageConfigOption->backLightControlStrengthMin);
	printf("    backLightControlStrengthMax		= %f\n",in_ImageConfigOption->backLightControlStrengthMax);
	printf("=== Cmd=0x%04x: User_getOptionsReply ===\n\n", CMD_IMG_GetOptionsOutput);
#endif
}

void User_getUserDefinedSettingsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	UserDefinedSettings* in_UserDefinedSettings = &deviceInfo->userDefinedSettings;

	printf("=== Cmd=0x%04x: User_getUserDefinedSettingsReply ===\n\n", CMD_GetUserDefinedSettingsInput);
	printf("    uerDefinedData 	stringLength	= %u\n",in_UserDefinedSettings->uerDefinedData.stringLength);
	printf("    uerDefinedData 	stringData	= %s\n",in_UserDefinedSettings->uerDefinedData.stringData);
	printf("=== Cmd=0x%04x: User_getUserDefinedSettingsReply ===\n\n", CMD_GetUserDefinedSettingsInput);
#endif
}
//-----------------------Imaging Service-------------------------

//-------------------------Media Service-------------------------
void User_getVideoPrivateAreaReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	VideoPrivateArea* in_VideoPrivateArea = &deviceInfo->videoPrivateArea;
	Byte i = 0;

	printf("=== Cmd=0x%04x: User_getVideoPrivateAreaReply ===\n\n", CMD_GetVideoPrivateAreaOutput);
	printf("    polygonListSize	= %u\n",in_VideoPrivateArea->polygonListSize);
	for( i = 0; i < in_VideoPrivateArea->polygonListSize; i ++)
	{
		printf("---------------------------------- Index %u -------------------------------------\n",i);
		printf("    polygon_x		= %u\n",in_VideoPrivateArea->privateAreaPolygon[i].polygon_x);
		printf("    polygon_y		= %u\n",in_VideoPrivateArea->privateAreaPolygon[i].polygon_y);
		printf("---------------------------------- Index %u -------------------------------------\n",i);
	}
	printf("    privateAreaEnable	= %u\n",in_VideoPrivateArea->privateAreaEnable);
	printf("=== Cmd=0x%04x: User_getVideoPrivateAreaReply ===\n\n", CMD_GetVideoPrivateAreaOutput);
#endif
}

void User_getVideoOSDConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	VideoOSDConfig *in_VideoOSDConfig = &deviceInfo->videoOSDConfig;

	printf("=== Cmd=0x%04x: User_getVideoOSDConfigReply ===\n\n", CMD_GetVideoOSDConfigurationOutput);
	printf("    dateEnable			= %u\n",in_VideoOSDConfig->dateEnable);
	printf("    datePosition		= %u\n",in_VideoOSDConfig->datePosition);
	printf("    dateFormat			= %u\n",in_VideoOSDConfig->dateFormat);
	printf("    timeEnable			= %u\n",in_VideoOSDConfig->timeEnable);
	printf("    timePosition		= %u\n",in_VideoOSDConfig->timePosition);
	printf("    timeFormat			= %u\n",in_VideoOSDConfig->timeFormat);
	printf("    logoEnable			= %u\n",in_VideoOSDConfig->logoEnable);
	printf("    logoPosition		= %u\n",in_VideoOSDConfig->logoPosition);
	printf("    logooption			= %u\n",in_VideoOSDConfig->logooption);
	printf("    detailInfoEnable		= %u\n",in_VideoOSDConfig->detailInfoEnable);
	printf("    detailInfoPosition		= %u\n",in_VideoOSDConfig->detailInfoPosition);
	printf("    detailInfoOption		= %u\n",in_VideoOSDConfig->detailInfoOption);
	printf("    textEnable			= %u\n",in_VideoOSDConfig->textEnable);
	printf("    textPosition		= %u\n",in_VideoOSDConfig->textPosition);
	printf("    text stringLength		= %u\n",in_VideoOSDConfig->text.stringLength);
	printf("    text stringData		= %s\n",in_VideoOSDConfig->text.stringData);
	printf("=== Cmd=0x%04x: User_getVideoOSDConfigReply ===\n\n", CMD_GetVideoOSDConfigurationOutput);
#endif
}

void User_getAudioEncConfigOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	AudioEncConfigOptions* in_AudioEncConfigOptions = &deviceInfo->audioEncConfigOptions;
	Byte i = 0, j = 0;

	printf("=== Cmd=0x%04x: User_getAudioEncConfigOptionsReply ===\n\n", CMD_GetAudioEncoderConfigurationOptionsOutput);

	printf("    configListSize			= %u\n",in_AudioEncConfigOptions->configListSize);
	for( i = 0; i < in_AudioEncConfigOptions->configListSize; i ++)
	{
		printf("---------------------------------- Index %u -------------------------------------\n",i);
		printf("    encodingOption			= %u\n",in_AudioEncConfigOptions->audioEncConfigOptionsParam[i].encodingOption);
		printf("    bitrateRangeMin			= %u\n",in_AudioEncConfigOptions->audioEncConfigOptionsParam[i].bitrateRangeMin);
		printf("    bitrateRangeMax			= %u\n",in_AudioEncConfigOptions->audioEncConfigOptionsParam[i].bitrateRangeMax);
		printf("    sampleRateAvailableListSize		= %u\n",in_AudioEncConfigOptions->audioEncConfigOptionsParam[i].sampleRateAvailableListSize);
		for( j = 0; j < in_AudioEncConfigOptions->audioEncConfigOptionsParam[i].sampleRateAvailableListSize; j ++)
			printf("    sampleRateAvailableList[%u]		= %u\n", j, in_AudioEncConfigOptions->audioEncConfigOptionsParam[i].sampleRateAvailableList[j]);
		printf("---------------------------------- Index %u -------------------------------------\n",i);
	}
	printf("=== Cmd=0x%04x: User_getAudioEncConfigOptionsReply ===\n\n", CMD_GetAudioEncoderConfigurationOptionsOutput);
#endif
}

void User_getAudioSrcConfigOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	AudioSrcConfigOptions* in_AudioSrcConfigOptions = &deviceInfo->audioSrcConfigOptions;
	Byte i = 0;

	printf("=== Cmd=0x%04x: User_getAudioSrcConfigOptionsReply ===\n\n", CMD_GetAudioSourceConfigurationOptionsOutput);
	printf("    audioSrcTokensAvailableListSize			= %u\n",in_AudioSrcConfigOptions->audioSrcTokensAvailableListSize);
	for( i = 0; i < in_AudioSrcConfigOptions->audioSrcTokensAvailableListSize; i ++)
	{
		printf("    audioSrcTokensAvailableList[%u] stringLength		= %u\n", i, in_AudioSrcConfigOptions->audioSrcTokensAvailableList[i].stringLength);
		printf("    audioSrcTokensAvailableList[%u] stringData		= %s\n", i, in_AudioSrcConfigOptions->audioSrcTokensAvailableList[i].stringData);
	}
	printf("=== Cmd=0x%04x: User_getAudioSrcConfigOptionsReply ===\n\n", CMD_GetAudioSourceConfigurationOptionsOutput);
#endif
}

void User_getVideoEncConfigOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	VideoEncConfigOptions* in_VideoEncConfigOptions = &deviceInfo->videoEncConfigOptions;
	Byte i;

	printf("=== Cmd=0x%04x: User_getVideoEncConfigOptionsReply ===\n\n", CMD_GetVideoEncoderConfigurationOptionsOutput);

	printf("    encodingOption			= 0x%x\n",in_VideoEncConfigOptions->encodingOption);
	printf("    resolutionsAvailableListSize	= %u\n",in_VideoEncConfigOptions->resolutionsAvailableListSize);
	for( i = 0; i < in_VideoEncConfigOptions->resolutionsAvailableListSize; i ++)
	{
		printf("    width x height			= %u x %u\n",in_VideoEncConfigOptions->resolutionsAvailableList[i].width, in_VideoEncConfigOptions->resolutionsAvailableList[i].height);
	}
	printf("    qualityMin				= %u\n",in_VideoEncConfigOptions->qualityMin);
	printf("    qualityMax				= %u\n",in_VideoEncConfigOptions->qualityMax);
	printf("    frameRateMin			= %u\n",in_VideoEncConfigOptions->frameRateMin);
	printf("    frameRateMax			= %u\n",in_VideoEncConfigOptions->frameRateMax);
	printf("    encodingIntervalMin			= %u\n",in_VideoEncConfigOptions->encodingIntervalMin);
	printf("    encodingIntervalMax			= %u\n",in_VideoEncConfigOptions->encodingIntervalMax);
	printf("    bitrateRangeMin			= %u\n",in_VideoEncConfigOptions->bitrateRangeMin);
	printf("    bitrateRangeMax			= %u\n",in_VideoEncConfigOptions->bitrateRangeMax);
	printf("    rateControlTypeOptions		= 0x%x\n",in_VideoEncConfigOptions->rateControlTypeOptions);
	printf("    govLengthRangeMin			= %u\n",in_VideoEncConfigOptions->govLengthRangeMin);
	printf("    govLengthRangeMax			= %u\n",in_VideoEncConfigOptions->govLengthRangeMax);
	printf("    profileOptions			= 0x%x\n",in_VideoEncConfigOptions->profileOptions);
	printf("    targetBitrateRangeMin		= %u\n",in_VideoEncConfigOptions->targetBitrateRangeMin);
	printf("    targetBitrateRangeMax		= %u\n",in_VideoEncConfigOptions->targetBitrateRangeMax);
	printf("    aspectRatioAvailableListSize	= %u\n",in_VideoEncConfigOptions->aspectRatioAvailableListSize);
	for( i = 0; i < in_VideoEncConfigOptions->aspectRatioAvailableListSize; i ++)
	{
		printf("    aspectRatioList[%u]			= 0x%04x\n", i ,in_VideoEncConfigOptions->aspectRatioList[i]);
	}

	printf("=== Cmd=0x%04x: User_getVideoEncConfigOptionsReply ===\n\n", CMD_GetVideoEncoderConfigurationOptionsOutput);
#endif
}

void User_getVideoSrcConfigOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	VideoSrcConfigOptions* in_VideoSrcConfigOptions = &deviceInfo->videoSrcConfigOptions;
	Byte i;

	printf("=== Cmd=0x%04x: User_getVideoSrcConfigOptionsReply ===\n\n", CMD_GetVideoSourceConfigurationOptionsOutput);

	printf("    boundsRange_X_Min					= %u\n",in_VideoSrcConfigOptions->boundsRange_X_Min);
	printf("    boundsRange_X_Max					= %u\n",in_VideoSrcConfigOptions->boundsRange_X_Max);
	printf("    boundsRange_Y_Min					= %u\n",in_VideoSrcConfigOptions->boundsRange_Y_Min);
	printf("    boundsRange_Y_Max					= %u\n",in_VideoSrcConfigOptions->boundsRange_Y_Max);
	printf("    boundsRange_Width_Min				= %u\n",in_VideoSrcConfigOptions->boundsRange_Width_Min);
	printf("    boundsRange_Width_Max				= %u\n",in_VideoSrcConfigOptions->boundsRange_Width_Max);
	printf("    boundsRange_Height_Min				= %u\n",in_VideoSrcConfigOptions->boundsRange_Height_Min);
	printf("    boundsRange_Heigh_Max				= %u\n",in_VideoSrcConfigOptions->boundsRange_Heigh_Max);
	printf("    videoSrcTokensAvailableListSize			= %u\n",in_VideoSrcConfigOptions->videoSrcTokensAvailableListSize);
	for( i = 0; i < in_VideoSrcConfigOptions->videoSrcTokensAvailableListSize; i ++)
	{
		printf("    videoSrcTokensAvailableList[%u] stringLength		= %u\n", i, in_VideoSrcConfigOptions->videoSrcTokensAvailableList[i].stringLength);
		printf("    videoSrcTokensAvailableList[%u] stringData		= %s\n", i, in_VideoSrcConfigOptions->videoSrcTokensAvailableList[i].stringData);
	}
	printf("    rotateModeOptions					= 0x%x\n",in_VideoSrcConfigOptions->rotateModeOptions);
	printf("    rotateDegreeMinOption				= %u\n",in_VideoSrcConfigOptions->rotateDegreeMinOption);
	printf("    mirrorModeOptions					= 0x%x\n",in_VideoSrcConfigOptions->mirrorModeOptions);
	printf("    maxFrameRateMin					= %u\n",in_VideoSrcConfigOptions->maxFrameRateMin);
	printf("    maxFrameRateMax					= %u\n",in_VideoSrcConfigOptions->maxFrameRateMax);
	printf("=== Cmd=0x%04x: User_getVideoSrcConfigOptionsReply ===\n\n", CMD_GetVideoSourceConfigurationOptionsOutput);
#endif
}

void User_getAudioEncConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	AudioEncConfig* in_AudioEncConfig = &deviceInfo->audioEncConfig;
	Byte i;

	printf("=== Cmd=0x%04x: User_getAudioEncConfigReply ===\n\n", CMD_GetAudioEncoderConfigurationsOutput);
	printf("    configListSize	= %u\n",in_AudioEncConfig->configListSize);

	for( i = 0; i < in_AudioEncConfig->configListSize; i ++)
	{
		printf("---------------------------------- Index %u -------------------------------------\n",i);
		printf("    token stringLength	= %u\n",in_AudioEncConfig->configList[i].token.stringLength);
		printf("    token stringData	= %s\n",in_AudioEncConfig->configList[i].token.stringData);
		printf("    name stringLength	= %u\n",in_AudioEncConfig->configList[i].name.stringLength);
		printf("    name stringData	= %s\n",in_AudioEncConfig->configList[i].name.stringData);
		printf("    useCount		= %u\n",in_AudioEncConfig->configList[i].useCount);
		printf("    encoding		= %u\n",in_AudioEncConfig->configList[i].encoding);
		printf("    bitrate		= %u\n",in_AudioEncConfig->configList[i].bitrate);
		printf("    sampleRate		= %u\n",in_AudioEncConfig->configList[i].sampleRate);
		printf("---------------------------------- Index %u -------------------------------------\n",i);
	}
	printf("=== Cmd=0x%04x: User_getAudioEncConfigReply ===\n\n", CMD_GetAudioEncoderConfigurationsOutput);
#endif
}

void User_getAudioSourceConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	AudioSrcConfig* in_AudioSrcConfig = &deviceInfo->audioSrcConfig;
	Byte i;

	printf("=== Cmd=0x%04x: User_getAudioSourceConfigReply ===\n\n", CMD_GetAudioSourceConfigurationsOutput);
	printf("    configListSize		= %u\n", in_AudioSrcConfig->configListSize);
	for( i = 0; i < in_AudioSrcConfig->configListSize; i ++)
	{
		printf("----------------------------- Index %u ----------------------------------\n",i);
		printf("    name stringLength		= %u\n", in_AudioSrcConfig->audioSrcConfigList[i].name.stringLength);
		printf("    name stringData		= %s\n",in_AudioSrcConfig->audioSrcConfigList[i].name.stringData);
		printf("    token stringLength		= %u\n",in_AudioSrcConfig->audioSrcConfigList[i].token.stringLength);
		printf("    token stringData		= %s\n",in_AudioSrcConfig->audioSrcConfigList[i].token.stringData);
		printf("    sourceToken stringLength	= %u\n",  in_AudioSrcConfig->audioSrcConfigList[i].sourceToken.stringLength);
		printf("    sourceToken stringData	= %s\n",in_AudioSrcConfig->audioSrcConfigList[i].sourceToken.stringData);
		printf("    useCount			= %u\n",in_AudioSrcConfig->audioSrcConfigList[i].useCount);
		printf("----------------------------- Index %u ----------------------------------\n",i);
	}
	printf("=== Cmd=0x%04x: User_getAudioSourceConfigReply ===\n\n", CMD_GetAudioSourceConfigurationsOutput);
#endif
}

void User_getAudioSourcesReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	AudioSources* in_AudioSources = &deviceInfo->audioSources;
	Byte i;

	printf("=== Cmd=0x%04x: User_getAudioSourcesReply ===\n\n", CMD_GetAudioSourcesInput);
	printf("    audioSourcesListSize		= %u\n", in_AudioSources->audioSourcesListSize);
	for( i = 0; i < in_AudioSources->audioSourcesListSize; i ++)
	{
		printf("----------------------------- Index %u ----------------------------------\n",i);
		printf("    audioSourcesToken stringLength	= %u\n", in_AudioSources->audioSourcesList[i].audioSourcesToken.stringLength);
		printf("    audioSourcesToken stringData	= %s\n",  in_AudioSources->audioSourcesList[i].audioSourcesToken.stringData);

		printf("    channels				= %u\n",  in_AudioSources->audioSourcesList[i].channels);
		printf("----------------------------- Index %u ----------------------------------\n",i);
	}
	printf("=== Cmd=0x%04x: User_getAudioSourcesReply ===\n\n", CMD_GetAudioSourcesInput);
#endif
}

void User_getGuaranteedEncsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	GuaranteedEncs* in_GuaranteedEncs = &deviceInfo->guaranteedEncs;

	printf("=== Cmd=0x%04x: User_getGuaranteedEncsReply ===\n\n", CMD_GetGuaranteedNumberOfVideoEncoderInstancesOutput);
	printf("    TotallNum	= %u\n", in_GuaranteedEncs->TotallNum);
	printf("    JPEGNum	= %u\n", in_GuaranteedEncs->JPEGNum);
	printf("    H264Num	= %u\n", in_GuaranteedEncs->H264Num);
	printf("    MPEG4Num	= %u\n", in_GuaranteedEncs->MPEG4Num);
	printf("    MPEG2Num	= %u\n", in_GuaranteedEncs->MPEG2Num);
	printf("=== Cmd=0x%04x: User_getGuaranteedEncsReply ===\n\n", CMD_GetGuaranteedNumberOfVideoEncoderInstancesOutput);
#endif
}

void User_getProfilesReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	MediaProfiles* in_MediaProfiles = &deviceInfo->mediaProfiles;
	Byte i;

	printf("=== Cmd=0x%04x: User_getProfilesReply ===\n\n", CMD_GetProfilesOutput);
	printf("    extensionFlag					= %u\n", in_MediaProfiles->extensionFlag);
	printf("    profilesListSize					= %u\n", in_MediaProfiles->profilesListSize);

	for( i = 0; i < in_MediaProfiles->profilesListSize; i ++)
	{
		printf("----------------------------- Index %u ----------------------------------\n",i);
		printf("    name stringLength					= %u\n", in_MediaProfiles->mediaProfilesParam[i].name.stringLength);
		printf("    name stringData					= %s\n", in_MediaProfiles->mediaProfilesParam[i].name.stringData);
		printf("    token stringLength					= %u\n", in_MediaProfiles->mediaProfilesParam[i].token.stringLength);
		printf("    token stringData					= %s\n", in_MediaProfiles->mediaProfilesParam[i].token.stringData);
		printf("    videoSrcName stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcName.stringLength);
		printf("    videoSrcName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcName.stringData);
		printf("    videoSrcToken stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcToken.stringLength);
		printf("    videoSrcToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcToken.stringData);
		printf("    videoSrcSourceToken stringLength			= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcSourceToken.stringLength);
		printf("    videoSrcSourceToken stringData			= %s\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcSourceToken.stringData);
		printf("    videoSrcUseCount					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcUseCount);
		printf("    videoSrcBounds_x					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcBounds_x);
		printf("    videoSrcBounds_y					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcBounds_y);
		printf("    videoSrcBoundsWidth					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcBoundsWidth);
		printf("    videoSrcBoundsHeight				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcBoundsHeight);
		printf("    videoSrcRotateMode					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcRotateMode);
		printf("    videoSrcRotateDegree				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcRotateDegree);
		printf("    videoSrcMirrorMode					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcMirrorMode);

		printf("    audioSrcName stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcName.stringLength);
		printf("    audioSrcName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcName.stringData);
		printf("    audioSrcToken stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcToken.stringLength);
		printf("    audioSrcToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcToken.stringData);
		printf("    audioSrcSourceToken stringLength			= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcSourceToken.stringLength);
		printf("    audioSrcSourceToken stringData			= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcSourceToken.stringData);
		printf("    audioSrcUseCount					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcUseCount);

		printf("    videoEncName stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncName.stringLength);
		printf("    videoEncName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].videoEncName.stringData);
		printf("    videoEncToken stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncToken.stringLength);
		printf("    videoEncToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].videoEncToken.stringData);
		printf("    videoEncUseCount					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncUseCount);
		printf("    videoEncEncodingMode				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncEncodingMode);
		printf("    videoEncResolutionWidth				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncResolutionWidth);
		printf("    videoEncResolutionHeight				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncResolutionHeight);
		printf("    videoEncQuality					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncQuality);
		printf("    videoEncRateControlFrameRateLimit			= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncRateControlFrameRateLimit);
		printf("    videoEncRateControlEncodingInterval			= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncRateControlEncodingInterval);
		printf("    videoEncRateControlBitrateLimit			= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncRateControlBitrateLimit);
		printf("    videoEncRateControlType				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncRateControlType);
		printf("    videoEncGovLength					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncGovLength);
		printf("    videoEncProfile					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncProfile);

		printf("    audioEncName stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncName.stringLength);
		printf("    audioEncName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioEncName.stringData);
		printf("    audioEncToken stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncToken.stringLength);
		printf("    audioEncToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioEncToken.stringData);
		printf("    audioEncUseCount					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncUseCount);
		printf("    audioEncEncoding					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncEncoding);
		printf("    audioEncBitrate					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncBitrate);
		printf("    audioEncSampleRate					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncSampleRate);

		printf("    audioOutputName stringLength			= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputName.stringLength);
		printf("    audioOutputName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputName.stringData);
		printf("    audioOutputToken stringLength			= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputToken.stringLength);
		printf("    audioOutputToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputToken.stringData);
		printf("    audioOutputOutputToken stringLength			= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputOutputToken.stringLength);
		printf("    audioOutputOutputToken stringData			= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputOutputToken.stringData);
		printf("    audioOutputUseCount					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputUseCount);
		printf("    audioOutputSendPrimacy				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputSendPrimacy);
		printf("    audioOutputOutputLevel				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputOutputLevel);

		printf("    audioDecName stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioDecName.stringLength);
		printf("    audioDecName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioDecName.stringData);
		printf("    audioDecToken stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioDecToken.stringLength);
		printf("    audioDecToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioDecToken.stringData);
		printf("    audioDecUseCount					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioDecUseCount);
		if(in_MediaProfiles->extensionFlag == 1)
		{
			printf("    videoEncConfigRateControlTargetBitrateLimit		= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncConfigRateControlTargetBitrateLimit);
		}else
		{
			printf("    No Extension Parameter!\n");
		}
		printf("----------------------------- Index %u ----------------------------------\n",i);
	}

	printf("=== Cmd=0x%04x: User_getProfilesReply ===\n\n", CMD_GetProfilesOutput);
#endif
}

void User_getVideoEncConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	VideoEncConfig* in_VideoEncConfig = &deviceInfo->videoEncConfig;
	Byte i;

	printf("=== Cmd=0x%04x: User_getVideoEncConfigReply ===\n\n", CMD_GetVideoEncoderConfigurationsOutput);
	printf("    extensionFlag		= %u\n", in_VideoEncConfig->extensionFlag);
	printf("    configListSize		= %u\n", in_VideoEncConfig->configListSize);
	for( i = 0; i < in_VideoEncConfig->configListSize; i ++)
	{
		printf("---------------------------------- Index %u -------------------------------------\n",i);
		printf("    name stringLength		= %u\n", in_VideoEncConfig->configList[i].name.stringLength);
		printf("    name stringData		= %s\n", in_VideoEncConfig->configList[i].name.stringData);
		printf("    token stringLength		= %u\n", in_VideoEncConfig->configList[i].token.stringLength);
		printf("    token stringData		= %s\n", in_VideoEncConfig->configList[i].token.stringData);
		printf("    useCount			= %u\n", in_VideoEncConfig->configList[i].useCount);
		printf("    encoding			= %u\n", in_VideoEncConfig->configList[i].encoding);
		printf("    width			= %u\n", in_VideoEncConfig->configList[i].width);
		printf("    height			= %u\n", in_VideoEncConfig->configList[i].height);
		printf("    quality			= %u\n", in_VideoEncConfig->configList[i].quality);
		printf("    frameRateLimit		= %u\n", in_VideoEncConfig->configList[i].frameRateLimit);
		printf("    encodingInterval		= %u\n", in_VideoEncConfig->configList[i].encodingInterval);
		printf("    bitrateLimit		= %u\n", in_VideoEncConfig->configList[i].bitrateLimit);
		printf("    rateControlType		= %u\n", in_VideoEncConfig->configList[i].rateControlType);
		printf("    govLength			= %u\n", in_VideoEncConfig->configList[i].govLength);
		printf("    profile			= %u\n", in_VideoEncConfig->configList[i].profile);
		if(in_VideoEncConfig->extensionFlag == 1)
		{
			printf("    targetBitrateLimit		= %u\n", in_VideoEncConfig->configList[i].targetBitrateLimit);
			printf("    aspectRatio			= 0x%04x\n", in_VideoEncConfig->configList[i].aspectRatio);
		}else
		{
			printf("    No Extension Parameter!\n");
		}
		printf("---------------------------------- Index %u -------------------------------------\n",i);
	}

	printf("=== Cmd=0x%04x: User_getVideoEncConfigReply ===\n\n", CMD_GetVideoEncoderConfigurationsOutput);
#endif
}

void User_getVideoSrcConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	VideoSrcConfig* in_VideoSrcConfig = &deviceInfo->videoSrcConfig;
	Byte i;

	printf("=== Cmd=0x%04x: User_getVideoSrcConfigReply ===\n\n", CMD_GetVideoSourceConfigurationsOutput);
	printf("    extensionFlag		= %u\n",in_VideoSrcConfig->extensionFlag);
	printf("    configListSize		= %u\n",in_VideoSrcConfig->configListSize);
	for( i = 0; i < in_VideoSrcConfig->configListSize; i ++)
	{
		printf("---------------------------------- Index %u -------------------------------------\n", i);
		printf("    name stringLength		= %u\n",in_VideoSrcConfig->configList[i].name.stringLength);
		printf("    name stringData		= %s\n",in_VideoSrcConfig->configList[i].name.stringData);
		printf("    token stringLength		= %u\n",in_VideoSrcConfig->configList[i].token.stringLength);
		printf("    token stringData		= %s\n",in_VideoSrcConfig->configList[i].token.stringData);
		printf("    srcToken stringLength	= %u\n",in_VideoSrcConfig->configList[i].srcToken.stringLength);
		printf("    srcToken stringData		= %s\n",in_VideoSrcConfig->configList[i].srcToken.stringData);
		printf("    useCount			= %u\n",in_VideoSrcConfig->configList[i].useCount);
		printf("    bounds_x			= %u\n",in_VideoSrcConfig->configList[i].bounds_x);
		printf("    bounds_y			= %u\n",in_VideoSrcConfig->configList[i].bounds_y);
		printf("    boundsWidth			= %u\n",in_VideoSrcConfig->configList[i].boundsWidth);
		printf("    boundsHeight		= %u\n",in_VideoSrcConfig->configList[i].boundsHeight);
		printf("    rotateMode			= %u\n",in_VideoSrcConfig->configList[i].rotateMode);
		printf("    rotateDegree		= %u\n",in_VideoSrcConfig->configList[i].rotateDegree);
		printf("    mirrorMode			= %u\n",in_VideoSrcConfig->configList[i].mirrorMode);
		if(in_VideoSrcConfig->extensionFlag == 1)
		{
			printf("    maxFrameRate		= %u\n",in_VideoSrcConfig->configList[i].maxFrameRate);
		}else
		{
			printf("    No Extension Parameter!\n");
		}
		printf("---------------------------------- Index %u -------------------------------------\n", i);
	}
	printf("=== Cmd=0x%04x: User_getVideoSrcConfigReply ===\n\n", CMD_GetVideoSourceConfigurationsOutput);
#endif
}

void User_getVideoSrcReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	VideoSrc* in_VideoSrc = &deviceInfo->videoSrc;
	Byte i;

	printf("=== Cmd=0x%04x: User_getVideoSrcReply ===\n\n", CMD_GetVideoSourcesOutput);
	printf("    videoSrcListSize	= %u\n",in_VideoSrc->videoSrcListSize);
	for( i = 0; i < in_VideoSrc->videoSrcListSize; i ++)
	{
		printf("---------------------------------- Index %u -------------------------------------\n", i);
		printf("    token stringLength	= %u\n",in_VideoSrc->srcList[i].token.stringLength);
		printf("    token stringData	= %s\n",in_VideoSrc->srcList[i].token.stringData);
		printf("    frameRate		= %u\n",in_VideoSrc->srcList[i].frameRate);
		printf("    resolutionWidth	= %u\n",in_VideoSrc->srcList[i].resolutionWidth);
		printf("    resolutionHeight	= %u\n",in_VideoSrc->srcList[i].resolutionHeight);
		printf("---------------------------------- Index %u -------------------------------------\n", i);
	}
	printf("=== Cmd=0x%04x: User_getVideoSrcReply ===\n\n", CMD_GetVideoSourcesOutput);
#endif
}
//-------------------------Media Service-------------------------

//--------------------Video Analytics Service--------------------
void User_getSupportedRulesReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	SupportedRules* in_SupportedRules = &deviceInfo->supportedRules;
	Byte i;

	printf("=== Cmd=0x%04x: User_getSupportedRulesReply ===\n\n", CMD_GetSupportedRulesOutput);
	printf("    supportListSize	= %u\n",in_SupportedRules->supportListSize);
	for( i = 0; i < in_SupportedRules->supportListSize; i ++)
	{
		printf("    ruleType		= 0x%02x\n",in_SupportedRules->ruleType[i]);
	}
	printf("=== Cmd=0x%04x: User_getSupportedRulesReply ===\n\n", CMD_GetSupportedRulesOutput);
#endif
}

void User_getRulesReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	RuleList* in_RuleList = &deviceInfo->ruleList;
	Byte i,j;
	Rule_LineDetector* ptrRule_LineDetector = NULL;
	Rule_FieldDetector* ptrRule_FieldDetector = NULL;
	Rule_MotionDetector* ptrRule_MotionDetector = NULL;
	Rule_Counting* ptrRule_Counting = NULL;
	Rule_CellMotion* ptrRule_CellMotion = NULL;

	printf("=== Cmd=0x%04x: User_getRulesReply ===\n\n", CMD_GetRulesOutput);
	printf("    ruleListSize			= %u\n",deviceInfo->ruleList.ruleListSize);
	printf("    extensionFlag			= %u\n",deviceInfo->ruleList.extensionFlag);
	for( i = 0; i < in_RuleList->ruleListSize; i ++)
	{
		printf("--------------------rule %u--------------------\n", i );
		printf("    ruleName stringLength	= %u\n",in_RuleList->totalRuleList[i].ruleName.stringLength);
		printf("    ruleName stringData		= %s\n",in_RuleList->totalRuleList[i].ruleName.stringData);
		printf("    ruleToken stringLength	= %u\n",in_RuleList->totalRuleList[i].ruleToken.stringLength);
		printf("    ruleToken stringData	= %s\n",in_RuleList->totalRuleList[i].ruleToken.stringData);
		printf("    type			= 0x%02x\n",in_RuleList->totalRuleList[i].type);

		if(deviceInfo->ruleList.extensionFlag == 1)
		{
			printf("    videoSrcToken stringLength	= %u\n",in_RuleList->totalRuleList[i].videoSrcToken.stringLength);
			printf("    videoSrcToken stringData	= %s\n",in_RuleList->totalRuleList[i].videoSrcToken.stringData);
			printf("    threshold			= %u\n",in_RuleList->totalRuleList[i].threshold);
			printf("    motionSensitivity		= %u\n",in_RuleList->totalRuleList[i].motionSensitivity);
		}else
		{
			printf("    No Extension Parameter!\n");
		}

		if(in_RuleList->totalRuleList[i].type == 0x10)
		{
			ptrRule_LineDetector = (Rule_LineDetector*) in_RuleList->totalRuleList[i].rule;
			printf("**************Line  Detector**************\n" );
			printf("    direction			= %u\n",ptrRule_LineDetector->direction);
			printf("    polygonListSize		= %u\n",ptrRule_LineDetector->polygonListSize);
			for(j = 0; j < ptrRule_LineDetector->polygonListSize; j ++)
			{
				printf("    polygon_x			= %u\n",ptrRule_LineDetector->detectPolygon[j].polygon_x);
				printf("    polygon_y			= %u\n",ptrRule_LineDetector->detectPolygon[j].polygon_y);
			}
			printf("    metadataStreamSwitch	= %u\n",ptrRule_LineDetector->metadataStreamSwitch);
			printf("**************Line Detector**************\n" );
			ptrRule_LineDetector = NULL;
		}else if(in_RuleList->totalRuleList[i].type == 0x11)
		{
			ptrRule_FieldDetector = (Rule_FieldDetector*) in_RuleList->totalRuleList[i].rule;
			printf("**************Field  Detector**************\n" );
			printf("    polygonListSize		= %u\n",ptrRule_FieldDetector->polygonListSize);
			for(j = 0; j < ptrRule_FieldDetector->polygonListSize; j ++)
			{
				printf("    polygon_x			= %u\n",ptrRule_FieldDetector->detectPolygon[j].polygon_x);
				printf("    polygon_y			= %u\n",ptrRule_FieldDetector->detectPolygon[j].polygon_y);
			}
			printf("    metadataStreamSwitch	= %u\n",ptrRule_FieldDetector->metadataStreamSwitch);
			printf("**************Field Detector**************\n" );
			ptrRule_FieldDetector = NULL;
		}else if(in_RuleList->totalRuleList[i].type == 0x12)
		{
			ptrRule_MotionDetector = (Rule_MotionDetector*) in_RuleList->totalRuleList[i].rule;
			printf("**************Motion Detector**************\n" );
			printf("    motionExpression		= %u\n",ptrRule_MotionDetector->motionExpression);
			printf("    polygonListSize		= %u\n",ptrRule_MotionDetector->polygonListSize);
			for(j = 0; j < ptrRule_MotionDetector->polygonListSize; j ++)
			{
				printf("    polygon_x			= %u\n",ptrRule_MotionDetector->detectPolygon[j].polygon_x);
				printf("    polygon_y			= %u\n",ptrRule_MotionDetector->detectPolygon[j].polygon_y);
			}
			printf("    metadataStreamSwitch	= %u\n",ptrRule_MotionDetector->metadataStreamSwitch);
			printf("**************Motion Detector**************\n" );
			ptrRule_MotionDetector = NULL;
		}else if(in_RuleList->totalRuleList[i].type == 0x13)
		{
			ptrRule_Counting = (Rule_Counting*) in_RuleList->totalRuleList[i].rule;
			printf("**************Counting**************\n" );
			printf("    reportTimeInterval		= %u\n",ptrRule_Counting->reportTimeInterval);
			printf("    resetTimeInterval		= %u\n",ptrRule_Counting->resetTimeInterval);
			printf("    direction			= %u\n",ptrRule_Counting->direction);
			printf("    polygonListSize		= %u\n",ptrRule_Counting->polygonListSize);
			for(j = 0; j < ptrRule_Counting->polygonListSize; j ++)
			{
				printf("    polygon_x			= %u\n",ptrRule_Counting->detectPolygon[j].polygon_x);
				printf("    polygon_y			= %u\n",ptrRule_Counting->detectPolygon[j].polygon_y);
			}
			printf("    metadataStreamSwitch	= %u\n",ptrRule_Counting->metadataStreamSwitch);
			printf("**************Counting**************\n" );
			ptrRule_Counting = NULL;
		}else if(in_RuleList->totalRuleList[i].type == 0x14)
		{
			ptrRule_CellMotion = (Rule_CellMotion*) in_RuleList->totalRuleList[i].rule;
			printf("**************Cell Motion Detector**************\n" );
			printf("    minCount			= %u\n",ptrRule_CellMotion->minCount);
			printf("    alarmOnDelay		= %u\n",ptrRule_CellMotion->alarmOnDelay);
			printf("    alarmOffDelay		= %u\n",ptrRule_CellMotion->alarmOffDelay);
			printf("    activeCellsSize		= %u\n",ptrRule_CellMotion->activeCellsSize);
			printf("    activeCells stringLength	= %u\n",ptrRule_CellMotion->activeCells.stringLength);
			printf("    activeCells stringData	= \n");
			for( j = 0; j < ptrRule_CellMotion->activeCells.stringLength; j ++)
				printf("    [%u] : 0x%02x\n", j , ptrRule_CellMotion->activeCells.stringData[j]);
			printf("    sensitivity			= %u\n",ptrRule_CellMotion->sensitivity);
			printf("    layoutBounds_x		= %u\n",ptrRule_CellMotion->layoutBounds_x);
			printf("    layoutBounds_y		= %u\n",ptrRule_CellMotion->layoutBounds_y);
			printf("    layoutBounds_width		= %u\n",ptrRule_CellMotion->layoutBounds_width);
			printf("    layoutBounds_height		= %u\n",ptrRule_CellMotion->layoutBounds_height);
			printf("    layoutColumns		= %u\n",ptrRule_CellMotion->layoutColumns);
			printf("    layoutRows			= %u\n",ptrRule_CellMotion->layoutRows);
			printf("    metadataStreamSwitch	= %u\n",ptrRule_CellMotion->metadataStreamSwitch);
			printf("**************Cell Motion Detector**************\n" );
			ptrRule_CellMotion = NULL;
		}
		printf("--------------------rule %u--------------------\n", i );
	}

	printf("=== Cmd=0x%04x: User_getRulesReply ===\n\n", CMD_GetRulesOutput);
#endif
}
//--------------------Video Analytics Service--------------------

//--------------------------PTZ Service--------------------------
void User_getPTZConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	PTZConfig* in_PTZConfig = &deviceInfo->ptzConfig;
	Byte i;

	printf("=== Cmd=0x%04x: User_getPTZConfigReply ===\n\n", CMD_GetConfigurationsOutput);
	printf("    extensionFlag	= %u\n",in_PTZConfig->extensionFlag);
	printf("    PTZConfigListSize	= %u\n",in_PTZConfig->PTZConfigListSize);

	for( i = 0; i < in_PTZConfig->PTZConfigListSize; i ++)
	{
		printf("------------------------------------ Index %u --------------------------------------\n", i);
		printf("    name stringLength		= %u\n",in_PTZConfig->PTZConfigList[i].name.stringLength);
		printf("    name stringData		= %s\n",in_PTZConfig->PTZConfigList[i].name.stringData);
		printf("    useCount			= %u\n",in_PTZConfig->PTZConfigList[i].useCount);
		printf("    token stringLength		= %u\n",in_PTZConfig->PTZConfigList[i].token.stringLength);
		printf("    token stringData		= %s\n",in_PTZConfig->PTZConfigList[i].token.stringData);
		printf("    defaultPanSpeed		= %hd\n",in_PTZConfig->PTZConfigList[i].defaultPanSpeed);
		printf("    defaultTiltSpeed		= %hd\n",in_PTZConfig->PTZConfigList[i].defaultTiltSpeed);
		printf("    defaultZoomSpeed		= %hd\n",in_PTZConfig->PTZConfigList[i].defaultZoomSpeed);
		printf("    defaultTimeout		= %u\n",in_PTZConfig->PTZConfigList[i].defaultTimeout);
		printf("    panLimitMin			= %u\n",in_PTZConfig->PTZConfigList[i].panLimitMin);
		printf("    panLimitMax			= %u\n",in_PTZConfig->PTZConfigList[i].panLimitMax);
		printf("    tiltLimitMin		= %u\n",in_PTZConfig->PTZConfigList[i].tiltLimitMin);
		printf("    tiltLimitMax		= %u\n",in_PTZConfig->PTZConfigList[i].tiltLimitMax);
		printf("    zoomLimitMin		= %u\n",in_PTZConfig->PTZConfigList[i].zoomLimitMin);
		printf("    zoomLimitMax		= %u\n",in_PTZConfig->PTZConfigList[i].zoomLimitMax);
		printf("    eFlipMode			= %u\n",in_PTZConfig->PTZConfigList[i].eFlipMode);
		printf("    reverseMode			= %u\n",in_PTZConfig->PTZConfigList[i].reverseMode);
		if(in_PTZConfig->extensionFlag == 1)
		{
			printf("    videoSrcToken stringLength	= %u\n",in_PTZConfig->PTZConfigList[i].videoSrcToken.stringLength);
			printf("    videoSrcToken stringData	= %s\n",in_PTZConfig->PTZConfigList[i].videoSrcToken.stringData);
		}else
		{
			printf("    No Extension Parameter!\n");
		}
		printf("------------------------------------ Index %u --------------------------------------\n", i);
	}
	printf("=== Cmd=0x%04x: User_getPTZConfigReply ===\n\n", CMD_GetConfigurationsOutput);
#endif
}

void User_getPTZStatusReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	PTZStatus* in_PTZStatus = &deviceInfo->ptzStatus;

	printf("=== Cmd=0x%04x: User_getPTZStatusReply ===\n\n", CMD_PTZ_GetStatusOutput);
	printf("    panPosition		= %hd\n",in_PTZStatus->panPosition);
	printf("    tiltPosition	= %hd\n",in_PTZStatus->tiltPosition);
	printf("    zoomPosition	= %hd\n",in_PTZStatus->zoomPosition);
	printf("    panTiltMoveStatus	= %u\n",in_PTZStatus->panTiltMoveStatus);
	printf("    zoomMoveStatus	= %u\n",in_PTZStatus->zoomMoveStatus);
	printf("    error stringLength	= %u\n",in_PTZStatus->error.stringLength);
	printf("    error stringData	= %s\n",in_PTZStatus->error.stringData);
	printf("    UTCHour		= %u\n",in_PTZStatus->UTCHour);
	printf("    UTCMinute		= %u\n",in_PTZStatus->UTCMinute);
	printf("    UTCSecond		= %u\n",in_PTZStatus->UTCSecond);
	printf("    UTCYear		= %u\n",in_PTZStatus->UTCYear);
	printf("    UTCMonth		= %u\n",in_PTZStatus->UTCMonth);
	printf("    UTCDay		= %u\n",in_PTZStatus->UTCDay);
	printf("=== Cmd=0x%04x: User_getPTZStatusReply ===\n\n", CMD_PTZ_GetStatusOutput);
#endif
}

void User_getPresetsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	PTZPresets* in_PTZPresets = &deviceInfo->ptzPresetsGet;
	Byte i;

	printf("=== Cmd=0x%04x: User_getPresetsReply ===\n\n", CMD_GetPresetsOutput);
	printf("    presetListSize		= %u\n",in_PTZPresets->configListSize);
	for( i = 0 ; i < in_PTZPresets->configListSize; i ++)
	{
		printf("------------------------------------ Index %u --------------------------------------\n", i);
		printf("    presetName stringLength	= %u\n",in_PTZPresets->configList[i].presetName.stringLength);
		printf("    presetName stringData	= %s\n",in_PTZPresets->configList[i].presetName.stringData);
		printf("    presetToken stringLength	= %u\n",in_PTZPresets->configList[i].presetToken.stringLength);
		printf("    presetToken stringData	= %s\n",in_PTZPresets->configList[i].presetToken.stringData);

		printf("    panPosition			= %hd\n",in_PTZPresets->configList[i].panPosition);
		printf("    tiltPosition		= %hd\n",in_PTZPresets->configList[i].tiltPosition);
		printf("    zoomPosition		= %hd\n",in_PTZPresets->configList[i].zoomPosition);
		printf("------------------------------------ Index %u --------------------------------------\n", i);
	}
	printf("=== Cmd=0x%04x: User_getPresetsReply ===\n\n", CMD_GetPresetsOutput);
#endif
}

void User_setPresetReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	PTZPresetsSet* in_PTZPresetsSet = &deviceInfo->ptzPresetsSet;

	printf("=== Cmd=0x%04x: User_setPresetReply ===\n\n", CMD_SetPresetOutput);
	printf("    presetToken stringLength	= %u\n",in_PTZPresetsSet->presetToken_set.stringLength);
	printf("    presetToken stringData	= %s\n",in_PTZPresetsSet->presetToken_set.stringData);
	printf("=== Cmd=0x%04x: User_setPresetReply ===\n\n", CMD_SetPresetOutput);
#endif
}
//--------------------------PTZ Service--------------------------

//-------------------Metadata Stream Service--------------------
void User_metadataStreamInfoReply(RCHostInfo* /*deviceInfo*/, Word /*command*/)
{
}

void User_metadataStreamDeviceReply(RCHostInfo* /*deviceInfo*/, Word /*command*/)
{
}

void User_metadataStreamLineEventReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	MetadataStreamInfo* in_MetadataStreamInfo = &deviceInfo->metadataStreamInfo;
	Metadata_Event* in_Metadata_Line_Event = &deviceInfo->metadataStreamInfo.metadata_Event;

	printf("\n=== Cmd=0x%04x: User_metadataStreamLineEventReply ===\r\n", CMD_MetadataStreamOutput);
	printf("     type			= 0x%04x\r\n",in_MetadataStreamInfo->type);
	printf("     version			= 0x%04x\r\n",in_MetadataStreamInfo->version);
	printf("     ruleToken stringLength	= %u\r\n",in_Metadata_Line_Event->ruleToken.stringLength);
	printf("     ruleToken stringData	= %s\r\n",in_Metadata_Line_Event->ruleToken.stringData);
	printf("     objectID			= %u\r\n",in_Metadata_Line_Event->objectID);
	printf("     UTCHour			= %u\r\n",in_Metadata_Line_Event->UTCHour);
	printf("     UTCMinute			= %u\r\n",in_Metadata_Line_Event->UTCMinute);
	printf("     UTCSecond			= %u\r\n",in_Metadata_Line_Event->UTCSecond);
	printf("     UTCYear			= %u\r\n",in_Metadata_Line_Event->UTCYear);
	printf("     UTCMonth			= %u\r\n",in_Metadata_Line_Event->UTCMonth);
	printf("     UTCDay			= %u\r\n",in_Metadata_Line_Event->UTCDay);
	printf("     extensionFlag		= %u\r\n",in_Metadata_Line_Event->extensionFlag);
	if(in_Metadata_Line_Event->extensionFlag == 1)
	{
		printf("     videoSrcToken stringLength	= %u\n",in_Metadata_Line_Event->videoSrcToken.stringLength);
		printf("     videoSrcToken stringData	= %s\n",in_Metadata_Line_Event->videoSrcToken.stringData);
	}else
	{
		printf("    No Extension Parameter!\n");
	}
	printf("=== Cmd=0x%04x: User_metadataStreamLineEventReply ===\r\n", CMD_MetadataStreamOutput);
#endif
}

void User_metadataStreamFieldEventReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	MetadataStreamInfo* in_MetadataStreamInfo = &deviceInfo->metadataStreamInfo;
	Metadata_Event* in_Metadata_Field_Event = &deviceInfo->metadataStreamInfo.metadata_Event;

	printf("\n=== Cmd=0x%04x: User_metadataStreamFieldEventReply ===\r\n", CMD_MetadataStreamOutput);
	printf("     type			= 0x%04x\r\n",in_MetadataStreamInfo->type);
	printf("     version			= 0x%04x\r\n",in_MetadataStreamInfo->version);
	printf("     ruleToken stringLength	= %u\r\n",in_Metadata_Field_Event->ruleToken.stringLength);
	printf("     ruleToken stringData	= %s\r\n",in_Metadata_Field_Event->ruleToken.stringData);
	printf("     objectID			= %u\r\n",in_Metadata_Field_Event->objectID);
	printf("     IsInside			= %u\r\n",in_Metadata_Field_Event->IsInside);
	printf("     UTCHour			= %u\r\n",in_Metadata_Field_Event->UTCHour);
	printf("     UTCMinute			= %u\r\n",in_Metadata_Field_Event->UTCMinute);
	printf("     UTCSecond			= %u\r\n",in_Metadata_Field_Event->UTCSecond);
	printf("     UTCYear			= %u\r\n",in_Metadata_Field_Event->UTCYear);
	printf("     UTCMonth			= %u\r\n",in_Metadata_Field_Event->UTCMonth);
	printf("     UTCDay			= %u\r\n",in_Metadata_Field_Event->UTCDay);
	printf("     extensionFlag		= %u\r\n",in_Metadata_Field_Event->extensionFlag);
	if(in_Metadata_Field_Event->extensionFlag == 1)
	{
		printf("     videoSrcToken stringLength	= %u\n",in_Metadata_Field_Event->videoSrcToken.stringLength);
		printf("     videoSrcToken stringData	= %s\n",in_Metadata_Field_Event->videoSrcToken.stringData);
	}else
	{
		printf("    No Extension Parameter!\n");
	}
	printf("=== Cmd=0x%04x: User_metadataStreamFieldEventReply ===\r\n", CMD_MetadataStreamOutput);
#endif
}

void User_metadataStreamMotionEventReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	MetadataStreamInfo* in_MetadataStreamInfo = &deviceInfo->metadataStreamInfo;
	Metadata_Event* in_Metadata_Motion_Event = &deviceInfo->metadataStreamInfo.metadata_Event;

	printf("\n=== Cmd=0x%04x: User_metadataStreamMotionEventReply ===\r\n", CMD_MetadataStreamOutput);
	printf("     type			= 0x%04x\r\n",in_MetadataStreamInfo->type);
	printf("     version			= 0x%04x\r\n",in_MetadataStreamInfo->version);
	printf("     ruleToken stringLength	= %u\r\n",in_Metadata_Motion_Event->ruleToken.stringLength);
	printf("     ruleToken stringData	= %s\r\n",in_Metadata_Motion_Event->ruleToken.stringData);
	printf("     objectID			= %u\r\n",in_Metadata_Motion_Event->objectID);
	printf("     UTCHour			= %u\r\n",in_Metadata_Motion_Event->UTCHour);
	printf("     UTCMinute			= %u\r\n",in_Metadata_Motion_Event->UTCMinute);
	printf("     UTCSecond			= %u\r\n",in_Metadata_Motion_Event->UTCSecond);
	printf("     UTCYear			= %u\r\n",in_Metadata_Motion_Event->UTCYear);
	printf("     UTCMonth			= %u\r\n",in_Metadata_Motion_Event->UTCMonth);
	printf("     UTCDay			= %u\r\n",in_Metadata_Motion_Event->UTCDay);
	printf("     extensionFlag		= %u\r\n",in_Metadata_Motion_Event->extensionFlag);
	if(in_Metadata_Motion_Event->extensionFlag == 1)
	{
		printf("     videoSrcToken stringLength	= %u\n",in_Metadata_Motion_Event->videoSrcToken.stringLength);
		printf("     videoSrcToken stringData	= %s\n",in_Metadata_Motion_Event->videoSrcToken.stringData);
	}else
	{
		printf("    No Extension Parameter!\n");
	}
	printf("=== Cmd=0x%04x: User_metadataStreamMotionEventReply ===\r\n", CMD_MetadataStreamOutput);
#endif
}

void User_metadataStreamCountingEventReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	MetadataStreamInfo* in_MetadataStreamInfo = &deviceInfo->metadataStreamInfo;
	Metadata_Event* in_Metadata_Counting_Event = &deviceInfo->metadataStreamInfo.metadata_Event;

	printf("\n=== Cmd=0x%04x: User_metadataStreamCountingEventReply ===\r\n", CMD_MetadataStreamOutput);
	printf("     type			= 0x%04x\r\n",in_MetadataStreamInfo->type);
	printf("     version			= 0x%04x\r\n",in_MetadataStreamInfo->version);
	printf("     ruleToken stringLength	= %u\r\n",in_Metadata_Counting_Event->ruleToken.stringLength);
	printf("     ruleToken stringData	= %s\r\n",in_Metadata_Counting_Event->ruleToken.stringData);
	printf("     objectID			= %u\r\n",in_Metadata_Counting_Event->objectID);
	printf("     count			= %u\r\n",in_Metadata_Counting_Event->count);
	printf("     UTCHour			= %u\r\n",in_Metadata_Counting_Event->UTCHour);
	printf("     UTCMinute			= %u\r\n",in_Metadata_Counting_Event->UTCMinute);
	printf("     UTCSecond			= %u\r\n",in_Metadata_Counting_Event->UTCSecond);
	printf("     UTCYear			= %u\r\n",in_Metadata_Counting_Event->UTCYear);
	printf("     UTCMonth			= %u\r\n",in_Metadata_Counting_Event->UTCMonth);
	printf("     UTCDay			= %u\r\n",in_Metadata_Counting_Event->UTCDay);
	printf("     extensionFlag		= %u\r\n",in_Metadata_Counting_Event->extensionFlag);
	if(in_Metadata_Counting_Event->extensionFlag == 1)
	{
		printf("     videoSrcToken stringLength	= %u\n",in_Metadata_Counting_Event->videoSrcToken.stringLength);
		printf("     videoSrcToken stringData	= %s\n",in_Metadata_Counting_Event->videoSrcToken.stringData);
	}else
	{
		printf("    No Extension Parameter!\n");
	}
	printf("=== Cmd=0x%04x: User_metadataStreamCountingEventReply ===\r\n", CMD_MetadataStreamOutput);
#endif
}

void User_metadataStreamCellMotionEventReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	MetadataStreamInfo* in_MetadataStreamInfo = &deviceInfo->metadataStreamInfo;
	Metadata_Event* in_Metadata_CellMotion_Event = &deviceInfo->metadataStreamInfo.metadata_Event;

	printf("\n=== Cmd=0x%04x: User_metadataStreamCellMotionEventReply ===\r\n", CMD_MetadataStreamOutput);
	printf("     type			= 0x%04x\r\n",in_MetadataStreamInfo->type);
	printf("     version			= 0x%04x\r\n",in_MetadataStreamInfo->version);
	printf("     ruleToken stringLength	= %u\r\n",in_Metadata_CellMotion_Event->ruleToken.stringLength);
	printf("     ruleToken stringData	= %s\r\n",in_Metadata_CellMotion_Event->ruleToken.stringData);
	printf("     IsMotion			= %u\r\n",in_Metadata_CellMotion_Event->IsMotion);
	printf("     UTCHour			= %u\r\n",in_Metadata_CellMotion_Event->UTCHour);
	printf("     UTCMinute			= %u\r\n",in_Metadata_CellMotion_Event->UTCMinute);
	printf("     UTCSecond			= %u\r\n",in_Metadata_CellMotion_Event->UTCSecond);
	printf("     UTCYear			= %u\r\n",in_Metadata_CellMotion_Event->UTCYear);
	printf("     UTCMonth			= %u\r\n",in_Metadata_CellMotion_Event->UTCMonth);
	printf("     UTCDay			= %u\r\n",in_Metadata_CellMotion_Event->UTCDay);
	printf("     extensionFlag		= %u\r\n",in_Metadata_CellMotion_Event->extensionFlag);
	if(in_Metadata_CellMotion_Event->extensionFlag == 1)
	{
		printf("     videoSrcToken stringLength	= %u\n",in_Metadata_CellMotion_Event->videoSrcToken.stringLength);
		printf("     videoSrcToken stringData	= %s\n",in_Metadata_CellMotion_Event->videoSrcToken.stringData);
	}else
	{
		printf("    No Extension Parameter!\n");
	}
	printf("=== Cmd=0x%04x: User_metadataStreamCellMotionEventReply ===\r\n", CMD_MetadataStreamOutput);
#endif
}

void User_getMetadataSettingsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
#if	UART_TEST
	MetadataSettings* in_MetadataSettings = &deviceInfo->metadataSettings;

	printf("=== Cmd=0x%04x: User_getMetadataSettingsReply ===\n\n", CMD_GetMetadataSettingsOutput);

	printf("     deviceInfoEnable		= %u\n",in_MetadataSettings->deviceInfoEnable);
	printf("     deviceInfoPeriod		= %u\n",in_MetadataSettings->deviceInfoPeriod);
	printf("     streamInfoEnable		= %u\n",in_MetadataSettings->streamInfoEnable);
	printf("     streamInfoPeriod		= %u\n",in_MetadataSettings->streamInfoPeriod);
	printf("     motionDetectorEnable	= %u\n",in_MetadataSettings->motionDetectorEnable);
	printf("     motionDetectorPeriod	= %u\n",in_MetadataSettings->motionDetectorPeriod);

	printf("=== Cmd=0x%04x: User_getMetadataSettingsReply ===\n\n", CMD_GetMetadataSettingsOutput);
#endif
}

//*******************User Define Reply********************
