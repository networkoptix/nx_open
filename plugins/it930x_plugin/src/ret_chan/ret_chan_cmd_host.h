#ifndef RETURN_CHANNEL_CMD_HOST_H
#define RETURN_CHANNEL_CMD_HOST_H

#include "ret_chan_cmd.h"

typedef struct TxRC
{
    TxRC();
    ~TxRC();

	//--------------General-------------------
	Security								security;
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

#endif
