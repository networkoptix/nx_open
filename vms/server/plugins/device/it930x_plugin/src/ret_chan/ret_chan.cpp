#include <cstring>

#include "ret_chan.h"

#define RETURN_CHANNEL_BAUDRATE 9600

//

unsigned RCString::readLength(const Byte * buf)
{
    unsigned x = buf[0];
    x <<= 8; x |= buf[1];
    x <<= 8; x |= buf[2];
    x <<= 8; x |= buf[3];
    return x;
}

unsigned RCString::copy(const RCString * srcStr)
{
    return set(srcStr->stringData, srcStr->stringLength);
}

unsigned RCString::set(const Byte * buf, unsigned bufferLength)
{
    clear();

    if (buf && bufferLength)
    {
        stringLength = bufferLength;
        stringData = new Byte[stringLength];
        memcpy(stringData, buf, bufferLength);
    }
    else
        stringLength = 0;

    return ReturnChannelError::NO_ERROR;
}

unsigned RCString::clear()
{
    if (stringData != NULL)
        delete [] stringData;
    stringLength = 0;
    stringData = NULL;

    return ReturnChannelError::NO_ERROR;
}

//

#define RC_USER_DEFINE_INIT_VALUE 0

TxRC::TxRC()
{
    static const RCString strNone((const Byte *)"NONE", 64);
    static const Byte FFx4[4] = {0xFF, 0xFF, 0xFF, 0xFF};

    TxRC * deviceInfo = this;

    Rule_LineDetector* ptrRule_LineDetector = NULL;
    Rule_FieldDetector* ptrRule_FieldDetector = NULL;
    Rule_MotionDetector* ptrRule_MotionDetector = NULL;
    Rule_Counting* ptrRule_Counting = NULL;
    Rule_CellMotion* ptrRule_CellMotion = NULL;

    //---------------------init_security-----------------------
    deviceInfo->security.userName.copy(&strNone);
    deviceInfo->security.password.copy(&strNone);
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
    deviceInfo->hwRegisterInfo.registerValues = new Byte[1];
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
    for(Byte i = 0;i<32;i++)
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
    deviceInfo->serviceConfig.serviceName.copy(&strNone);
    deviceInfo->serviceConfig.provider.copy(&strNone);
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
    deviceInfo->calibrationTable.tableData.copy(&strNone);
    //------------------init_CalibrationTable------------------------
    //------------------init_EITInfo------------------------
    deviceInfo->eitInfo.videoEncConfigToken.copy(&strNone);
    deviceInfo->eitInfo.listSize = 2;
    deviceInfo->eitInfo.eitInfoParam = new EITInfoParam[deviceInfo->eitInfo.listSize];
    for (Byte i = 0; i < deviceInfo->eitInfo.listSize; ++i)
    {
        deviceInfo->eitInfo.eitInfoParam[i].enable = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->eitInfo.eitInfoParam[i].startDate = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->eitInfo.eitInfoParam[i].startTime = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->eitInfo.eitInfoParam[i].duration = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->eitInfo.eitInfoParam[i].eventName.copy(&strNone);
        deviceInfo->eitInfo.eitInfoParam[i].eventText.copy(&strNone);
    }
    //------------------init_EITInfo------------------------
    //-------------------init_DeviceCapability------------------------
    deviceInfo->deviceCapability.supportedFeatures = RC_USER_DEFINE_INIT_VALUE;
    //-------------------init_DeviceCapability------------------------
    //----------------------init_DeviceInfo---------------------------
    deviceInfo->manufactureInfo.manufactureName.copy(&strNone);
    deviceInfo->manufactureInfo.modelName.copy(&strNone);
    deviceInfo->manufactureInfo.firmwareVersion.copy(&strNone);
    deviceInfo->manufactureInfo.serialNumber.copy(&strNone);
    deviceInfo->manufactureInfo.hardwareId.copy(&strNone);
    //----------------------init_DeviceInfo---------------------------
    //-----------------------init_Hostname---------------------------
    deviceInfo->hostInfo.hostName.copy(&strNone);
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
    deviceInfo->systemLog.logData.copy(&strNone);
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
    deviceInfo->osdInfo.logoOption = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->osdInfo.detailInfoEnable = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->osdInfo.detailInfoPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->osdInfo.detailInfoOption = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->osdInfo.textEnable = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->osdInfo.textPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->osdInfo.text.copy(&strNone);
    //------------------------init_OSDInfo---------------------------
    //----------------------init_SystemDefault------------------------
    deviceInfo->systemDefault.factoryDefault = RC_USER_DEFINE_INIT_VALUE;
    //----------------------init_SystemDefault------------------------
    //-----------------init_SystemRebootMessage--------------------
    deviceInfo->systemReboot.rebootType = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->systemReboot.responseMessage.copy(&strNone);
    //-----------------init_SystemRebootMessage--------------------
    //-----------------init_SystemFirmware--------------------
    deviceInfo->systemFirmware.firmwareType = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->systemFirmware.data.copy(&strNone);
    deviceInfo->systemFirmware.message.copy(&strNone);
    //-----------------init_SystemFirmware--------------------
    //----------------------init_DigitalInputs--------------------------
    deviceInfo->digitalInputsInfo.listSize = 2;
    deviceInfo->digitalInputsInfo.tokenList = new RCString[deviceInfo->digitalInputsInfo.listSize];
    for (Byte i = 0; i < deviceInfo->digitalInputsInfo.listSize; ++i)
    {
        deviceInfo->digitalInputsInfo.tokenList[i].copy(&strNone);
    }
    //----------------------init_DigitalInputs--------------------------
    //-----------------------init_RelayOutputs------------------------
    deviceInfo->relayOutputs.listSize = 2;

    deviceInfo->relayOutputs.relayOutputsParam = new RelayOutputsParam[deviceInfo->relayOutputs.listSize];

    for (Byte i = 0; i < deviceInfo->relayOutputs.listSize; ++i)
    {
        deviceInfo->relayOutputs.relayOutputsParam[i].token.copy(&strNone);

        deviceInfo->relayOutputs.relayOutputsParam[i].mode = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->relayOutputs.relayOutputsParam[i].delayTime = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->relayOutputs.relayOutputsParam[i].idleState = RC_USER_DEFINE_INIT_VALUE;
    }
    //-----------------------init_RelayOutputs------------------------
    //------------------init_RelayOutputsSetParam-------------------
    deviceInfo->relayOutputsSetParam.token.copy(&strNone);

    deviceInfo->relayOutputsSetParam.mode = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->relayOutputsSetParam.delayTime = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->relayOutputsSetParam.idleState = RC_USER_DEFINE_INIT_VALUE;
    //------------------init_RelayOutputsSetParam-------------------
    //---------------------init_RelayOutputState----------------------
    deviceInfo->relayOutputState.logicalState = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->relayOutputState.token.copy(&strNone);
    //---------------------init_RelayOutputState----------------------
    //----------------------init_ImagingSettings-----------------------
    deviceInfo->imageConfig.videoSourceToken.copy(&strNone);

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
    deviceInfo->focusStatusInfo.videoSourceToken.copy(&strNone);

    deviceInfo->focusStatusInfo.position = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->focusStatusInfo.moveStatus = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->focusStatusInfo.error.copy(&strNone);
    //--------------------------init_Status----------------------------
    //----------------------init_ImagingSettingsOption-----------------------
    deviceInfo->imageConfigOption.videoSourceToken.copy(&strNone);

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
    deviceInfo->focusMoveInfo.videoSourceToken.copy(&strNone);

    deviceInfo->focusMoveInfo.absolutePosition = (float)RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->focusMoveInfo.absoluteSpeed = (float)RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->focusMoveInfo.relativeDistance = (float)RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->focusMoveInfo.relativeSpeed = (float)RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->focusMoveInfo.continuousSpeed = (float)RC_USER_DEFINE_INIT_VALUE;
    //--------------------------init_Move----------------------------
    //---------------------------init_Stop----------------------------
    deviceInfo->focusStopInfo.videoSourceToken.copy(&strNone);
    //---------------------------init_Stop----------------------------
    //---------------------------init_UserDefinedSettings----------------------------
    deviceInfo->userDefinedSettings.videoSourceToken.copy(&strNone);
    deviceInfo->userDefinedSettings.uerDefinedData.copy(&strNone);
    //---------------------------init_UserDefinedSettings----------------------------
    //--------------------init_AudioEncConfig-----------------------
    deviceInfo->audioEncConfig.configListSize = 1;
    deviceInfo->audioEncConfig.configList = new AudioEncConfigParam[deviceInfo->audioEncConfig.configListSize];

    for (Byte i = 0; i < deviceInfo->audioEncConfig.configListSize; ++i)
    {
        deviceInfo->audioEncConfig.configList[i].token.copy(&strNone);
        deviceInfo->audioEncConfig.configList[i].name.copy(&strNone);
        deviceInfo->audioEncConfig.configList[i].useCount = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->audioEncConfig.configList[i].encoding = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->audioEncConfig.configList[i].bitrate = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->audioEncConfig.configList[i].sampleRate = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->audioEncConfig.configList[i].forcePersistence = RC_USER_DEFINE_INIT_VALUE;
    }
    //--------------------init_AudioEncConfig-----------------------
    //-------------------init_AudioSourceConfig---------------------
    deviceInfo->audioSrcConfig.configListSize = 1;
    deviceInfo->audioSrcConfig.audioSrcConfigList = new AudioSrcConfigParam[deviceInfo->audioSrcConfig.configListSize];
    for (Byte i = 0; i < deviceInfo->audioSrcConfig.configListSize; ++i)
    {
        deviceInfo->audioSrcConfig.audioSrcConfigList[i].name.copy(&strNone);
        deviceInfo->audioSrcConfig.audioSrcConfigList[i].token.copy(&strNone);
        deviceInfo->audioSrcConfig.audioSrcConfigList[i].sourceToken.copy(&strNone);
        deviceInfo->audioSrcConfig.audioSrcConfigList[i].useCount = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->audioSrcConfig.audioSrcConfigList[i].forcePersistence = RC_USER_DEFINE_INIT_VALUE;
    }
    //-------------------init_AudioSourceConfig---------------------
    //----------------------init_AudioSources------------------------
    deviceInfo->audioSources.audioSourcesListSize = 2;
    deviceInfo->audioSources.audioSourcesList = new AudioSourcesParam[deviceInfo->audioSources.audioSourcesListSize];
    for (Byte i = 0; i < deviceInfo->audioSources.audioSourcesListSize; ++i)
    {
        deviceInfo->audioSources.audioSourcesList[i].audioSourcesToken.copy(&strNone);
        deviceInfo->audioSources.audioSourcesList[i].channels = RC_USER_DEFINE_INIT_VALUE;
    }
    //----------------------init_AudioSources------------------------
    //---------------------init_GuaranteedEncs-----------------------
    deviceInfo->guaranteedEncs.configurationToken.copy(&strNone);

    deviceInfo->guaranteedEncs.TotallNum = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->guaranteedEncs.JPEGNum = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->guaranteedEncs.H264Num = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->guaranteedEncs.MPEG4Num = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->guaranteedEncs.MPEG2Num = RC_USER_DEFINE_INIT_VALUE;
    //---------------------init_GuaranteedEncs-----------------------
    //-------------------------init_Profiles----------------------------
    deviceInfo->mediaProfiles.profilesListSize = 2;
    deviceInfo->mediaProfiles.mediaProfilesParam = new MediaProfilesParam[deviceInfo->mediaProfiles.profilesListSize];

    for (Byte i = 0; i < deviceInfo->mediaProfiles.profilesListSize; ++i)
    {
        deviceInfo->mediaProfiles.mediaProfilesParam[i].name.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].token.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcName.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcToken.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcSourceToken.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcUseCount = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBounds_x = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBounds_y = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBoundsWidth = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcBoundsHeight = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcRotateMode = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcRotateDegree = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcMirrorMode = RC_USER_DEFINE_INIT_VALUE;

        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcName.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcToken.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcSourceToken.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcUseCount = RC_USER_DEFINE_INIT_VALUE;

        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncName.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncToken.copy(&strNone);
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

        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncName.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncToken.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncUseCount = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncEncoding = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncBitrate = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncSampleRate = RC_USER_DEFINE_INIT_VALUE;

        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputName.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputToken.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputOutputToken.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputUseCount = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputSendPrimacy = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputOutputLevel = RC_USER_DEFINE_INIT_VALUE;

        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecName.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecToken.copy(&strNone);
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecUseCount = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncConfigRateControlTargetBitrateLimit = RC_USER_DEFINE_INIT_VALUE;
    }
    deviceInfo->mediaProfiles.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
    //-------------------------init_Profiles----------------------------
    //--------------------init_VideoEncConfig------------------------
    deviceInfo->videoEncConfig.configListSize = 1;
    deviceInfo->videoEncConfig.configList = new VideoEncConfigParam[deviceInfo->videoEncConfig.configListSize];
    for (Byte i = 0; i < deviceInfo->videoEncConfig.configListSize; ++i)
    {
        deviceInfo->videoEncConfig.configList[i].name.copy(&strNone);
        deviceInfo->videoEncConfig.configList[i].token.copy(&strNone);
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
    deviceInfo->videoSrcConfig.configList = new VideoSrcConfigParam[deviceInfo->videoSrcConfig.configListSize];
    for (Byte i = 0; i < deviceInfo->videoSrcConfig.configListSize; ++i)
    {
        deviceInfo->videoSrcConfig.configList[i].name.copy(&strNone);
        deviceInfo->videoSrcConfig.configList[i].token.copy(&strNone);
        deviceInfo->videoSrcConfig.configList[i].srcToken.copy(&strNone);
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
    deviceInfo->videoSrc.srcList = new VideoSrcParam[deviceInfo->videoSrc.videoSrcListSize];
    for (Byte i = 0; i < deviceInfo->videoSrc.videoSrcListSize; ++i)
    {
        deviceInfo->videoSrc.srcList[i].token.copy(&strNone);
        deviceInfo->videoSrc.srcList[i].frameRate =RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->videoSrc.srcList[i].resolutionWidth = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->videoSrc.srcList[i].resolutionHeight = RC_USER_DEFINE_INIT_VALUE;
    }
    //------------------------init_VideoSrc---------------------------
    //------------------------init_VideoSrcConfigOptions---------------------------
    deviceInfo->videoSrcConfigOptions.videoSrcConfigToken.copy(&strNone);
    deviceInfo->videoSrcConfigOptions.profileToken.copy(&strNone);

    deviceInfo->videoSrcConfigOptions.boundsRange_X_Min = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoSrcConfigOptions.boundsRange_X_Max = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoSrcConfigOptions.boundsRange_Y_Min = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoSrcConfigOptions.boundsRange_Y_Max = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoSrcConfigOptions.boundsRange_Width_Min = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoSrcConfigOptions.boundsRange_Width_Max = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoSrcConfigOptions.boundsRange_Height_Min = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoSrcConfigOptions.boundsRange_Heigh_Max = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize = 2;
    deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList = new RCString[deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize];
    for (Byte i = 0; i < deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize; ++i)
        deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList[i].copy(&strNone);
    deviceInfo->videoSrcConfigOptions.rotateModeOptions = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoSrcConfigOptions.rotateDegreeMinOption = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoSrcConfigOptions. mirrorModeOptions = RC_USER_DEFINE_INIT_VALUE;

    //------------------------init_VideoSrcConfigOptions---------------------------
    //------------------------init_VideoEncConfigOptions---------------------------
    deviceInfo->videoEncConfigOptions.videoEncConfigToken.copy(&strNone);
    deviceInfo->videoEncConfigOptions.profileToken.copy(&strNone);

    deviceInfo->videoEncConfigOptions.encodingOption = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize = 2;
    deviceInfo->videoEncConfigOptions.resolutionsAvailableList = new ResolutionsAvailableList[deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize];
    for (Byte i = 0; i < deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize; ++i)
    {
        deviceInfo->videoEncConfigOptions.resolutionsAvailableList[i].width = RC_USER_DEFINE_INIT_VALUE + i;
        deviceInfo->videoEncConfigOptions.resolutionsAvailableList[i].height = RC_USER_DEFINE_INIT_VALUE + i;
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
    deviceInfo->videoEncConfigOptions.aspectRatioList = new Word[deviceInfo->videoEncConfigOptions.aspectRatioAvailableListSize];
    for (Byte i = 0; i < deviceInfo->videoEncConfigOptions.aspectRatioAvailableListSize; ++i)
    {
        deviceInfo->videoEncConfigOptions.aspectRatioList[i] = RC_USER_DEFINE_INIT_VALUE;
    }

    //------------------------init_VideoEncConfigOptions---------------------------
    //------------------------init_AudioSrcConfigOptions---------------------------
    deviceInfo->audioSrcConfigOptions.audioSrcConfigToken.copy(&strNone);
    deviceInfo->audioSrcConfigOptions.profileToken.copy(&strNone);
    deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize = 2;
    deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList = new RCString[deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize];
    for (Byte i = 0; i < deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize; ++i)
        deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList[i].copy(&strNone);
    //------------------------init_AudioSrcConfigOptions---------------------------
    //------------------------init_AudioEncConfigOptions---------------------------
    deviceInfo->audioEncConfigOptions.audioEncConfigToken.copy(&strNone);
    deviceInfo->audioEncConfigOptions.profileToken.copy(&strNone);

    deviceInfo->audioEncConfigOptions.configListSize = 2;
    deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam = new AudioEncConfigOptionsParam[deviceInfo->audioEncConfigOptions.configListSize];
    for (Byte i = 0; i < deviceInfo->audioEncConfigOptions.configListSize; ++i)
    {
        deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].encodingOption = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].bitrateRangeMin = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].bitrateRangeMax = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableListSize = 2;
        deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableList = new Word[deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableListSize];
        for (Byte j = 0; j < deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableListSize; ++j)
        {
            deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableList[j] = RC_USER_DEFINE_INIT_VALUE;
        }
    }
    //------------------------init_AudioEncConfigOptions---------------------------
    //----------------init_AudioEncConfigSetParam------------------
    deviceInfo->audioEncConfigSetParam.name.copy(&strNone);
    deviceInfo->audioEncConfigSetParam.token.copy(&strNone);

    deviceInfo->audioEncConfigSetParam.useCount =RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->audioEncConfigSetParam.encoding = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->audioEncConfigSetParam.bitrate = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->audioEncConfigSetParam.sampleRate = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->audioEncConfigSetParam.forcePersistence = RC_USER_DEFINE_INIT_VALUE;
    //----------------init_AudioEncConfigSetParam------------------
    //----------------init_AudioSrcConfigSetParam------------------
    deviceInfo->audioSrcConfigSetParam.name.copy(&strNone);
    deviceInfo->audioSrcConfigSetParam.token.copy(&strNone);
    deviceInfo->audioSrcConfigSetParam.sourceToken.copy(&strNone);
    deviceInfo->audioSrcConfigSetParam.useCount = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->audioSrcConfigSetParam.forcePersistence = RC_USER_DEFINE_INIT_VALUE;
    //----------------init_AudioSrcConfigSetParam------------------
    //----------------init_VideoEncConfigSetParam------------------
    deviceInfo->videoEncConfigSetParam.name.copy(&strNone);
    deviceInfo->videoEncConfigSetParam.token.copy(&strNone);
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
    deviceInfo->videoSrcConfigSetParam.name.copy(&strNone);
    deviceInfo->videoSrcConfigSetParam.token.copy(&strNone);
    deviceInfo->videoSrcConfigSetParam.srcToken.copy(&strNone);

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
    deviceInfo->videoOSDConfig.videoSrcToken.copy(&strNone);
    deviceInfo->videoOSDConfig.dateEnable = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.datePosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.dateFormat = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.timeEnable = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.timePosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.timeFormat = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.logoEnable = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.logoPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.logoOption = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.detailInfoEnable = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.detailInfoPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.detailInfoOption = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.textEnable = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.textPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->videoOSDConfig.text.copy(&strNone);
    //----------------init_VideoOSDConfig------------------
    //------------------------init_VideoPrivateArea--------------------------
    deviceInfo->videoPrivateArea.videoSrcToken.copy(&strNone);
    deviceInfo->videoPrivateArea.polygonListSize = 2;
    deviceInfo->videoPrivateArea.privateAreaPolygon = new PrivateAreaPolygon[deviceInfo->videoPrivateArea.polygonListSize];
    for (Byte i = 0; i < deviceInfo->videoPrivateArea.polygonListSize; ++i)
    {
        deviceInfo->videoPrivateArea.privateAreaPolygon[i].polygon_x = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->videoPrivateArea.privateAreaPolygon[i].polygon_y = RC_USER_DEFINE_INIT_VALUE;
    }
    deviceInfo->videoPrivateArea.privateAreaEnable = RC_USER_DEFINE_INIT_VALUE;
    //------------------------init_VideoPrivateArea--------------------------
    //------------------------init_SyncPoint--------------------------
    deviceInfo->syncPoint.profileToken.copy(&strNone);
    //------------------------init_SyncPoint--------------------------
    //------------------------init_VideoSrcControl--------------------------
    deviceInfo->videoSrcControl.videoSrcToken.copy(&strNone);
    deviceInfo->videoSrcControl.controlCommand =  RC_USER_DEFINE_INIT_VALUE;
    //------------------------init_VideoSrcControl--------------------------
    //------------------------init_PTZConfig--------------------------
    deviceInfo->ptzConfig.PTZConfigListSize = 2;
    deviceInfo->ptzConfig.PTZConfigList = new PTZConfigParam[deviceInfo->ptzConfig.PTZConfigListSize];
    for (Byte i = 0; i < deviceInfo->ptzConfig.PTZConfigListSize; ++i)
    {
        deviceInfo->ptzConfig.PTZConfigList[i].name.copy(&strNone);
        deviceInfo->ptzConfig.PTZConfigList[i].useCount = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->ptzConfig.PTZConfigList[i].token.copy(&strNone);
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
        deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken.copy(&strNone);
    }
    deviceInfo->ptzConfig.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
    //------------------------init_PTZConfig--------------------------
    //------------------------init_PTZStatus--------------------------
    deviceInfo->ptzStatus.token.copy(&strNone);
    deviceInfo->ptzStatus.panPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzStatus.tiltPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzStatus.zoomPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzStatus.panTiltMoveStatus  = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzStatus.zoomMoveStatus = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzStatus.error.copy(&strNone);
    deviceInfo->ptzStatus.UTCHour = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzStatus.UTCMinute = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzStatus.UTCSecond = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzStatus.UTCYear = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzStatus.UTCMonth = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzStatus.UTCDay = RC_USER_DEFINE_INIT_VALUE;
    //------------------------init_PTZStatus--------------------------
    //-----------------------init_PTZPresetsGet--------------------------
    deviceInfo->ptzPresetsGet.token.copy(&strNone);
    deviceInfo->ptzPresetsGet.configListSize = 1;
    deviceInfo->ptzPresetsGet.configList = new PTZPresetsConfig[deviceInfo->ptzPresetsGet.configListSize];
    for (Byte i = 0; i < deviceInfo->ptzPresetsGet.configListSize; ++i)
    {
        deviceInfo->ptzPresetsGet.configList[i].presetName.copy(&strNone);
        deviceInfo->ptzPresetsGet.configList[i].presetToken.copy(&strNone);
        deviceInfo->ptzPresetsGet.configList[i].panPosition = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->ptzPresetsGet.configList[i].tiltPosition = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->ptzPresetsGet.configList[i].zoomPosition = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->ptzPresetsGet.configList[i].panSpeed = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->ptzPresetsGet.configList[i].tiltSpeed = RC_USER_DEFINE_INIT_VALUE;
        deviceInfo->ptzPresetsGet.configList[i].zoomSpeed = RC_USER_DEFINE_INIT_VALUE;
    }
    //-----------------------init_PTZPresetsGet--------------------------
    //-----------------------init_PTZPresetsParam--------------------------
    deviceInfo->ptzPresetsSet.token.copy(&strNone);
    deviceInfo->ptzPresetsSet.presetName.copy(&strNone);
    deviceInfo->ptzPresetsSet.presetToken.copy(&strNone);
    deviceInfo->ptzPresetsSet.presetToken_set.copy(&strNone);
    //-----------------------init_PTZPresetsParam--------------------------
    //-----------------------init_PTZGotoParam----------------------
    deviceInfo->ptzGotoParam.token.copy(&strNone);
    deviceInfo->ptzGotoParam.presetToken.copy(&strNone);
    deviceInfo->ptzGotoParam.panSpeed = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzGotoParam.tiltSpeed = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzGotoParam.zoomSpeed = RC_USER_DEFINE_INIT_VALUE;
    //-----------------------init_PTZGotoParam----------------------
    //---------------------init_PTZRemoveParam---------------------
    deviceInfo->ptzRemoveParam.token.copy(&strNone);
    deviceInfo->ptzRemoveParam.presetToken.copy(&strNone);
    //---------------------init_PTZRemoveParam---------------------
    //---------------------init_PTZAbsoluteMove---------------------
    deviceInfo->ptzAbsoluteMove.token.copy(&strNone);
    deviceInfo->ptzAbsoluteMove.panPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzAbsoluteMove.tiltPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzAbsoluteMove.zoomPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzAbsoluteMove.panSpeed = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzAbsoluteMove.tiltSpeed = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzAbsoluteMove.zoomSpeed = RC_USER_DEFINE_INIT_VALUE;
    //---------------------init_PTZAbsoluteMove---------------------
    //----------------------init_PTZRelativeMove---------------------
    deviceInfo->ptzRelativeMove.token.copy(&strNone);
    deviceInfo->ptzRelativeMove.panTranslation = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzRelativeMove.tiltTranslation = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzRelativeMove.zoomTranslation = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzRelativeMove.panSpeed = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzRelativeMove.tiltSpeed = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzRelativeMove.zoomSpeed = RC_USER_DEFINE_INIT_VALUE;
    //----------------------init_PTZRelativeMove---------------------
    //--------------------init_PTZContinuousMove-------------------
    deviceInfo->ptzContinuousMove.token.copy(&strNone);
    deviceInfo->ptzContinuousMove.panVelocity = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzContinuousMove.tiltVelocity = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzContinuousMove.zoomVelocity = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzContinuousMove.timeout = RC_USER_DEFINE_INIT_VALUE;
    //--------------------init_PTZContinuousMove-------------------
    //---------------------init_PTZHomePosition---------------------
    deviceInfo->ptzHomePosition.token.copy(&strNone);
    deviceInfo->ptzHomePosition.panSpeed = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzHomePosition.tiltSpeed = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzHomePosition.zoomSpeed = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzHomePosition.panPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzHomePosition.tiltPosition = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->ptzHomePosition.zoomPosition = RC_USER_DEFINE_INIT_VALUE;
    //---------------------init_PTZHomePosition---------------------
    //--------------------------init_PTZStop-------------------------
    deviceInfo->ptzStop.token.copy(&strNone);
    deviceInfo->ptzStop.panTiltZoom = RC_USER_DEFINE_INIT_VALUE;
    //--------------------------init_PTZStop-------------------------
    //----------------------init_SupportedRules-----------------------
    deviceInfo->supportedRules.supportListSize = 5;
    deviceInfo->supportedRules.ruleType = new Byte[deviceInfo->supportedRules.supportListSize];
    for (Byte i = 0; i < deviceInfo->supportedRules.supportListSize; ++i)
        deviceInfo->supportedRules.ruleType[i] = i + 0x10;
    //----------------------init_SupportedRules-----------------------
    //------------------------init_DeleteRule--------------------------
    deviceInfo->deleteRule.ruleToken.copy(&strNone);
    //------------------------init_DeleteRule--------------------------
    //-------------------------init_totalRule--------------------------
    deviceInfo->totalRule.ruleName.copy(&strNone);
    deviceInfo->totalRule.ruleToken.copy(&strNone);
    deviceInfo->totalRule.videoSrcToken.copy(&strNone);
    deviceInfo->totalRule.type = 0;
    deviceInfo->totalRule.rule = NULL;
    //-------------------------init_totalRule---------------------------
    //--------------------------init_ruleList----------------------------
    deviceInfo->ruleList.maxListSize = 10;
    deviceInfo->ruleList.ruleListSize = 5;
    deviceInfo->ruleList.totalRuleList = new TotalRule[deviceInfo->ruleList.maxListSize];
    for (Byte i = 0; i < deviceInfo->ruleList.maxListSize; ++i)
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
            deviceInfo->ruleList.totalRuleList[i].ruleName.copy(&strNone);
            deviceInfo->ruleList.totalRuleList[i].ruleToken.copy(&strNone);
            deviceInfo->ruleList.totalRuleList[i].videoSrcToken.copy(&strNone);
        }else{
            continue;
        }
        if(deviceInfo->ruleList.totalRuleList[i].type == 0x10 )
        {
            deviceInfo->ruleList.totalRuleList[i].rule = new Rule_LineDetector[1];
            ptrRule_LineDetector = (Rule_LineDetector*) deviceInfo->ruleList.totalRuleList[i].rule;

            ptrRule_LineDetector->direction = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_LineDetector->polygonListSize = 2;
            ptrRule_LineDetector->detectPolygon = new DetectPolygon[ptrRule_LineDetector->polygonListSize];

            for (Byte j = 0; j < ptrRule_LineDetector->polygonListSize; ++j)
            {
                ptrRule_LineDetector->detectPolygon[j].polygon_x = RC_USER_DEFINE_INIT_VALUE;
                ptrRule_LineDetector->detectPolygon[j].polygon_y = RC_USER_DEFINE_INIT_VALUE;
            }
            ptrRule_LineDetector->metadataStreamSwitch = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_LineDetector = NULL;
        }else if(deviceInfo->ruleList.totalRuleList[i].type == 0x11 )
        {
            deviceInfo->ruleList.totalRuleList[i].rule = new Rule_FieldDetector[1];
            ptrRule_FieldDetector = (Rule_FieldDetector*) deviceInfo->ruleList.totalRuleList[i].rule;

            ptrRule_FieldDetector->polygonListSize = 2;
            ptrRule_FieldDetector->detectPolygon = new DetectPolygon[ptrRule_FieldDetector->polygonListSize];
            for (Byte j = 0; j < ptrRule_FieldDetector->polygonListSize; ++j)
            {
                ptrRule_FieldDetector->detectPolygon[j].polygon_x = RC_USER_DEFINE_INIT_VALUE;
                ptrRule_FieldDetector->detectPolygon[j].polygon_y = RC_USER_DEFINE_INIT_VALUE;
            }
            ptrRule_FieldDetector->metadataStreamSwitch = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_FieldDetector = NULL;
        }else if(deviceInfo->ruleList.totalRuleList[i].type == 0x12 )
        {
            deviceInfo->ruleList.totalRuleList[i].rule = new Rule_MotionDetector[1];
            ptrRule_MotionDetector = (Rule_MotionDetector*) deviceInfo->ruleList.totalRuleList[i].rule;

            ptrRule_MotionDetector->motionExpression = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_MotionDetector->polygonListSize = 2 ;
            ptrRule_MotionDetector->detectPolygon = new DetectPolygon[ptrRule_MotionDetector->polygonListSize];

            for (Byte j = 0; j < ptrRule_MotionDetector->polygonListSize; ++j)
            {
                ptrRule_MotionDetector->detectPolygon[j].polygon_x = RC_USER_DEFINE_INIT_VALUE;
                ptrRule_MotionDetector->detectPolygon[j].polygon_y = RC_USER_DEFINE_INIT_VALUE;
            }
            ptrRule_MotionDetector->metadataStreamSwitch = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_MotionDetector = NULL;
        }else if(deviceInfo->ruleList.totalRuleList[i].type == 0x13 )
        {

            deviceInfo->ruleList.totalRuleList[i].rule = new Rule_Counting[1];
            ptrRule_Counting = (Rule_Counting*) deviceInfo->ruleList.totalRuleList[i].rule;

            ptrRule_Counting->reportTimeInterval = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_Counting->resetTimeInterval = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_Counting->direction = RC_USER_DEFINE_INIT_VALUE;

            ptrRule_Counting->polygonListSize = 2;
            ptrRule_Counting->detectPolygon = new DetectPolygon[ptrRule_Counting->polygonListSize];

            for (Byte j = 0; j < ptrRule_Counting->polygonListSize; ++j)
            {
                ptrRule_Counting->detectPolygon[j].polygon_x = RC_USER_DEFINE_INIT_VALUE;
                ptrRule_Counting->detectPolygon[j].polygon_y = RC_USER_DEFINE_INIT_VALUE;
            }
            ptrRule_Counting->metadataStreamSwitch = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_Counting = NULL;
        }else if(deviceInfo->ruleList.totalRuleList[i].type == 0x14 )
        {
            deviceInfo->ruleList.totalRuleList[i].rule = new Rule_CellMotion[1];
            ptrRule_CellMotion = (Rule_CellMotion*) deviceInfo->ruleList.totalRuleList[i].rule;

            ptrRule_CellMotion->minCount = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_CellMotion->alarmOnDelay = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_CellMotion->alarmOffDelay = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_CellMotion->activeCellsSize = RC_USER_DEFINE_INIT_VALUE;
            ptrRule_CellMotion->activeCells.set(FFx4, 4);
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
    deviceInfo->metadataStreamInfo.metadata_Device.textInformation.copy(&strNone);
    deviceInfo->metadataStreamInfo.metadata_Device.extensionFlag = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->metadataStreamInfo.metadata_Device.baudRate = RETURN_CHANNEL_BAUDRATE;

    deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize = 1;
    deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList = new Metadata_StreamParam[deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize];
    for (Byte i = 0; i < deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize; ++i)
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
        deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken.copy(&strNone);
    }
    deviceInfo->metadataStreamInfo.metadata_Stream.extensionFlag = RC_USER_DEFINE_INIT_VALUE;

    //******************init_metadata_Event******************
    deviceInfo->metadataStreamInfo.metadata_Event.ruleToken.copy(&strNone);
    deviceInfo->metadataStreamInfo.metadata_Event.objectID = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->metadataStreamInfo.metadata_Event.UTCHour = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->metadataStreamInfo.metadata_Event.UTCMinute = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->metadataStreamInfo.metadata_Event.UTCSecond = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->metadataStreamInfo.metadata_Event.UTCYear = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->metadataStreamInfo.metadata_Event.UTCMonth = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->metadataStreamInfo.metadata_Event.UTCDay = RC_USER_DEFINE_INIT_VALUE;
    deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.copy(&strNone);
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
}

TxRC::~TxRC()
{
    TxRC * deviceInfo = this;

    Rule_LineDetector* ptrRule_LineDetector = NULL;
    Rule_FieldDetector* ptrRule_FieldDetector = NULL;
    Rule_MotionDetector* ptrRule_MotionDetector = NULL;
    Rule_Counting* ptrRule_Counting = NULL;
    Rule_CellMotion* ptrRule_CellMotion = NULL;

    //---------------------uninit_security-----------------------
    deviceInfo->security.userName.clear();
    deviceInfo->security.password.clear();
    //---------------------uninit_security-----------------------
    //------------------uninit_HwRegisterValues-----------------------
    delete [] deviceInfo->hwRegisterInfo.registerValues;
    //------------------uninit_HwRegisterValues-----------------------
    //------------------uninit_SdtServiceConfig------------------------
    deviceInfo->serviceConfig.serviceName.clear();
    deviceInfo->serviceConfig.provider.clear();
    //------------------uninit_SdtServiceConfig------------------------
    //-------------------uninit_CalibrationTable------------------------
    deviceInfo->calibrationTable.tableData.clear();
    //------------------init_CalibrationTable------------------------
    //-----------------uninit_EITInfo-----------------
    deviceInfo->eitInfo.videoEncConfigToken.clear();
    for (Byte i = 0; i < deviceInfo->eitInfo.listSize; ++i)
    {
        deviceInfo->eitInfo.eitInfoParam[i].eventName.clear();
        deviceInfo->eitInfo.eitInfoParam[i].eventText.clear();
    }
    delete [] deviceInfo->eitInfo.eitInfoParam;
    deviceInfo->eitInfo.eitInfoParam = NULL;
    deviceInfo->eitInfo.listSize = 0;
    //-----------------uninit_EITInfo-----------------
    //-----------------uninit_DeviceInfo-----------------
    deviceInfo->manufactureInfo.manufactureName.clear();
    deviceInfo->manufactureInfo.modelName.clear();
    deviceInfo->manufactureInfo.firmwareVersion.clear();
    deviceInfo->manufactureInfo.serialNumber.clear();
    deviceInfo->manufactureInfo.hardwareId.clear();
    //-----------------uninit_DeviceInfo-----------------
    //-----------------uninit_Hostname------------------
    deviceInfo->hostInfo.hostName.clear();
    //-----------------uninit_Hostname------------------
    //-----------------uninit_SystemLog-----------------
    deviceInfo->systemLog.logData.clear();
    //-----------------uninit_SystemLog-----------------
    //------------------uninit_OSDInfo------------------
    deviceInfo->osdInfo.text.clear();
    //------------------uninit_OSDInfo------------------
    //-----------uninit_SystemRebootMessage----------
    deviceInfo->systemReboot.responseMessage.clear();
    //-----------uninit_SystemRebootMessage----------
    //-----------------uninit_SystemFirmware--------------------
    deviceInfo->systemFirmware.data.clear();
    deviceInfo->systemFirmware.message.clear();
    //-----------------uninit_SystemFirmware--------------------
    //-----------------uninit_DigitalInputs----------------
    for (Byte i = 0; i < deviceInfo->digitalInputsInfo.listSize; ++i)
    {
        deviceInfo->digitalInputsInfo.tokenList[i].clear();
    }
    delete [] deviceInfo->digitalInputsInfo.tokenList;
    deviceInfo->digitalInputsInfo.tokenList = NULL;
    deviceInfo->digitalInputsInfo.listSize = 0;
    //-----------------uninit_DigitalInputs----------------
    //-----------------uninit_RelayOutputs---------------
    for (Byte i = 0; i < deviceInfo->relayOutputs.listSize; ++i)
    {
        deviceInfo->relayOutputs.relayOutputsParam[i].token.clear();
    }
    delete [] deviceInfo->relayOutputs.relayOutputsParam;
    deviceInfo->relayOutputs.relayOutputsParam = NULL;
    deviceInfo->relayOutputs.listSize = 0;
    //-----------------uninit_RelayOutputs---------------
    //------------uninit_RelayOutputsSetParam----------
    deviceInfo->relayOutputsSetParam.token.clear();
    //------------uninit_RelayOutputsSetParam----------
    //---------------uninit_RelayOutputState-------------
    deviceInfo->relayOutputState.token.clear();
    //---------------uninit_RelayOutputState-------------
    //---------------uninit_ImagingSettings---------------
    deviceInfo->imageConfig.videoSourceToken.clear();
    //---------------uninit_ImagingSettings---------------
    //--------------------uninit_Status--------------------
    deviceInfo->focusStatusInfo.videoSourceToken.clear();
    deviceInfo->focusStatusInfo.error.clear();
    //--------------------uninit_Status--------------------
    //--------------------uninit_ImagingSettingsOption--------------------
    deviceInfo->imageConfigOption.videoSourceToken.clear();
    //--------------------uninit_ImagingSettingsOption--------------------
    //--------------------uninit_Move--------------------
    deviceInfo->focusMoveInfo.videoSourceToken.clear();
    //--------------------uninit_Move--------------------
    //---------------------------uninit_Stop----------------------------
    deviceInfo->focusStopInfo.videoSourceToken.clear();
    //---------------------------uninit_Stop----------------------------
    //---------------------------uninit_UserDefinedSettings----------------------------
    deviceInfo->userDefinedSettings.videoSourceToken.clear();
    deviceInfo->userDefinedSettings.uerDefinedData.clear();
    //---------------------------uninit_UserDefinedSettings----------------------------
    //--------------uninit_AudioEncConfig---------------
    for (Byte i = 0; i < deviceInfo->audioEncConfig.configListSize; ++i)
    {
        deviceInfo->audioEncConfig.configList[i].token.clear();
        deviceInfo->audioEncConfig.configList[i].name.clear();
    }
    delete [] deviceInfo->audioEncConfig.configList;
    deviceInfo->audioEncConfig.configList = NULL;
    deviceInfo->audioEncConfig.configListSize = 0;
    //--------------uninit_AudioEncConfig---------------
    //------------uninit_AudioSourceConfig--------------
    for (Byte i = 0; i < deviceInfo->audioSrcConfig.configListSize; ++i)
    {
        deviceInfo->audioSrcConfig.audioSrcConfigList[i].name.clear();
        deviceInfo->audioSrcConfig.audioSrcConfigList[i].token.clear();
        deviceInfo->audioSrcConfig.audioSrcConfigList[i].sourceToken.clear();
    }
    delete [] deviceInfo->audioSrcConfig.audioSrcConfigList;
    deviceInfo->audioSrcConfig.audioSrcConfigList = NULL;
    deviceInfo->audioSrcConfig.configListSize = 0;
    //------------uninit_AudioSourceConfig--------------
    //---------------uninit_AudioSources-----------------
    for (Byte i = 0; i < deviceInfo->audioSources.audioSourcesListSize; ++i)
    {
        deviceInfo->audioSources.audioSourcesList[i].audioSourcesToken.clear();
    }
    delete [] deviceInfo->audioSources.audioSourcesList;
    deviceInfo->audioSources.audioSourcesList = NULL;
    deviceInfo->audioSources.audioSourcesListSize = 0;
    //---------------uninit_AudioSources-----------------
    //---------------uninit_GuaranteedEncs-----------------
    deviceInfo->guaranteedEncs.configurationToken.clear();
    //---------------uninit_GuaranteedEncs-----------------
    //--------------------uninit_Profiles---------------------
    for (Byte i = 0; i < deviceInfo->mediaProfiles.profilesListSize; ++i)
    {
        deviceInfo->mediaProfiles.mediaProfilesParam[i].name.clear();
        deviceInfo->mediaProfiles.mediaProfilesParam[i].token.clear();
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcName.clear();
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcToken.clear();
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoSrcSourceToken.clear();

        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcName.clear();
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcToken.clear();
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioSrcSourceToken.clear();

        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncName.clear();
        deviceInfo->mediaProfiles.mediaProfilesParam[i].videoEncToken.clear();

        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncName.clear();
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioEncToken.clear();

        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputName.clear();
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputToken.clear();
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioOutputOutputToken.clear();

        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecName.clear();
        deviceInfo->mediaProfiles.mediaProfilesParam[i].audioDecToken.clear();
    }
    delete [] deviceInfo->mediaProfiles.mediaProfilesParam;
    deviceInfo->mediaProfiles.mediaProfilesParam = NULL;
    deviceInfo->mediaProfiles.profilesListSize = 0;
    //--------------------uninit_Profiles---------------------
    //---------------uninit_VideoEncConfig-----------------
    for (Byte i = 0; i < deviceInfo->videoEncConfig.configListSize; ++i)
    {
        deviceInfo->videoEncConfig.configList[i].name.clear();
        deviceInfo->videoEncConfig.configList[i].token.clear();
    }
    delete [] deviceInfo->videoEncConfig.configList;
    deviceInfo->videoEncConfig.configList = NULL;
    deviceInfo->videoEncConfig.configListSize = 0;
    //---------------uninit_VideoEncConfig-----------------
    //---------------uninit_VideoSrcConfig-----------------
    for (Byte i = 0; i < deviceInfo->videoSrcConfig.configListSize; ++i)
    {
        deviceInfo->videoSrcConfig.configList[i].name.clear();
        deviceInfo->videoSrcConfig.configList[i].token.clear();
        deviceInfo->videoSrcConfig.configList[i].srcToken.clear();
    }
    delete [] deviceInfo->videoSrcConfig.configList;
    deviceInfo->videoSrcConfig.configList = NULL;
    deviceInfo->videoSrcConfig.configListSize = 0;
    //---------------uninit_VideoSrcConfig-----------------
    //------------------uninit_VideoSrc---------------------
    for (Byte i = 0; i < deviceInfo->videoSrc.videoSrcListSize; ++i)
    {
        deviceInfo->videoSrc.srcList[i].token.clear();
    }
    delete [] deviceInfo->videoSrc.srcList;
    deviceInfo->videoSrc.srcList = NULL;
    deviceInfo->videoSrc.videoSrcListSize = 0;
    //------------------uninit_VideoSrc---------------------
    //------------------------uninit_VideoSrcConfigOptions---------------------------
    deviceInfo->videoSrcConfigOptions.videoSrcConfigToken.clear();
    deviceInfo->videoSrcConfigOptions.profileToken.clear();
    for (Byte i = 0; i < deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize; ++i)
        deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList[i].clear();
    delete [] deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList;
    deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableList = NULL;
    deviceInfo->videoSrcConfigOptions.videoSrcTokensAvailableListSize = 0;
    //------------------------uninit_VideoSrcConfigOptions---------------------------
    //------------------------uninit_VideoEncConfigOptions---------------------------
    deviceInfo->videoEncConfigOptions.videoEncConfigToken.clear();
    deviceInfo->videoEncConfigOptions.profileToken.clear();
    delete [] deviceInfo->videoEncConfigOptions.resolutionsAvailableList;
    delete [] deviceInfo->videoEncConfigOptions.aspectRatioList;
    deviceInfo->videoEncConfigOptions.resolutionsAvailableList = NULL;
    deviceInfo->videoEncConfigOptions.aspectRatioList = NULL;
    deviceInfo->videoEncConfigOptions.resolutionsAvailableListSize = 0;
    deviceInfo->videoEncConfigOptions.aspectRatioAvailableListSize = 0;
    //------------------------uninit_VideoEncConfigOptions---------------------------
    //------------------------uninit_AudioSrcConfigOptions---------------------------
    deviceInfo->audioSrcConfigOptions.audioSrcConfigToken.clear();
    deviceInfo->audioSrcConfigOptions.profileToken.clear();
    for (Byte i = 0; i < deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize; ++i)
        deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList[i].clear();
    delete [] deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList;
    deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableList = NULL;
    deviceInfo->audioSrcConfigOptions.audioSrcTokensAvailableListSize = 0;
    //------------------------uninit_AudioSrcConfigOptions---------------------------
    //------------------------uninit_AudioEncConfigOptions---------------------------
    deviceInfo->audioEncConfigOptions.audioEncConfigToken.clear();
    deviceInfo->audioEncConfigOptions.profileToken.clear();
    for (Byte i = 0; i < deviceInfo->audioEncConfigOptions.configListSize; ++i)
        delete [] deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam[i].sampleRateAvailableList;
    delete [] deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam;
    deviceInfo->audioEncConfigOptions.audioEncConfigOptionsParam = NULL;
    deviceInfo->audioEncConfigOptions.configListSize = 0;
    //------------------------uninit_AudioEncConfigOptions---------------------------
    //----------uninit_AudioEncConfigSetParam------------
    deviceInfo->audioEncConfigSetParam.name.clear();
    deviceInfo->audioEncConfigSetParam.token.clear();
    //----------uninit_AudioEncConfigSetParam------------
    //----------uninit_AudioSrcConfigSetParam------------
    deviceInfo->audioSrcConfigSetParam.name.clear();
    deviceInfo->audioSrcConfigSetParam.token.clear();
    deviceInfo->audioSrcConfigSetParam.sourceToken.clear();
    //----------uninit_AudioSrcConfigSetParam------------
    //----------uninit_VideoEncConfigSetParam------------
    deviceInfo->videoEncConfigSetParam.name.clear();
    deviceInfo->videoEncConfigSetParam.token.clear();
    //----------uninit_VideoEncConfigSetParam------------
    //----------uninit_VideoSrcConfigSetParam------------
    deviceInfo->videoSrcConfigSetParam.name.clear();
    deviceInfo->videoSrcConfigSetParam.token.clear();
    deviceInfo->videoSrcConfigSetParam.srcToken.clear();
    //----------uninit_VideoSrcConfigSetParam------------
    //----------------uninit_VideoOSDConfig------------------
    deviceInfo->videoOSDConfig.videoSrcToken.clear();
    deviceInfo->videoOSDConfig.text.clear();
    //----------------uninit_VideoOSDConfig------------------
    //------------------------uninit_VideoPrivateArea--------------------------
    deviceInfo->videoPrivateArea.videoSrcToken.clear();
    delete [] deviceInfo->videoPrivateArea.privateAreaPolygon;
    deviceInfo->videoPrivateArea.privateAreaPolygon = NULL;
    deviceInfo->videoPrivateArea.polygonListSize = 0;
    //------------------------uninit_VideoPrivateArea--------------------------
    //-------------------uninit_SyncPoint---------------------
    deviceInfo->syncPoint.profileToken.clear();
    //-------------------uninit_SyncPoint---------------------
    //------------------------uninit_VideoSrcControl--------------------------
    deviceInfo->videoSrcControl.videoSrcToken.clear();
    //------------------------uninit_VideoSrcControl--------------------------
    //-------------------uninit_PTZConfig--------------------
    for (Byte i = 0; i < deviceInfo->ptzConfig.PTZConfigListSize; ++i)
    {
        deviceInfo->ptzConfig.PTZConfigList[i].name.clear();
        deviceInfo->ptzConfig.PTZConfigList[i].token.clear();
        deviceInfo->ptzConfig.PTZConfigList[i].videoSrcToken.clear();
    }
    delete [] deviceInfo->ptzConfig.PTZConfigList;
    deviceInfo->ptzConfig.PTZConfigList = NULL;
    deviceInfo->ptzConfig.PTZConfigListSize = 0;
    //-------------------uninit_PTZConfig--------------------
    //-------------------uninit_PTZStatus---------------------
    deviceInfo->ptzStatus.token.clear();
    deviceInfo->ptzStatus.error.clear();
    //-------------------uninit_PTZStatus---------------------
    //------------------uninit_PTZPresetsGet---------------------
    for (Byte i = 0; i < deviceInfo->ptzPresetsGet.configListSize; ++i)
    {
        deviceInfo->ptzPresetsGet.configList[i].presetName.clear();
        deviceInfo->ptzPresetsGet.configList[i].presetToken.clear();
    }
    delete [] deviceInfo->ptzPresetsGet.configList;
    deviceInfo->ptzPresetsGet.configList = NULL;
    deviceInfo->ptzPresetsGet.configListSize = 0;
    deviceInfo->ptzPresetsGet.token.clear();
    //------------------uninit_PTZPresetsGet---------------------
    //-----------------------uninit_PTZPresetsParam--------------------------
    deviceInfo->ptzPresetsSet.token.clear();
    deviceInfo->ptzPresetsSet.presetName.clear();
    deviceInfo->ptzPresetsSet.presetToken.clear();
    deviceInfo->ptzPresetsSet.presetToken_set.clear();
    //-----------------------uninit_PTZPresetsParam--------------------------
    //----------------uninit_PTZGotoParam-------------------
    deviceInfo->ptzGotoParam.token.clear();
    deviceInfo->ptzGotoParam.presetToken.clear();
    //----------------uninit_PTZGotoParam-------------------
    //----------------uninit_PTZRemoveParam----------------
    deviceInfo->ptzRemoveParam.token.clear();
    deviceInfo->ptzRemoveParam.presetToken.clear();
    //----------------uninit_PTZRemoveParam----------------
    //----------------uninit_PTZAbsoluteMove----------------
    deviceInfo->ptzAbsoluteMove.token.clear();
    //----------------uninit_PTZAbsoluteMove----------------
    //----------------------uninit_PTZRelativeMove---------------------
    deviceInfo->ptzRelativeMove.token.clear();
    //----------------------uninit_PTZRelativeMove---------------------
    //----------------uninit_PTZContinuousMove--------------
    deviceInfo->ptzContinuousMove.token.clear();
    //----------------uninit_PTZContinuousMove--------------
    //-----------------uninit_PTZHomePosition----------------
    deviceInfo->ptzHomePosition.token.clear();
    //-----------------uninit_PTZHomePosition----------------
    //----------------------uninit_PTZStop---------------------
    deviceInfo->ptzStop.token.clear();
    //----------------------uninit_PTZStop---------------------
    //-----------------uninit_SupportedRules-----------------
    delete [] deviceInfo->supportedRules.ruleType;
    //-----------------uninit_SupportedRules-----------------
    //------------------------init_DeleteRule--------------------------
    deviceInfo->deleteRule.ruleToken.clear();
    //------------------------init_DeleteRule--------------------------
    //--------------------uninit_totalRule----------------------
    deviceInfo->totalRule.ruleToken.clear();
    deviceInfo->totalRule.ruleName.clear();
    deviceInfo->totalRule.videoSrcToken.clear();
    if(deviceInfo->totalRule.type == 0x10)
    {
        ptrRule_LineDetector = (Rule_LineDetector*) deviceInfo->totalRule.rule;
        delete [] ptrRule_LineDetector->detectPolygon;
        delete [] ptrRule_LineDetector;
        deviceInfo->totalRule.type = 0x00;
        deviceInfo->totalRule.rule = NULL;
        ptrRule_LineDetector = NULL;
    }else if(deviceInfo->totalRule.type == 0x11)
    {
        ptrRule_FieldDetector = (Rule_FieldDetector*) deviceInfo->totalRule.rule;
        delete [] ptrRule_FieldDetector->detectPolygon;
        delete [] ptrRule_FieldDetector;
        deviceInfo->totalRule.type = 0x00;
        deviceInfo->totalRule.rule = NULL;
        ptrRule_FieldDetector = NULL;
    }else if(deviceInfo->totalRule.type == 0x12)
    {
        ptrRule_MotionDetector = (Rule_MotionDetector*) deviceInfo->totalRule.rule;
        delete [] ptrRule_MotionDetector->detectPolygon;
        delete [] ptrRule_MotionDetector;
        deviceInfo->totalRule.type = 0x00;
        deviceInfo->totalRule.rule = NULL;
        ptrRule_MotionDetector = NULL;
    }else if(deviceInfo->totalRule.type == 0x13)
    {
        ptrRule_Counting = (Rule_Counting*) deviceInfo->totalRule.rule;
        delete [] ptrRule_Counting->detectPolygon;
        delete [] ptrRule_Counting;
        deviceInfo->totalRule.type = 0x00;
        deviceInfo->totalRule.rule = NULL;
        ptrRule_Counting = NULL;
    }else if(deviceInfo->totalRule.type == 0x14)
    {
        ptrRule_CellMotion = (Rule_CellMotion*) deviceInfo->totalRule.rule;
        ptrRule_CellMotion->activeCells.clear();
        delete [] ptrRule_CellMotion;
        deviceInfo->totalRule.type = 0x00;
        deviceInfo->totalRule.rule = NULL;
        ptrRule_CellMotion = NULL;
    }
    //--------------------uninit_totalRule----------------------
    //---------------------uninit_ruleList-----------------------
    for (Byte i = 0; i < deviceInfo->ruleList.maxListSize; ++i)
    {
        if((deviceInfo->ruleList.totalRuleList[i].type >= 0x10) && (deviceInfo->ruleList.totalRuleList[i].type <= 0x14))
        {
            deviceInfo->ruleList.totalRuleList[i].ruleName.clear();
            deviceInfo->ruleList.totalRuleList[i].ruleToken.clear();
            deviceInfo->ruleList.totalRuleList[i].videoSrcToken.clear();
        }else{
            continue;
        }

        if(deviceInfo->ruleList.totalRuleList[i].type == 0x10)
        {
            ptrRule_LineDetector = (Rule_LineDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
            delete [] ptrRule_LineDetector->detectPolygon;
            ptrRule_LineDetector->detectPolygon = NULL;
            delete [] ptrRule_LineDetector;
            deviceInfo->ruleList.totalRuleList[i].type = 0x00;
            deviceInfo->ruleList.totalRuleList[i].rule = NULL;
            ptrRule_LineDetector = NULL;
        }else if(deviceInfo->ruleList.totalRuleList[i].type == 0x11)
        {
            ptrRule_FieldDetector = (Rule_FieldDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
            delete [] ptrRule_FieldDetector->detectPolygon;
            ptrRule_FieldDetector->detectPolygon = NULL;
            delete [] ptrRule_FieldDetector;
            deviceInfo->ruleList.totalRuleList[i].type = 0x00;
            deviceInfo->ruleList.totalRuleList[i].rule = NULL;
            ptrRule_FieldDetector = NULL;
        }else if(deviceInfo->ruleList.totalRuleList[i].type == 0x12)
        {
            ptrRule_MotionDetector = (Rule_MotionDetector*) deviceInfo->ruleList.totalRuleList[i].rule;
            delete [] ptrRule_MotionDetector->detectPolygon;
            ptrRule_MotionDetector->detectPolygon = NULL;
            delete [] ptrRule_MotionDetector;
            deviceInfo->ruleList.totalRuleList[i].type = 0x00;
            deviceInfo->ruleList.totalRuleList[i].rule = NULL;
            ptrRule_MotionDetector = NULL;
        }else if(deviceInfo->ruleList.totalRuleList[i].type == 0x13)
        {
            ptrRule_Counting = (Rule_Counting*) deviceInfo->ruleList.totalRuleList[i].rule;
            delete [] ptrRule_Counting->detectPolygon;
            ptrRule_Counting->detectPolygon = NULL;
            delete [] ptrRule_Counting;
            deviceInfo->ruleList.totalRuleList[i].type = 0x00;
            deviceInfo->ruleList.totalRuleList[i].rule = NULL;
            ptrRule_Counting = NULL;
        }else if(deviceInfo->ruleList.totalRuleList[i].type == 0x14)
        {
            ptrRule_CellMotion = (Rule_CellMotion*) deviceInfo->ruleList.totalRuleList[i].rule;
            ptrRule_CellMotion->activeCells.clear();
            delete [] ptrRule_CellMotion;
            deviceInfo->ruleList.totalRuleList[i].type = 0x00;
            deviceInfo->ruleList.totalRuleList[i].rule = NULL;
            ptrRule_CellMotion = NULL;
        }
    }
    delete [] deviceInfo->ruleList.totalRuleList;
    deviceInfo->ruleList.totalRuleList = NULL;

    //-------------------uninit_ruleList----------------------
    //--------------uninit_MetaDataStream-----------------
    //*************uninit_metadata_Event*************
    deviceInfo->metadataStreamInfo.metadata_Event.ruleToken.clear();
    deviceInfo->metadataStreamInfo.metadata_Event.videoSrcToken.clear();
    //*************uninit_metadata_Event*************
    deviceInfo->metadataStreamInfo.metadata_Device.textInformation.clear();
    for (Byte i = 0; i < deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize; ++i)
        deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList[i].videoSrcToken.clear();
    delete [] deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList;
    deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoList = NULL;
    deviceInfo->metadataStreamInfo.metadata_Stream.streamInfoListSize = 0;
    //--------------uninit_MetaDataStream-----------------
}

//

static void SWordToShort(unsigned short SWordNum, short * num)
{
    short out = 0;
    out = *((short*)&SWordNum);

    (*num) = out;
}

static void shortToSWord(short num, unsigned short * SWordNum)
{
    unsigned short outSWord = 0;
    outSWord = *((unsigned short*)&num);

    (* SWordNum) = outSWord;
}

unsigned Cmd_BytesRead(const Byte* buf , unsigned bufferLength, unsigned* index, Byte* bufDst, unsigned dstLength)
{
    unsigned error = ReturnChannelError::NO_ERROR;
    unsigned tempIndex = 0;

    tempIndex = (* index);
    if(bufferLength< tempIndex + dstLength)
    {
        error = ReturnChannelError::CMD_READ_FAIL;

        memset( bufDst, 0xFF, dstLength);
    }
    else
    {
        memcpy( bufDst, buf + tempIndex, dstLength);

        (* index) = (* index) +dstLength;
    }
    return error;
}

unsigned Cmd_ByteRead(const Byte* buf , unsigned bufferLength, unsigned* index, Byte* var, Byte defaultValue)
{
    unsigned error = ReturnChannelError::NO_ERROR;
    unsigned tempIndex = 0;

    tempIndex = (* index);
    if(bufferLength< tempIndex +1)
    {
        error = ReturnChannelError::CMD_READ_FAIL;
        (* var) = defaultValue;
    }
    else
    {
        (* var) = buf[tempIndex];
        (* index) = (* index) +1;
    }
    return error;
}

unsigned Cmd_WordRead(const Byte* buf , unsigned bufferLength, unsigned* index, Word* var, Word defaultValue)
{
    unsigned error = ReturnChannelError::NO_ERROR;
    unsigned tempIndex = 0;

    tempIndex = (* index);
    if(bufferLength< tempIndex +2)
    {
        error = ReturnChannelError::CMD_READ_FAIL;
        (* var) = defaultValue;
    }
    else
    {
        (* var) = (buf[tempIndex]<<8) | buf[tempIndex+1] ;
        (* index) = (* index) + 2;
    }
    return error;
}

unsigned Cmd_DwordRead(const Byte* buf , unsigned bufferLength, unsigned* index, unsigned* var, unsigned  defaultValue)
{
    unsigned tempIndex = 0;
    unsigned error = ReturnChannelError::NO_ERROR;

    tempIndex = (* index);
    if(bufferLength< tempIndex +4)
    {
        error = ReturnChannelError::CMD_READ_FAIL;
        (* var) = defaultValue;
    }
    else
    {
        (* var) = (buf[tempIndex]<<24) | (buf[tempIndex+1]<<16) | (buf[tempIndex+2]<<8) | buf[tempIndex+3] ;
        (* index) = (* index) + 4;
    }

    return error;
}

unsigned Cmd_FloatRead(const Byte* buf , unsigned bufferLength, unsigned* index, float* var, float  defaultValue)
{
    unsigned tempIndex = 0;
    unsigned tempFloat = 0;
    unsigned error = ReturnChannelError::NO_ERROR;
    Byte *unsignedint_ptr = (Byte *)&tempFloat;
    Byte *float_ptr = (Byte *)var;

    tempIndex = (* index);
    if(bufferLength< tempIndex +4)
    {
        error = ReturnChannelError::CMD_READ_FAIL;
        (* var) = defaultValue;
    }
    else
    {
        tempFloat = (buf[tempIndex]<<24) | (buf[tempIndex+1]<<16) | (buf[tempIndex+2]<<8) | buf[tempIndex+3] ;

        float_ptr[0] = unsignedint_ptr[0];
        float_ptr[1] = unsignedint_ptr[1];
        float_ptr[2] = unsignedint_ptr[2];
        float_ptr[3] = unsignedint_ptr[3];

        (* index) = (* index) + 4;
    }
    return error;
}

unsigned Cmd_ShortRead(const Byte* buf , unsigned bufferLength, unsigned* index, short* var, short defaultValue)
{
    unsigned error = ReturnChannelError::NO_ERROR;
    unsigned tempIndex = 0;
    unsigned short tempSWord = 0;

    tempIndex = (* index);
    if(bufferLength< tempIndex +2)
    {
        error = ReturnChannelError::CMD_READ_FAIL;
        (* var) = defaultValue;
    }
    else
    {

        tempSWord = (buf[tempIndex]<<8) | buf[tempIndex+1] ;
        SWordToShort( tempSWord , var);
        (* index) = (* index) + 2;
    }
    return error;
}

unsigned Cmd_CharRead(const Byte* buf , unsigned bufferLength, unsigned* index, char* var, char defaultValue)
{
    unsigned error = ReturnChannelError::NO_ERROR;
    unsigned tempIndex = 0;

    tempIndex = (* index);
    if(bufferLength< tempIndex +1)
    {
        error = ReturnChannelError::CMD_READ_FAIL;
        (* var) = defaultValue;
    }
    else
    {
        (* var) = buf[tempIndex];
        (* index) = (* index) +1;
    }
    return error;
}

void Cmd_CheckByteIndexRead(const Byte* buf , unsigned length, unsigned* var)
{
    (* var) = (buf[length]<<24) | (buf[length+1]<<16) | (buf[length+2]<<8) | buf[length+3] ;
}

unsigned Cmd_QwordRead(const Byte* buf , unsigned bufferLength, unsigned* index, unsigned long long * var, unsigned long long defaultValue)
{
    unsigned tempIndex = 0;
    Byte i = 0;
    unsigned error = ReturnChannelError::NO_ERROR;

    tempIndex = (* index);
    if(bufferLength< tempIndex +8)
    {
        error = ReturnChannelError::CMD_READ_FAIL;
        (* var) = defaultValue;
    }
    else
    {
        for( i = 0; i < 8; i ++)
        {
            (* var) =  (* var)  << 8;
            (* var)  =  (* var) | buf[tempIndex+i];
        }
        (* index) = (* index) + 8;
    }
    return error;
}

unsigned Cmd_StringRead(const Byte * buf, unsigned bufferLength, unsigned * index, RCString * str)
{
    unsigned error = ReturnChannelError::NO_ERROR;
    unsigned tempIndex = *index;

    if (bufferLength < tempIndex + 4)
    {
        error = ReturnChannelError::CMD_READ_FAIL;
        str->clear();
    }
    else
    {
        unsigned strLength = RCString::readLength( &buf[tempIndex] );
        tempIndex = tempIndex + 4;

        if (bufferLength < tempIndex + strLength)
        {
            error = ReturnChannelError::CMD_READ_FAIL;
            str->clear();
        }
        else
        {
            str->set(buf + tempIndex, strLength);

            tempIndex = tempIndex + str->length();
            (* index) = tempIndex;
        }
    }
    return error;
}

void Cmd_WordAssign(Byte* buf,Word var,unsigned* length)
{
    unsigned tempLength =0;
    tempLength = (* length);
    buf[tempLength+0] = (Byte)(var>>8);
    buf[tempLength+1] = (Byte)(var);

    (*length) = (*length) + 2;
}

void Cmd_DwordAssign(Byte* buf,unsigned var,unsigned* length)
{
    unsigned tempLength =0;
    tempLength = (* length);
    buf[tempLength+0] = (Byte)(var>>24);
    buf[tempLength+1] = (Byte)(var>>16);
    buf[tempLength+2] = (Byte)(var>>8);
    buf[tempLength+3] = (Byte)(var);

    (*length) = (*length) + 4;
}

void Cmd_QwordAssign(Byte* buf, unsigned long long var,unsigned* length)
{
    unsigned tempLength =0;
    tempLength = (* length);
    buf[tempLength+0] = (Byte)(var>>56);
    buf[tempLength+1] = (Byte)(var>>48);
    buf[tempLength+2] = (Byte)(var>>40);
    buf[tempLength+3] = (Byte)(var>>32);
    buf[tempLength+4] = (Byte)(var>>24);
    buf[tempLength+5] = (Byte)(var>>16);
    buf[tempLength+6] = (Byte)(var>>8);
    buf[tempLength+7] = (Byte)(var);
    (*length) = (*length) + 8;
}

void Cmd_StringAssign(Byte* buf,const RCString* str,unsigned* length)
{
    unsigned tempLength = (* length);
    unsigned strLength = str->length();
    buf[tempLength + 0] = (Byte)(strLength>>24);
    buf[tempLength + 1]  = (Byte)(strLength>>16);
    buf[tempLength + 2] = (Byte)(strLength>>8);
    buf[tempLength + 3]  = (Byte)strLength;

    tempLength = tempLength + 4;

    memcpy( buf + tempLength, str->data(), strLength);

    tempLength = tempLength + strLength;
    (*length) = tempLength;
}

void Cmd_FloatAssign(Byte* buf,float var,unsigned* length)
{
    unsigned tempFloat = 0;
    Byte *unsignedint_ptr = (Byte *)&tempFloat;
    Byte *float_ptr = (Byte *)&var;
    unsigned tempLength =0;

    tempLength = (* length);

    unsignedint_ptr[0] = float_ptr[0];
    unsignedint_ptr[1] = float_ptr[1];
    unsignedint_ptr[2] = float_ptr[2];
    unsignedint_ptr[3] = float_ptr[3];

    //Big Endian
    buf[tempLength+0] = (Byte)(tempFloat>>24);
    buf[tempLength+1] = (Byte)(tempFloat>>16);
    buf[tempLength+2] = (Byte)(tempFloat>>8);
    buf[tempLength+3] = (Byte)(tempFloat);

    (*length) = (*length) + 4;
}

void Cmd_ShortAssign(Byte* buf,short var,unsigned* length)
{
    unsigned tempLength =0;
    unsigned short tempSWord;

    shortToSWord( var, &tempSWord);

    tempLength = (* length);
    buf[tempLength+0] = (Byte)(tempSWord>>8);
    buf[tempLength+1] = (Byte)(tempSWord);

    (*length) = (*length) + 2;
}
