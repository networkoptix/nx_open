#include <cstdio>

#include "rc_reply.h"
#include "object_counter.h"

#define USER_REPLY  1

#if USER_REPLY

//*******************User Define Reply********************
//--------------------------General-------------------------------

void User_getGeneralReply(RCHostInfo* /*deviceInfo*/, Word command)
{
    printf("[RC] cmd 0x%04x OK!\n", command);
}

#if 0
void User_getErrorReply(Device device, ReturnValue returnValue, Word command)
{
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
    }
    else
    {
        printf("    No Extension Parameter!\n");
    }
    printf("=== Cmd=0x%04x: User_getTransmissionParameterReply ===\n\n", CMD_GetTransmissionParametersOutput);
}

void User_getHwRegisterValuesReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    HwRegisterInfo* in_HwRegisterInfo = &deviceInfo->hwRegisterInfo;
    Byte i = 0;

    printf("=== Cmd=0x%04x: User_getHwRegisterValuesReply ===\n\n", CMD_GetHwRegisterValuesOutput);
    printf("    valueListSize	= %u\n",in_HwRegisterInfo->valueListSize);

    printf("    registerValues	=\n");
    for( i = 0; i < in_HwRegisterInfo->valueListSize; i ++)
        printf("    %u : 0x%02x \n",i,in_HwRegisterInfo->registerValues[i]);
    printf("\n");
    printf("=== Cmd=0x%04x: User_getHwRegisterValuesReply ===\n\n", CMD_GetHwRegisterValuesOutput);
}

void User_getAdvanceOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
    }
    else
    {
        printf("    No Extension Parameter!\n");
    }
    printf("=== Cmd=0x%04x: User_getAdvanceOptionsReply ===\n\n", CMD_GetAdvanceOptionsOutput);
}

void User_getTPSInfoReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    TPSInfo* in_TPSInfo = &deviceInfo->tpsInfo;

    printf("=== Cmd=0x%04x: User_getTPSInfoReply ===\n\n", CMD_GetTPSInformationOutput);
    printf("    Cell ID		= 0x%x\n",in_TPSInfo->cellID);
    printf("    High Code Rate	= 0x%x\n",in_TPSInfo->highCodeRate);
    printf("    Low Code Rate	= 0x%x\n",in_TPSInfo->lowCodeRate);
    printf("    Transmission Mode	= 0x%x\n",in_TPSInfo->transmissionMode);
    printf("    Constellation	= 0x%x\n",in_TPSInfo->constellation);
    printf("    Interval		= 0x%x\n",in_TPSInfo->interval);
    printf("=== Cmd=0x%04x: User_getTPSInfoReply ===\n\n", CMD_GetTPSInformationOutput);
}

void User_getSiPsiTableReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
    }
    else
    {
        printf("    No Extension Parameter!\n");
    }
    printf("=== Cmd=0x%04x: User_getSiPsiTableReply ===\n\n", CMD_GetSiPsiTableOutput);
}

void User_getNitLocationReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    NITLoacation* in_NITLoacation = &deviceInfo->nitLoacation;

    printf("=== Cmd=0x%04x: User_getNitLocationReply ===\n\n", CMD_GetNitLocationOutput);
    printf("    latitude		= %u\n",in_NITLoacation->latitude);
    printf("    longitude		= %u\n",in_NITLoacation->longitude);
    printf("    extentLatitude	= %u\n",in_NITLoacation->extentLatitude);
    printf("    extentLongitude	= %u\n",in_NITLoacation->extentLongitude);
    printf("=== Cmd=0x%04x: User_getNitLocationReply ===\n\n", CMD_GetNitLocationOutput);
}

void User_getSdtServiceReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    ServiceConfig* in_ServiceConfig = &deviceInfo->serviceConfig;

    printf("=== Cmd=0x%04x: User_getSdtServiceReply ===\n\n", CMD_GetSdtServiceOutput);
    printf("    serviceID				= %u\n",in_ServiceConfig->serviceID);
    printf("    enable				= %u\n",in_ServiceConfig->enable);
    printf("    LCN					= 0x%x\n",in_ServiceConfig->LCN);

    printf("    serviceName stringLength		= %u\n",in_ServiceConfig->serviceName.length());
    printf("    serviceName stringData		= %s\n",in_ServiceConfig->serviceName.data());
    printf("    provider stringLength		= %u\n",in_ServiceConfig->provider.length());
    printf("    provider stringData			= %s\n",in_ServiceConfig->provider.data());
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
    }
    else
    {
        printf("    No Extension Parameter!\n");
    }
    printf("=== Cmd=0x%04x: User_getSdtServiceReply ===\n\n", CMD_GetSdtServiceOutput);
}

void User_getEITInfoReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
        printf("    eventName stringLength	= %u\n",in_EITInfo->eitInfoParam[i].eventName.length());
        printf("    eventName stringData	= %s\n",in_EITInfo->eitInfoParam[i].eventName.data());
        printf("    eventText stringLength	= %u\n",in_EITInfo->eitInfoParam[i].eventText.length());
        printf("    eventText stringData	= %s\n",in_EITInfo->eitInfoParam[i].eventText.data());
        printf("---------------------------------- Index %u -------------------------------------\n",i);
    }
    printf("=== Cmd=0x%04x: User_getEITInfoReply ===\n\n", CMD_GetEITInformationOutput);
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
    DeviceCapability* in_DeviceCapability = &deviceInfo->deviceCapability;

    printf("=== Cmd=0x%04x: User_getDeviceCapabilityReply ===\n\n", CMD_GetCapabilitiesOutput);
    printf("    supportedFeatures	= 0x%x\n",in_DeviceCapability->supportedFeatures);
    printf("=== Cmd=0x%04x: User_getDeviceCapabilityReply ===\n\n", CMD_GetCapabilitiesOutput);
}

void User_getDeviceInformationReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    ManufactureInfo* in_ManufactureInfo = &deviceInfo->manufactureInfo;

    printf("=== Cmd=0x%04x: User_getDeviceInformationReply ===\n\n", CMD_GetDeviceInformationOutput);
    printf("    manufactureName.stringLength	= %u\n",in_ManufactureInfo->manufactureName.length());
    printf("    manufactureName.stringData		= %s\n",in_ManufactureInfo->manufactureName.data());

    printf("    modelName.stringLength		= %u\n",in_ManufactureInfo->modelName.length());
    printf("    modelName.stringData		= %s\n",in_ManufactureInfo->modelName.data());

    printf("    firmwareVersion.stringLength	= %u\n",in_ManufactureInfo->firmwareVersion.length());
    printf("    firmwareVersion.stringData		= %s\n",in_ManufactureInfo->firmwareVersion.data());

    printf("    serialNumber.stringLength		= %u\n",in_ManufactureInfo->serialNumber.length());
    printf("    serialNumber.stringData		= %s\n",in_ManufactureInfo->serialNumber.data());

    printf("    hardwareId.stringLength		= %u\n",in_ManufactureInfo->hardwareId.length());
    printf("    hardwareId.stringData		= %s\n",in_ManufactureInfo->hardwareId.data());
    printf("=== Cmd=0x%04x: User_getDeviceInformationReply ===\n\n", CMD_GetDeviceInformationOutput);
}

void User_getHostnameReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    HostInfo* in_HostInfo = &deviceInfo->hostInfo;

    printf("=== Cmd=0x%04x: User_getHostnameReply ===\n\n", CMD_GetHostnameOutput);
    printf("    hostName.stringLength	= %u\n",in_HostInfo->hostName.length());
    printf("    hostName.stringData		= %s\n",in_HostInfo->hostName.data());
    printf("=== Cmd=0x%04x: User_getHostnameReply ===\n\n", CMD_GetHostnameOutput);
}

void User_getSystemDateAndTimeReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
}

void User_getSystemLogReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    SystemLog* in_SystemLog = &deviceInfo->systemLog;

    printf("=== Cmd=0x%04x: User_getSystemLogReply ===\n\n", CMD_GetSystemLogOutput);
    printf("    systemLog stringLength	= %u\n",in_SystemLog->logData.length());
    printf("    systemLog stringData	= %s\n",in_SystemLog->logData.data());
    printf("=== Cmd=0x%04x: User_getSystemLogReply ===\n\n", CMD_GetSystemLogOutput);
}

void User_getOSDInfoReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    OSDInfo* in_OSDInfo = &deviceInfo->osdInfo;

    printf("=== Cmd=0x%04x: User_getOSDInfoReply ===\n\n", CMD_GetOSDInformationOutput);
    printf("    dateEnable		= %u\n",in_OSDInfo->dateEnable);
    printf("    datePosition	= %u\n",in_OSDInfo->datePosition);
    printf("    dateFormat		= %u\n",in_OSDInfo->dateFormat);
    printf("    timeEnable		= %u\n",in_OSDInfo->timeEnable);
    printf("    timePosition	= %u\n",in_OSDInfo->timePosition);
    printf("    timeFormat		= %u\n",in_OSDInfo->timeFormat);
    printf("    logoEnable		= %u\n",in_OSDInfo->logoEnable);
    printf("    logoPosition	= %u\n",in_OSDInfo->logoPosition);
    printf("    logoOption		= %u\n",in_OSDInfo->logoOption);
    printf("    detailInfoEnable	= %u\n",in_OSDInfo->detailInfoEnable);
    printf("    detailInfoPosition	= %u\n",in_OSDInfo->detailInfoPosition);
    printf("    detailInfoOption	= %u\n",in_OSDInfo->detailInfoOption);
    printf("    textEnable		= %u\n",in_OSDInfo->textEnable);
    printf("    textPosition	= %u\n",in_OSDInfo->textPosition);

    printf("    text stringLength	= %u\n",in_OSDInfo->text.length());
    printf("    text stringData	= %s\n",in_OSDInfo->text.data());
    printf("=== Cmd=0x%04x: User_getOSDInfoReply ===\n\n", CMD_GetOSDInformationOutput);
}

void User_systemRebootReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    SystemReboot* in_SystemReboot = &deviceInfo->systemReboot;

    printf("=== Cmd=0x%04x: User_systemReboot ===\n\n", CMD_SystemRebootOutput);
    printf("    rebootMessage stringLength	= %u\n",in_SystemReboot->responseMessage.length());
    printf("    rebootMessage stringData	= %s\n",in_SystemReboot->responseMessage.data());
    printf("=== Cmd=0x%04x: User_systemReboot ===\n\n", CMD_SystemRebootOutput);
}

void User_upgradeSystemFirmwareReply(RCHostInfo* deviceInfo, Word command)
{
    SystemFirmware* in_SystemFirmware = &deviceInfo->systemFirmware;

    printf("=== Cmd=0x%04x: User_upgradeSystemFirmwareReply ===\n\n", command);
    printf("    message stringLength	= %u\n",in_SystemFirmware->message.length());
    printf("    message stringData		= %s\n",in_SystemFirmware->message.data());
    printf("=== Cmd=0x%04x: User_upgradeSystemFirmwareReply ===\n\n", command);
}

//-----------------Device Management Service-------------------

//---------------------Device_IO Service------------------------
void User_getDigitalInputsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    DigitalInputsInfo* in_DigitalInputsInfo = &deviceInfo->digitalInputsInfo;
    Byte i =0;

    printf("=== Cmd=0x%04x: User_getDigitalInputsReply ===\n\n", CMD_GetDigitalInputsOutput);
    printf("    listSize			= %u\n",in_DigitalInputsInfo->listSize);

    for( i = 0; i < in_DigitalInputsInfo->listSize; i ++)
    {
        printf("---------------------------------- Index %u -------------------------------------\n",i);
        printf("    upgradeMessage stringLength	= %u\n",in_DigitalInputsInfo->tokenList[i].length());
        printf("    upgradeMessage stringData	= %s\n",in_DigitalInputsInfo->tokenList[i].data());
        printf("---------------------------------- Index %u -------------------------------------\n",i);
    }
    printf("=== Cmd=0x%04x: User_getDigitalInputsReply ===\n\n", CMD_GetDigitalInputsOutput);
}

void User_getRelayOutputsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    RelayOutputs* in_RelayOutputs = &deviceInfo->relayOutputs;
    Byte i =0;

    printf("=== Cmd=0x%04x: User_getRelayOutputsReply ===\n\n", CMD_GetRelayOutputsOutput);
    printf("    listSize		= %u\n",in_RelayOutputs->listSize);

    for( i = 0; i < in_RelayOutputs->listSize; i ++)
    {
        printf("---------------------------------- Index %u -------------------------------------\n",i);
        printf("    token stringLength	= %u\n", in_RelayOutputs->relayOutputsParam[i].token.length());
        printf("    token stringData	= %s\n", in_RelayOutputs->relayOutputsParam[i].token.data());

        printf("    mode		= %u\n", in_RelayOutputs->relayOutputsParam[i].mode);
        printf("    delayTime		= %u\n", in_RelayOutputs->relayOutputsParam[i].delayTime);
        printf("    idleState		= %u\n", in_RelayOutputs->relayOutputsParam[i].idleState);
        printf("---------------------------------- Index %u -------------------------------------\n",i);
    }
    printf("=== Cmd=0x%04x: User_getRelayOutputsReply ===\n\n", CMD_GetRelayOutputsOutput);
}

//---------------------Device_IO Service------------------------

//-----------------------Imaging Service-------------------------
void User_getImagingSettingsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
}

void User_getStatusReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    FocusStatusInfo* in_FocusStatusInfo = &deviceInfo->focusStatusInfo;

    printf("=== Cmd=0x%04x: User_getStatusReply ===\n\n", CMD_IMG_GetStatusOutput);
    printf("    position		= %u\n",in_FocusStatusInfo->position);
    printf("    moveStatus		= %u\n",in_FocusStatusInfo->moveStatus);
    printf("    error stringLength	= %u\n",in_FocusStatusInfo->error.length());
    printf("    error stringData	= %s\n",in_FocusStatusInfo->error.data());
    printf("=== Cmd=0x%04x: User_getStatusReply ===\n\n", CMD_IMG_GetStatusOutput);
}

void User_getOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
}

void User_getUserDefinedSettingsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    UserDefinedSettings* in_UserDefinedSettings = &deviceInfo->userDefinedSettings;

    printf("=== Cmd=0x%04x: User_getUserDefinedSettingsReply ===\n\n", CMD_GetUserDefinedSettingsInput);
    printf("    uerDefinedData 	stringLength	= %u\n",in_UserDefinedSettings->uerDefinedData.length());
    printf("    uerDefinedData 	stringData	= %s\n",in_UserDefinedSettings->uerDefinedData.data());
    printf("=== Cmd=0x%04x: User_getUserDefinedSettingsReply ===\n\n", CMD_GetUserDefinedSettingsInput);
}
//-----------------------Imaging Service-------------------------

//-------------------------Media Service-------------------------
void User_getVideoPrivateAreaReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
}

void User_getVideoOSDConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
    printf("    logooption			= %u\n",in_VideoOSDConfig->logoOption);
    printf("    detailInfoEnable		= %u\n",in_VideoOSDConfig->detailInfoEnable);
    printf("    detailInfoPosition		= %u\n",in_VideoOSDConfig->detailInfoPosition);
    printf("    detailInfoOption		= %u\n",in_VideoOSDConfig->detailInfoOption);
    printf("    textEnable			= %u\n",in_VideoOSDConfig->textEnable);
    printf("    textPosition		= %u\n",in_VideoOSDConfig->textPosition);
    printf("    text stringLength		= %u\n",in_VideoOSDConfig->text.length());
    printf("    text stringData		= %s\n",in_VideoOSDConfig->text.data());
    printf("=== Cmd=0x%04x: User_getVideoOSDConfigReply ===\n\n", CMD_GetVideoOSDConfigurationOutput);
}

void User_getAudioEncConfigOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
}

void User_getAudioSrcConfigOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    AudioSrcConfigOptions* in_AudioSrcConfigOptions = &deviceInfo->audioSrcConfigOptions;
    Byte i = 0;

    printf("=== Cmd=0x%04x: User_getAudioSrcConfigOptionsReply ===\n\n", CMD_GetAudioSourceConfigurationOptionsOutput);
    printf("    audioSrcTokensAvailableListSize			= %u\n",in_AudioSrcConfigOptions->audioSrcTokensAvailableListSize);
    for( i = 0; i < in_AudioSrcConfigOptions->audioSrcTokensAvailableListSize; i ++)
    {
        printf("    audioSrcTokensAvailableList[%u] stringLength		= %u\n", i, in_AudioSrcConfigOptions->audioSrcTokensAvailableList[i].length());
        printf("    audioSrcTokensAvailableList[%u] stringData		= %s\n", i, in_AudioSrcConfigOptions->audioSrcTokensAvailableList[i].data());
    }
    printf("=== Cmd=0x%04x: User_getAudioSrcConfigOptionsReply ===\n\n", CMD_GetAudioSourceConfigurationOptionsOutput);
}

void User_getVideoEncConfigOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
}

void User_getVideoSrcConfigOptionsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
        printf("    videoSrcTokensAvailableList[%u] stringLength		= %u\n", i, in_VideoSrcConfigOptions->videoSrcTokensAvailableList[i].length());
        printf("    videoSrcTokensAvailableList[%u] stringData		= %s\n", i, in_VideoSrcConfigOptions->videoSrcTokensAvailableList[i].data());
    }
    printf("    rotateModeOptions					= 0x%x\n",in_VideoSrcConfigOptions->rotateModeOptions);
    printf("    rotateDegreeMinOption				= %u\n",in_VideoSrcConfigOptions->rotateDegreeMinOption);
    printf("    mirrorModeOptions					= 0x%x\n",in_VideoSrcConfigOptions->mirrorModeOptions);
    printf("    maxFrameRateMin					= %u\n",in_VideoSrcConfigOptions->maxFrameRateMin);
    printf("    maxFrameRateMax					= %u\n",in_VideoSrcConfigOptions->maxFrameRateMax);
    printf("=== Cmd=0x%04x: User_getVideoSrcConfigOptionsReply ===\n\n", CMD_GetVideoSourceConfigurationOptionsOutput);
}

void User_getAudioEncConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    AudioEncConfig* in_AudioEncConfig = &deviceInfo->audioEncConfig;
    Byte i;

    printf("=== Cmd=0x%04x: User_getAudioEncConfigReply ===\n\n", CMD_GetAudioEncoderConfigurationsOutput);
    printf("    configListSize	= %u\n",in_AudioEncConfig->configListSize);

    for( i = 0; i < in_AudioEncConfig->configListSize; i ++)
    {
        printf("---------------------------------- Index %u -------------------------------------\n",i);
        printf("    token stringLength	= %u\n",in_AudioEncConfig->configList[i].token.length());
        printf("    token stringData	= %s\n",in_AudioEncConfig->configList[i].token.data());
        printf("    name stringLength	= %u\n",in_AudioEncConfig->configList[i].name.length());
        printf("    name stringData	= %s\n",in_AudioEncConfig->configList[i].name.data());
        printf("    useCount		= %u\n",in_AudioEncConfig->configList[i].useCount);
        printf("    encoding		= %u\n",in_AudioEncConfig->configList[i].encoding);
        printf("    bitrate		= %u\n",in_AudioEncConfig->configList[i].bitrate);
        printf("    sampleRate		= %u\n",in_AudioEncConfig->configList[i].sampleRate);
        printf("---------------------------------- Index %u -------------------------------------\n",i);
    }
    printf("=== Cmd=0x%04x: User_getAudioEncConfigReply ===\n\n", CMD_GetAudioEncoderConfigurationsOutput);
}

void User_getAudioSourceConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    AudioSrcConfig* in_AudioSrcConfig = &deviceInfo->audioSrcConfig;
    Byte i;

    printf("=== Cmd=0x%04x: User_getAudioSourceConfigReply ===\n\n", CMD_GetAudioSourceConfigurationsOutput);
    printf("    configListSize		= %u\n", in_AudioSrcConfig->configListSize);
    for( i = 0; i < in_AudioSrcConfig->configListSize; i ++)
    {
        printf("----------------------------- Index %u ----------------------------------\n",i);
        printf("    name stringLength		= %u\n", in_AudioSrcConfig->audioSrcConfigList[i].name.length());
        printf("    name stringData		= %s\n",in_AudioSrcConfig->audioSrcConfigList[i].name.data());
        printf("    token stringLength		= %u\n",in_AudioSrcConfig->audioSrcConfigList[i].token.length());
        printf("    token stringData		= %s\n",in_AudioSrcConfig->audioSrcConfigList[i].token.data());
        printf("    sourceToken stringLength	= %u\n",  in_AudioSrcConfig->audioSrcConfigList[i].sourceToken.length());
        printf("    sourceToken stringData	= %s\n",in_AudioSrcConfig->audioSrcConfigList[i].sourceToken.data());
        printf("    useCount			= %u\n",in_AudioSrcConfig->audioSrcConfigList[i].useCount);
        printf("----------------------------- Index %u ----------------------------------\n",i);
    }
    printf("=== Cmd=0x%04x: User_getAudioSourceConfigReply ===\n\n", CMD_GetAudioSourceConfigurationsOutput);
}

void User_getAudioSourcesReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    AudioSources* in_AudioSources = &deviceInfo->audioSources;
    Byte i;

    printf("=== Cmd=0x%04x: User_getAudioSourcesReply ===\n\n", CMD_GetAudioSourcesInput);
    printf("    audioSourcesListSize		= %u\n", in_AudioSources->audioSourcesListSize);
    for( i = 0; i < in_AudioSources->audioSourcesListSize; i ++)
    {
        printf("----------------------------- Index %u ----------------------------------\n",i);
        printf("    audioSourcesToken stringLength	= %u\n", in_AudioSources->audioSourcesList[i].audioSourcesToken.length());
        printf("    audioSourcesToken stringData	= %s\n",  in_AudioSources->audioSourcesList[i].audioSourcesToken.data());

        printf("    channels				= %u\n",  in_AudioSources->audioSourcesList[i].channels);
        printf("----------------------------- Index %u ----------------------------------\n",i);
    }
    printf("=== Cmd=0x%04x: User_getAudioSourcesReply ===\n\n", CMD_GetAudioSourcesInput);
}

void User_getGuaranteedEncsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    GuaranteedEncs* in_GuaranteedEncs = &deviceInfo->guaranteedEncs;

    printf("=== Cmd=0x%04x: User_getGuaranteedEncsReply ===\n\n", CMD_GetGuaranteedNumberOfVideoEncoderInstancesOutput);
    printf("    TotallNum	= %u\n", in_GuaranteedEncs->TotallNum);
    printf("    JPEGNum	= %u\n", in_GuaranteedEncs->JPEGNum);
    printf("    H264Num	= %u\n", in_GuaranteedEncs->H264Num);
    printf("    MPEG4Num	= %u\n", in_GuaranteedEncs->MPEG4Num);
    printf("    MPEG2Num	= %u\n", in_GuaranteedEncs->MPEG2Num);
    printf("=== Cmd=0x%04x: User_getGuaranteedEncsReply ===\n\n", CMD_GetGuaranteedNumberOfVideoEncoderInstancesOutput);
}

void User_getProfilesReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    MediaProfiles* in_MediaProfiles = &deviceInfo->mediaProfiles;
    Byte i;

    printf("=== Cmd=0x%04x: User_getProfilesReply ===\n\n", CMD_GetProfilesOutput);
    printf("    extensionFlag					= %u\n", in_MediaProfiles->extensionFlag);
    printf("    profilesListSize					= %u\n", in_MediaProfiles->profilesListSize);

    for( i = 0; i < in_MediaProfiles->profilesListSize; i ++)
    {
        printf("----------------------------- Index %u ----------------------------------\n",i);
        printf("    name stringLength					= %u\n", in_MediaProfiles->mediaProfilesParam[i].name.length());
        printf("    name stringData					= %s\n", in_MediaProfiles->mediaProfilesParam[i].name.data());
        printf("    token stringLength					= %u\n", in_MediaProfiles->mediaProfilesParam[i].token.length());
        printf("    token stringData					= %s\n", in_MediaProfiles->mediaProfilesParam[i].token.data());
        printf("    videoSrcName stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcName.length());
        printf("    videoSrcName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcName.data());
        printf("    videoSrcToken stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcToken.length());
        printf("    videoSrcToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcToken.data());
        printf("    videoSrcSourceToken stringLength			= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcSourceToken.length());
        printf("    videoSrcSourceToken stringData			= %s\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcSourceToken.data());
        printf("    videoSrcUseCount					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcUseCount);
        printf("    videoSrcBounds_x					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcBounds_x);
        printf("    videoSrcBounds_y					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcBounds_y);
        printf("    videoSrcBoundsWidth					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcBoundsWidth);
        printf("    videoSrcBoundsHeight				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcBoundsHeight);
        printf("    videoSrcRotateMode					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcRotateMode);
        printf("    videoSrcRotateDegree				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcRotateDegree);
        printf("    videoSrcMirrorMode					= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoSrcMirrorMode);

        printf("    audioSrcName stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcName.length());
        printf("    audioSrcName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcName.data());
        printf("    audioSrcToken stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcToken.length());
        printf("    audioSrcToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcToken.data());
        printf("    audioSrcSourceToken stringLength			= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcSourceToken.length());
        printf("    audioSrcSourceToken stringData			= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcSourceToken.data());
        printf("    audioSrcUseCount					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioSrcUseCount);

        printf("    videoEncName stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncName.length());
        printf("    videoEncName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].videoEncName.data());
        printf("    videoEncToken stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].videoEncToken.length());
        printf("    videoEncToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].videoEncToken.data());
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

        printf("    audioEncName stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncName.length());
        printf("    audioEncName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioEncName.data());
        printf("    audioEncToken stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncToken.length());
        printf("    audioEncToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioEncToken.data());
        printf("    audioEncUseCount					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncUseCount);
        printf("    audioEncEncoding					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncEncoding);
        printf("    audioEncBitrate					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncBitrate);
        printf("    audioEncSampleRate					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioEncSampleRate);

        printf("    audioOutputName stringLength			= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputName.length());
        printf("    audioOutputName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputName.data());
        printf("    audioOutputToken stringLength			= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputToken.length());
        printf("    audioOutputToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputToken.data());
        printf("    audioOutputOutputToken stringLength			= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputOutputToken.length());
        printf("    audioOutputOutputToken stringData			= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputOutputToken.data());
        printf("    audioOutputUseCount					= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputUseCount);
        printf("    audioOutputSendPrimacy				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputSendPrimacy);
        printf("    audioOutputOutputLevel				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioOutputOutputLevel);

        printf("    audioDecName stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioDecName.length());
        printf("    audioDecName stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioDecName.data());
        printf("    audioDecToken stringLength				= %u\n", in_MediaProfiles->mediaProfilesParam[i].audioDecToken.length());
        printf("    audioDecToken stringData				= %s\n", in_MediaProfiles->mediaProfilesParam[i].audioDecToken.data());
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
}

void User_getVideoEncConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    VideoEncConfig* in_VideoEncConfig = &deviceInfo->videoEncConfig;
    Byte i;

    printf("=== Cmd=0x%04x: User_getVideoEncConfigReply ===\n\n", CMD_GetVideoEncoderConfigurationsOutput);
    printf("    extensionFlag		= %u\n", in_VideoEncConfig->extensionFlag);
    printf("    configListSize		= %u\n", in_VideoEncConfig->configListSize);
    for( i = 0; i < in_VideoEncConfig->configListSize; i ++)
    {
        printf("---------------------------------- Index %u -------------------------------------\n",i);
        printf("    name stringLength		= %u\n", in_VideoEncConfig->configList[i].name.length());
        printf("    name stringData		= %s\n", in_VideoEncConfig->configList[i].name.data());
        printf("    token stringLength		= %u\n", in_VideoEncConfig->configList[i].token.length());
        printf("    token stringData		= %s\n", in_VideoEncConfig->configList[i].token.data());
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
}

void User_getVideoSrcConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    VideoSrcConfig* in_VideoSrcConfig = &deviceInfo->videoSrcConfig;
    Byte i;

    printf("=== Cmd=0x%04x: User_getVideoSrcConfigReply ===\n\n", CMD_GetVideoSourceConfigurationsOutput);
    printf("    extensionFlag		= %u\n",in_VideoSrcConfig->extensionFlag);
    printf("    configListSize		= %u\n",in_VideoSrcConfig->configListSize);
    for( i = 0; i < in_VideoSrcConfig->configListSize; i ++)
    {
        printf("---------------------------------- Index %u -------------------------------------\n", i);
        printf("    name stringLength		= %u\n",in_VideoSrcConfig->configList[i].name.length());
        printf("    name stringData		= %s\n",in_VideoSrcConfig->configList[i].name.data());
        printf("    token stringLength		= %u\n",in_VideoSrcConfig->configList[i].token.length());
        printf("    token stringData		= %s\n",in_VideoSrcConfig->configList[i].token.data());
        printf("    srcToken stringLength	= %u\n",in_VideoSrcConfig->configList[i].srcToken.length());
        printf("    srcToken stringData		= %s\n",in_VideoSrcConfig->configList[i].srcToken.data());
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
}

void User_getVideoSrcReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    VideoSrc* in_VideoSrc = &deviceInfo->videoSrc;
    Byte i;

    printf("=== Cmd=0x%04x: User_getVideoSrcReply ===\n\n", CMD_GetVideoSourcesOutput);
    printf("    videoSrcListSize	= %u\n",in_VideoSrc->videoSrcListSize);
    for( i = 0; i < in_VideoSrc->videoSrcListSize; i ++)
    {
        printf("---------------------------------- Index %u -------------------------------------\n", i);
        printf("    token stringLength	= %u\n",in_VideoSrc->srcList[i].token.length());
        printf("    token stringData	= %s\n",in_VideoSrc->srcList[i].token.data());
        printf("    frameRate		= %u\n",in_VideoSrc->srcList[i].frameRate);
        printf("    resolutionWidth	= %u\n",in_VideoSrc->srcList[i].resolutionWidth);
        printf("    resolutionHeight	= %u\n",in_VideoSrc->srcList[i].resolutionHeight);
        printf("---------------------------------- Index %u -------------------------------------\n", i);
    }
    printf("=== Cmd=0x%04x: User_getVideoSrcReply ===\n\n", CMD_GetVideoSourcesOutput);
}
//-------------------------Media Service-------------------------

//--------------------Video Analytics Service--------------------
void User_getSupportedRulesReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    SupportedRules* in_SupportedRules = &deviceInfo->supportedRules;
    Byte i;

    printf("=== Cmd=0x%04x: User_getSupportedRulesReply ===\n\n", CMD_GetSupportedRulesOutput);
    printf("    supportListSize	= %u\n",in_SupportedRules->supportListSize);
    for( i = 0; i < in_SupportedRules->supportListSize; i ++)
    {
        printf("    ruleType		= 0x%02x\n",in_SupportedRules->ruleType[i]);
    }
    printf("=== Cmd=0x%04x: User_getSupportedRulesReply ===\n\n", CMD_GetSupportedRulesOutput);
}

void User_getRulesReply(RCHostInfo* deviceInfo, Word /*command*/)
{
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
        printf("    ruleName stringLength	= %u\n",in_RuleList->totalRuleList[i].ruleName.length());
        printf("    ruleName stringData		= %s\n",in_RuleList->totalRuleList[i].ruleName.data());
        printf("    ruleToken stringLength	= %u\n",in_RuleList->totalRuleList[i].ruleToken.length());
        printf("    ruleToken stringData	= %s\n",in_RuleList->totalRuleList[i].ruleToken.data());
        printf("    type			= 0x%02x\n",in_RuleList->totalRuleList[i].type);

        if(deviceInfo->ruleList.extensionFlag == 1)
        {
            printf("    videoSrcToken stringLength	= %u\n",in_RuleList->totalRuleList[i].videoSrcToken.length());
            printf("    videoSrcToken stringData	= %s\n",in_RuleList->totalRuleList[i].videoSrcToken.data());
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
            printf("    activeCells stringLength	= %u\n",ptrRule_CellMotion->activeCells.length());
            printf("    activeCells stringData	= \n");
            for (j = 0; j < ptrRule_CellMotion->activeCells.length(); j++)
                printf("    [%u] : 0x%02x\n", j , ptrRule_CellMotion->activeCells.data()[j]);
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
}
//--------------------Video Analytics Service--------------------

//--------------------------PTZ Service--------------------------
void User_getPTZConfigReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    PTZConfig* in_PTZConfig = &deviceInfo->ptzConfig;
    Byte i;

    printf("=== Cmd=0x%04x: User_getPTZConfigReply ===\n\n", CMD_GetConfigurationsOutput);
    printf("    extensionFlag	= %u\n",in_PTZConfig->extensionFlag);
    printf("    PTZConfigListSize	= %u\n",in_PTZConfig->PTZConfigListSize);

    for( i = 0; i < in_PTZConfig->PTZConfigListSize; i ++)
    {
        printf("------------------------------------ Index %u --------------------------------------\n", i);
        printf("    name stringLength		= %u\n",in_PTZConfig->PTZConfigList[i].name.length());
        printf("    name stringData		= %s\n",in_PTZConfig->PTZConfigList[i].name.data());
        printf("    useCount			= %u\n",in_PTZConfig->PTZConfigList[i].useCount);
        printf("    token stringLength		= %u\n",in_PTZConfig->PTZConfigList[i].token.length());
        printf("    token stringData		= %s\n",in_PTZConfig->PTZConfigList[i].token.data());
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
            printf("    videoSrcToken stringLength	= %u\n",in_PTZConfig->PTZConfigList[i].videoSrcToken.length());
            printf("    videoSrcToken stringData	= %s\n",in_PTZConfig->PTZConfigList[i].videoSrcToken.data());
        }else
        {
            printf("    No Extension Parameter!\n");
        }
        printf("------------------------------------ Index %u --------------------------------------\n", i);
    }
    printf("=== Cmd=0x%04x: User_getPTZConfigReply ===\n\n", CMD_GetConfigurationsOutput);
}

void User_getPTZStatusReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    PTZStatus* in_PTZStatus = &deviceInfo->ptzStatus;

    printf("=== Cmd=0x%04x: User_getPTZStatusReply ===\n\n", CMD_PTZ_GetStatusOutput);
    printf("    panPosition		= %hd\n",in_PTZStatus->panPosition);
    printf("    tiltPosition	= %hd\n",in_PTZStatus->tiltPosition);
    printf("    zoomPosition	= %hd\n",in_PTZStatus->zoomPosition);
    printf("    panTiltMoveStatus	= %u\n",in_PTZStatus->panTiltMoveStatus);
    printf("    zoomMoveStatus	= %u\n",in_PTZStatus->zoomMoveStatus);
    printf("    error stringLength	= %u\n",in_PTZStatus->error.length());
    printf("    error stringData	= %s\n",in_PTZStatus->error.data());
    printf("    UTCHour		= %u\n",in_PTZStatus->UTCHour);
    printf("    UTCMinute		= %u\n",in_PTZStatus->UTCMinute);
    printf("    UTCSecond		= %u\n",in_PTZStatus->UTCSecond);
    printf("    UTCYear		= %u\n",in_PTZStatus->UTCYear);
    printf("    UTCMonth		= %u\n",in_PTZStatus->UTCMonth);
    printf("    UTCDay		= %u\n",in_PTZStatus->UTCDay);
    printf("=== Cmd=0x%04x: User_getPTZStatusReply ===\n\n", CMD_PTZ_GetStatusOutput);
}

void User_getPresetsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    PTZPresets* in_PTZPresets = &deviceInfo->ptzPresetsGet;
    Byte i;

    printf("=== Cmd=0x%04x: User_getPresetsReply ===\n\n", CMD_GetPresetsOutput);
    printf("    presetListSize		= %u\n",in_PTZPresets->configListSize);
    for( i = 0 ; i < in_PTZPresets->configListSize; i ++)
    {
        printf("------------------------------------ Index %u --------------------------------------\n", i);
        printf("    presetName stringLength	= %u\n",in_PTZPresets->configList[i].presetName.length());
        printf("    presetName stringData	= %s\n",in_PTZPresets->configList[i].presetName.data());
        printf("    presetToken stringLength	= %u\n",in_PTZPresets->configList[i].presetToken.length());
        printf("    presetToken stringData	= %s\n",in_PTZPresets->configList[i].presetToken.data());

        printf("    panPosition			= %hd\n",in_PTZPresets->configList[i].panPosition);
        printf("    tiltPosition		= %hd\n",in_PTZPresets->configList[i].tiltPosition);
        printf("    zoomPosition		= %hd\n",in_PTZPresets->configList[i].zoomPosition);
        printf("------------------------------------ Index %u --------------------------------------\n", i);
    }
    printf("=== Cmd=0x%04x: User_getPresetsReply ===\n\n", CMD_GetPresetsOutput);
}

void User_setPresetReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    PTZPresetsSet* in_PTZPresetsSet = &deviceInfo->ptzPresetsSet;

    printf("=== Cmd=0x%04x: User_setPresetReply ===\n\n", CMD_SetPresetOutput);
    printf("    presetToken stringLength	= %u\n",in_PTZPresetsSet->presetToken_set.length());
    printf("    presetToken stringData	= %s\n",in_PTZPresetsSet->presetToken_set.data());
    printf("=== Cmd=0x%04x: User_setPresetReply ===\n\n", CMD_SetPresetOutput);
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
    MetadataStreamInfo* in_MetadataStreamInfo = &deviceInfo->metadataStreamInfo;
    Metadata_Event* in_Metadata_Line_Event = &deviceInfo->metadataStreamInfo.metadata_Event;

    printf("\n=== Cmd=0x%04x: User_metadataStreamLineEventReply ===\r\n", CMD_MetadataStreamOutput);
    printf("     type			= 0x%04x\r\n",in_MetadataStreamInfo->type);
    printf("     version			= 0x%04x\r\n",in_MetadataStreamInfo->version);
    printf("     ruleToken stringLength	= %u\r\n",in_Metadata_Line_Event->ruleToken.length());
    printf("     ruleToken stringData	= %s\r\n",in_Metadata_Line_Event->ruleToken.data());
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
        printf("     videoSrcToken stringLength	= %u\n",in_Metadata_Line_Event->videoSrcToken.length());
        printf("     videoSrcToken stringData	= %s\n",in_Metadata_Line_Event->videoSrcToken.data());
    }else
    {
        printf("    No Extension Parameter!\n");
    }
    printf("=== Cmd=0x%04x: User_metadataStreamLineEventReply ===\r\n", CMD_MetadataStreamOutput);
}

void User_metadataStreamFieldEventReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    MetadataStreamInfo* in_MetadataStreamInfo = &deviceInfo->metadataStreamInfo;
    Metadata_Event* in_Metadata_Field_Event = &deviceInfo->metadataStreamInfo.metadata_Event;

    printf("\n=== Cmd=0x%04x: User_metadataStreamFieldEventReply ===\r\n", CMD_MetadataStreamOutput);
    printf("     type			= 0x%04x\r\n",in_MetadataStreamInfo->type);
    printf("     version			= 0x%04x\r\n",in_MetadataStreamInfo->version);
    printf("     ruleToken stringLength	= %u\r\n",in_Metadata_Field_Event->ruleToken.length());
    printf("     ruleToken stringData	= %s\r\n",in_Metadata_Field_Event->ruleToken.data());
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
        printf("     videoSrcToken stringLength	= %u\n",in_Metadata_Field_Event->videoSrcToken.length());
        printf("     videoSrcToken stringData	= %s\n",in_Metadata_Field_Event->videoSrcToken.data());
    }else
    {
        printf("    No Extension Parameter!\n");
    }
    printf("=== Cmd=0x%04x: User_metadataStreamFieldEventReply ===\r\n", CMD_MetadataStreamOutput);
}

void User_metadataStreamMotionEventReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    MetadataStreamInfo* in_MetadataStreamInfo = &deviceInfo->metadataStreamInfo;
    Metadata_Event* in_Metadata_Motion_Event = &deviceInfo->metadataStreamInfo.metadata_Event;

    printf("\n=== Cmd=0x%04x: User_metadataStreamMotionEventReply ===\r\n", CMD_MetadataStreamOutput);
    printf("     type			= 0x%04x\r\n",in_MetadataStreamInfo->type);
    printf("     version			= 0x%04x\r\n",in_MetadataStreamInfo->version);
    printf("     ruleToken stringLength	= %u\r\n",in_Metadata_Motion_Event->ruleToken.length());
    printf("     ruleToken stringData	= %s\r\n",in_Metadata_Motion_Event->ruleToken.data());
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
        printf("     videoSrcToken stringLength	= %u\n",in_Metadata_Motion_Event->videoSrcToken.length());
        printf("     videoSrcToken stringData	= %s\n",in_Metadata_Motion_Event->videoSrcToken.data());
    }else
    {
        printf("    No Extension Parameter!\n");
    }
    printf("=== Cmd=0x%04x: User_metadataStreamMotionEventReply ===\r\n", CMD_MetadataStreamOutput);
}

void User_metadataStreamCountingEventReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    MetadataStreamInfo* in_MetadataStreamInfo = &deviceInfo->metadataStreamInfo;
    Metadata_Event* in_Metadata_Counting_Event = &deviceInfo->metadataStreamInfo.metadata_Event;

    printf("\n=== Cmd=0x%04x: User_metadataStreamCountingEventReply ===\r\n", CMD_MetadataStreamOutput);
    printf("     type			= 0x%04x\r\n",in_MetadataStreamInfo->type);
    printf("     version			= 0x%04x\r\n",in_MetadataStreamInfo->version);
    printf("     ruleToken stringLength	= %u\r\n",in_Metadata_Counting_Event->ruleToken.length());
    printf("     ruleToken stringData	= %s\r\n",in_Metadata_Counting_Event->ruleToken.data());
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
        printf("     videoSrcToken stringLength	= %u\n",in_Metadata_Counting_Event->videoSrcToken.length());
        printf("     videoSrcToken stringData	= %s\n",in_Metadata_Counting_Event->videoSrcToken.data());
    }else
    {
        printf("    No Extension Parameter!\n");
    }
    printf("=== Cmd=0x%04x: User_metadataStreamCountingEventReply ===\r\n", CMD_MetadataStreamOutput);
}

void User_metadataStreamCellMotionEventReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    MetadataStreamInfo* in_MetadataStreamInfo = &deviceInfo->metadataStreamInfo;
    Metadata_Event* in_Metadata_CellMotion_Event = &deviceInfo->metadataStreamInfo.metadata_Event;

    printf("\n=== Cmd=0x%04x: User_metadataStreamCellMotionEventReply ===\r\n", CMD_MetadataStreamOutput);
    printf("     type			= 0x%04x\r\n",in_MetadataStreamInfo->type);
    printf("     version			= 0x%04x\r\n",in_MetadataStreamInfo->version);
    printf("     ruleToken stringLength	= %u\r\n",in_Metadata_CellMotion_Event->ruleToken.length());
    printf("     ruleToken stringData	= %s\r\n",in_Metadata_CellMotion_Event->ruleToken.data());
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
        printf("     videoSrcToken stringLength	= %u\n",in_Metadata_CellMotion_Event->videoSrcToken.length());
        printf("     videoSrcToken stringData	= %s\n",in_Metadata_CellMotion_Event->videoSrcToken.data());
    }else
    {
        printf("    No Extension Parameter!\n");
    }
    printf("=== Cmd=0x%04x: User_metadataStreamCellMotionEventReply ===\r\n", CMD_MetadataStreamOutput);
}

void User_getMetadataSettingsReply(RCHostInfo* deviceInfo, Word /*command*/)
{
    MetadataSettings* in_MetadataSettings = &deviceInfo->metadataSettings;

    printf("=== Cmd=0x%04x: User_getMetadataSettingsReply ===\n\n", CMD_GetMetadataSettingsOutput);

    printf("     deviceInfoEnable		= %u\n",in_MetadataSettings->deviceInfoEnable);
    printf("     deviceInfoPeriod		= %u\n",in_MetadataSettings->deviceInfoPeriod);
    printf("     streamInfoEnable		= %u\n",in_MetadataSettings->streamInfoEnable);
    printf("     streamInfoPeriod		= %u\n",in_MetadataSettings->streamInfoPeriod);
    printf("     motionDetectorEnable	= %u\n",in_MetadataSettings->motionDetectorEnable);
    printf("     motionDetectorPeriod	= %u\n",in_MetadataSettings->motionDetectorPeriod);

    printf("=== Cmd=0x%04x: User_getMetadataSettingsReply ===\n\n", CMD_GetMetadataSettingsOutput);
}

//*******************User Define Reply********************

#endif
