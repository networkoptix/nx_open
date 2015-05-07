#ifndef RETURN_CHANNEL_USER_HOST_H
#define RETURN_CHANNEL_USER_HOST_H

#include "ret_chan_cmd_host.h"

//*******************User Define Init********************
void User_Host_init(RCHostInfo* deviceInfo);
void User_init_DeviceID_Host(Word* TxID, Word* RxID);
//*******************User Define Init********************
//*******************User Define UnInit********************
void User_Host_uninit(RCHostInfo* deviceInfo);
//*******************User Define UnInit********************
//*******************User Define Reply********************
//--------------------------General-------------------------------
void User_getSecurity(RCString userName, RCString password,  Byte* valid);
void User_getGeneralReply(RCHostInfo* deviceInfo, Word command);
//void User_getErrorReply(Device device, ReturnValue returnValue, Word command);
//--------------------------General-------------------------------
//-----------------------ccHDtv Service--------------------------
//void User_getDeviceAddressReply(RCHostInfo* deviceInfo, Word command);
void User_getTransmissionParameterReply(RCHostInfo* deviceInfo, Word command);
void User_getHwRegisterValuesReply(RCHostInfo* deviceInfo, Word command);
void User_getAdvanceOptionsReply(RCHostInfo* deviceInfo, Word command);
void User_getTPSInfoReply(RCHostInfo* deviceInfo, Word command);
void User_getSiPsiTableReply(RCHostInfo* deviceInfo, Word command);
void User_getNitLocationReply(RCHostInfo* deviceInfo, Word command);
void User_getSdtServiceReply(RCHostInfo* deviceInfo, Word command);
void User_getEITInfoReply(RCHostInfo* deviceInfo, Word command);
void User_getTransmissionParameterCapabilitiesReply(RCHostInfo* deviceInfo, Word command);
//-----------------------ccHDtv Service--------------------------
//-----------------Device Management Service-------------------
void User_getDeviceCapabilityReply(RCHostInfo* deviceInfo, Word command);
void User_getDeviceInformationReply(RCHostInfo* deviceInfo, Word command);
void User_getHostnameReply(RCHostInfo* deviceInfo, Word command);
void User_getSystemDateAndTimeReply(RCHostInfo* deviceInfo, Word command);
void User_getSystemLogReply(RCHostInfo* deviceInfo, Word command);
void User_getOSDInfoReply(RCHostInfo* deviceInfo, Word command);
void User_systemRebootReply(RCHostInfo* deviceInfo, Word command);
void User_upgradeSystemFirmwareReply(RCHostInfo* deviceInfo, Word command);
//-----------------Device Management Service-------------------
//---------------------Device_IO Service------------------------
void User_getDigitalInputsReply(RCHostInfo* deviceInfo, Word command);
void User_getRelayOutputsReply(RCHostInfo* deviceInfo, Word command);
//---------------------Device_IO Service------------------------
//-----------------------Imaging Service-------------------------
void User_getImagingSettingsReply(RCHostInfo* deviceInfo, Word command);
void User_getStatusReply(RCHostInfo* deviceInfo, Word command);
void User_getOptionsReply(RCHostInfo* deviceInfo, Word command);
void User_getUserDefinedSettingsReply(RCHostInfo* deviceInfo, Word command);
//-----------------------Imaging Service-------------------------
//-------------------------Media Service-------------------------
void User_getVideoPrivateAreaReply(RCHostInfo* deviceInfo, Word command);
void User_getVideoOSDConfigReply(RCHostInfo* deviceInfo, Word command);
void User_getAudioEncConfigOptionsReply(RCHostInfo* deviceInfo, Word command);
void User_getAudioSrcConfigOptionsReply(RCHostInfo* deviceInfo, Word command);
void User_getVideoEncConfigOptionsReply(RCHostInfo* deviceInfo, Word command);
void User_getVideoSrcConfigOptionsReply(RCHostInfo* deviceInfo, Word command);
void User_getAudioEncConfigReply(RCHostInfo* deviceInfo, Word command);
void User_getAudioSourceConfigReply(RCHostInfo* deviceInfo, Word command);
void User_getAudioSourcesReply(RCHostInfo* deviceInfo, Word command);
void User_getGuaranteedEncsReply(RCHostInfo* deviceInfo, Word command);
void User_getProfilesReply(RCHostInfo* deviceInfo, Word command);
void User_getVideoEncConfigReply(RCHostInfo* deviceInfo, Word command);
void User_getVideoSrcConfigReply(RCHostInfo* deviceInfo, Word command);
void User_getVideoSrcReply(RCHostInfo* deviceInfo, Word command);
//-------------------------Media Service-------------------------
//--------------------Video Analytics Service--------------------
void User_getSupportedRulesReply(RCHostInfo* deviceInfo, Word command);
void User_getRulesReply(RCHostInfo* deviceInfo, Word command);
//--------------------Video Analytics Service--------------------
//--------------------------PTZ Service--------------------------
void User_getPTZConfigReply(RCHostInfo* deviceInfo, Word command);
void User_getPTZStatusReply(RCHostInfo* deviceInfo, Word command);
void User_getPresetsReply(RCHostInfo* deviceInfo, Word command);
void User_setPresetReply(RCHostInfo* deviceInfo, Word command);
//--------------------------PTZ Service--------------------------
//-------------------Metadata Stream Service--------------------
void User_metadataStreamInfoReply(RCHostInfo* deviceInfo, Word command);
void User_metadataStreamDeviceReply(RCHostInfo* deviceInfo, Word command);
void User_metadataStreamLineEventReply(RCHostInfo* deviceInfo, Word command);
void User_metadataStreamFieldEventReply(RCHostInfo* deviceInfo, Word command);
void User_metadataStreamMotionEventReply(RCHostInfo* deviceInfo, Word command);
void User_metadataStreamCountingEventReply(RCHostInfo* deviceInfo, Word command);
void User_metadataStreamCellMotionEventReply(RCHostInfo* deviceInfo, Word command);
void User_getMetadataSettingsReply(RCHostInfo* deviceInfo, Word command);
//-------------------Metadata Stream Service--------------------
//*******************User Define Reply********************
#endif
