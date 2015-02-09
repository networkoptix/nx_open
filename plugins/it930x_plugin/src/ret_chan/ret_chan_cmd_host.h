#ifndef RETURN_CHANNEL_CMD_HOST_H
#define RETURN_CHANNEL_CMD_HOST_H

#include "ret_chan_cmd.h"

typedef struct RCHostHandler
{
	//--------------General-------------------
	Device									device;
	Security								security;
	CmdSendConfig							cmdSendConfig;
	//--------------General-------------------

	//-----------ccHDtv Service--------------
	NewTxDevice								newTxDevice;
	HwRegisterInfo							hwRegisterInfo;
	AdvanceOptions							advanceOptions;
	TPSInfo									tpsInfo;
	TransmissionParameterCapabilities		transmissionParameterCapabilities;
	TransmissionParameter					transmissionParameter;
	PSITable								psiTable;
	NITLoacation							nitLoacation;
	ServiceConfig							serviceConfig;
	CalibrationTable						calibrationTable;
	EITInfo									eitInfo;
	//-----------ccHDtv Service--------------

	//-------Device Management Service-----
	DeviceCapability						deviceCapability;
	ManufactureInfo							manufactureInfo;
	HostInfo								hostInfo;
	SystemTime								systemTime;
	SystemLog								systemLog;
	OSDInfo									osdInfo;
	SystemDefault							systemDefault;
	SystemReboot							systemReboot;
	SystemFirmware							systemFirmware;
	//-------Device Management Service-----

	//----------Device_IO Service-----------
	DigitalInputsInfo						digitalInputsInfo;
	RelayOutputs							relayOutputs;
	RelayOutputsParam						relayOutputsSetParam;
	RelayOutputState						relayOutputState;
	//----------Device_IO Service-----------

	//------------------Imaging Service---------------------
	ImageConfig								imageConfig;
	FocusStatusInfo							focusStatusInfo;
	FocusMoveInfo							focusMoveInfo;
	FocusStopInfo							focusStopInfo;
	ImageConfigOption						imageConfigOption;
	UserDefinedSettings						userDefinedSettings;
	//------------------Imaging Service---------------------

	//------------------Media Service------------------
	AudioEncConfig							audioEncConfig;
	AudioSrcConfig							audioSrcConfig;
	AudioSources							audioSources;
	GuaranteedEncs							guaranteedEncs;
	MediaProfiles							mediaProfiles;
	VideoEncConfig							videoEncConfig;
	VideoSrcConfig							videoSrcConfig;
	VideoSrc								videoSrc;
	VideoSrcConfigOptions					videoSrcConfigOptions;
	VideoEncConfigOptions					videoEncConfigOptions;
	AudioSrcConfigOptions					audioSrcConfigOptions;
	AudioEncConfigOptions					audioEncConfigOptions;
	AudioEncConfigParam						audioEncConfigSetParam;
	AudioSrcConfigParam						audioSrcConfigSetParam;
	VideoEncConfigParam						videoEncConfigSetParam;
	VideoSrcConfigParam						videoSrcConfigSetParam;
	SyncPoint								syncPoint;
	VideoOSDConfig							videoOSDConfig;
	VideoPrivateArea						videoPrivateArea;
	VideoSrcControl							videoSrcControl;
	//------------------Media Service------------------

	//-------------------PTZ Service-------------------
	PTZConfig								ptzConfig;
	PTZStatus								ptzStatus;
	PTZPresets								ptzPresetsGet;
	PTZGotoParam							ptzGotoParam;
	PTZPresetsSet							ptzPresetsSet;
	PTZRemoveParam							ptzRemoveParam;
	PTZAbsoluteMove							ptzAbsoluteMove;
	PTZRelativeMove							ptzRelativeMove;
	PTZContinuousMove						ptzContinuousMove;
	PTZHomePosition							ptzHomePosition;
	PTZStop									ptzStop;
	//-------------------PTZ Service-------------------

	SupportedRules							supportedRules;
	TotalRule								totalRule;
	DeleteRule								deleteRule;
	RuleList								ruleList;

	//------------Metadata Stream Service------------
	MetadataStreamInfo						metadataStreamInfo;
	MetadataSettings						metadataSettings;
	//------------Metadata Stream Service------------
} RCHostInfo;


//*******************Parser************************

unsigned Cmd_HostParser(IN RCHostInfo * deviceInfo, IN Word command, const Byte * Buffer, unsigned bufferLength);
unsigned parseReadyCmd_Host(IN RCHostInfo * deviceInfo);

//*******************Parser************************


//*******************Get*************************

//----------------------General--------------------------------
unsigned Cmd_Send(IN RCHostInfo* deviceInfo, IN Word command);
unsigned Cmd_generalGet(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_getWithByte(IN RCHostInfo * deviceInfo, IN Byte byteData, IN Word command);
unsigned Cmd_getWithString(IN RCHostInfo * deviceInfo, IN const RCString * string, IN Word command);
unsigned Cmd_getWithStrings(IN RCHostInfo * deviceInfo, IN const RCString * stringArray, IN unsigned stringSize, IN Word command);
//----------------------General--------------------------------

//---------------------ccHDtv Service--------------------------
unsigned Cmd_getHwRegisterValues(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setTxDeviceAddressID(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setCalibrationTable(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setHwRegisterValues(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setAdvanceOptions(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setTPSInfo(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setTransmissionParameters(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setPSITable(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setNitLocation(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setSdtServiceConfiguration(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setEITInformation(IN RCHostInfo * deviceInfo, IN Word command);
//---------------------ccHDtv Service--------------------------

//------------------Device Management Service----------------
unsigned Cmd_setSystemDateAndTime(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setOSDInformation(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_upgradeSystemFirmware(IN RCHostInfo * deviceInfo, IN Word command);
//------------------Device Management Service----------------

//--------------------Device_IO Service-----------------------
unsigned Cmd_setRelayOutputSettings(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setRelayOutputState(IN RCHostInfo * deviceInfo, IN Word command) ;
//--------------------Device_IO Service-----------------------

//----------------------Imaging Service------------------------
unsigned Cmd_move(IN RCHostInfo * deviceInfo, IN Word command) ;
unsigned Cmd_setImagingSettings(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setUserDefinedSettings(IN RCHostInfo * deviceInfo, IN Word command);
//----------------------Imaging Service------------------------

//----------------------Media Service--------------------------
unsigned Cmd_setVideoSrcControl(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setVideoPrivateArea(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setVideoOSDConfig(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setAudioEncConfig(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setAudioSrcConfig(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setVideoEncConfig(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setVideoSrcConfig(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_getVideoSrcConfigOptions(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_getVideoEncConfigOptions(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_getAudioSrcConfigOptions(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_getAudioEncConfigOptions(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_getVideoOSDConfig(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_getVideoPrivateArea(IN RCHostInfo * deviceInfo, IN Word command);
//----------------------Media Service--------------------------

//------------------------PTZ Service--------------------------
unsigned Cmd_gotoPreset(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_gotoPreset(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setPreset(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_absoluteMove(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_relativeMove(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_continuousMove(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_gotoHomePosition(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_PTZStop(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_removePreset(IN RCHostInfo * deviceInfo, IN Word command);
//------------------------PTZ Service--------------------------

//------------------Video Analytics Service---------------------
unsigned Cmd_createRule(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_modifyRule(IN RCHostInfo * deviceInfo, IN Word command);
//------------------Video Analytics Service---------------------

//------------------Metadata Stream Service---------------------
unsigned Cmd_getMetadataSettings(IN RCHostInfo * deviceInfo, IN Word command);
unsigned Cmd_setMetadataSettings(IN RCHostInfo * deviceInfo, IN Word command);
//------------------Metadata Stream Service---------------------

//*******************Get*************************
#endif
