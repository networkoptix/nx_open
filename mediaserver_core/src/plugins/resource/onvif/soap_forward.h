#pragma once

// All the following classes are defined in <onvif/soapStub.h>
// In order not to include it here, we place forward declarations.
//-------------------------------------------------------------------------------------------------
class _onvifDevice__CreateUsers;
class _onvifDevice__CreateUsersResponse;
class _onvifDevice__GetServiceCapabilities;
class _onvifDevice__GetServiceCapabilitiesResponse;
class _onvifDevice__GetCapabilities;
class _onvifDevice__GetCapabilitiesResponse;
class _onvifDevice__GetSystemDateAndTime;
class _onvifDevice__GetSystemDateAndTimeResponse;
class _onvifDevice__GetDeviceInformation;
class _onvifDevice__GetDeviceInformationResponse;
class _onvifDevice__GetNetworkInterfaces;
class _onvifDevice__GetNetworkInterfacesResponse;
class _onvifDevice__SetSystemFactoryDefault;
class _onvifDevice__SetSystemFactoryDefaultResponse;
class _onvifDevice__GetRelayOutputs;
class _onvifDevice__GetRelayOutputsResponse;
class _onvifDevice__SetRelayOutputState;
class _onvifDevice__SetRelayOutputStateResponse;
class _onvifDevice__SetRelayOutputSettings;
class _onvifDevice__SetRelayOutputSettingsResponse;
class _onvifDevice__SystemReboot;
class _onvifDevice__SystemRebootResponse;

typedef _onvifDevice__CreateUsers CreateUsersReq;
typedef _onvifDevice__CreateUsersResponse CreateUsersResp;
typedef _onvifDevice__GetCapabilities CapabilitiesReq;
typedef _onvifDevice__GetCapabilitiesResponse CapabilitiesResp;
typedef _onvifDevice__GetServices GetServicesReq;
typedef _onvifDevice__GetServicesResponse GetServicesResp;
typedef _onvifDevice__GetDeviceInformation DeviceInfoReq;
typedef _onvifDevice__GetDeviceInformationResponse DeviceInfoResp;
typedef _onvifDevice__GetNetworkInterfaces NetIfacesReq;
typedef _onvifDevice__GetNetworkInterfacesResponse NetIfacesResp;
typedef _onvifDevice__SetSystemFactoryDefault FactoryDefaultReq;
typedef _onvifDevice__SetSystemFactoryDefaultResponse FactoryDefaultResp;
typedef _onvifDevice__SystemReboot RebootReq;
typedef _onvifDevice__SystemRebootResponse RebootResp;
//-------------------------------------------------------------------------------------------------
class _onvifDeviceIO__GetDigitalInputs;
class _onvifDeviceIO__GetDigitalInputsResponse;
class _onvifDeviceIO__GetRelayOutputOptions;
class _onvifDeviceIO__GetRelayOutputOptionsResponse;
class _onvifDeviceIO__SetRelayOutputSettings;
class _onvifDeviceIO__SetRelayOutputSettingsResponse;
//-------------------------------------------------------------------------------------------------
class _onvifMedia__GetCompatibleMetadataConfigurations;
class _onvifMedia__GetCompatibleMetadataConfigurationsResponse;
class _onvifMedia__GetCompatibleAudioDecoderConfigurations;
class _onvifMedia__GetCompatibleAudioDecoderConfigurationsResponse;
class _onvifMedia__GetAudioOutputs;
class _onvifMedia__GetAudioOutputsResponse;
class _onvifMedia__AddAudioOutputConfiguration;
class _onvifMedia__AddAudioOutputConfigurationResponse;
class _onvifMedia__AddAudioDecoderConfiguration;
class _onvifMedia__AddAudioDecoderConfigurationResponse;
class _onvifMedia__GetAudioOutputConfigurations;
class _onvifMedia__GetAudioOutputConfigurationsResponse;
class _onvifMedia__AddAudioEncoderConfiguration;
class _onvifMedia__AddAudioEncoderConfigurationResponse;
class _onvifMedia__AddAudioSourceConfiguration;
class _onvifMedia__AddAudioSourceConfigurationResponse;
class _onvifMedia__AddVideoEncoderConfiguration;
class _onvifMedia__AddVideoEncoderConfigurationResponse;
class _onvifMedia__GetVideoSources;
class _onvifMedia__GetVideoSourcesResponse;
class _onvifMedia__AddVideoSourceConfiguration;
class _onvifMedia__AddVideoSourceConfigurationResponse;
class _onvifMedia__AddPTZConfiguration;
class _onvifMedia__AddPTZConfigurationResponse;
class _onvifMedia__CreateProfile;
class _onvifMedia__CreateProfileResponse;
class _onvifMedia__GetAudioEncoderConfigurationOptions;
class _onvifMedia__GetAudioEncoderConfigurationOptionsResponse;
class _onvifMedia__GetAudioEncoderConfigurations;
class _onvifMedia__GetAudioEncoderConfigurationsResponse;
class _onvifMedia__GetAudioSourceConfigurations;
class _onvifMedia__GetAudioSourceConfigurationsResponse;
class _onvifMedia__GetProfile;
class _onvifMedia__GetProfile;
class _onvifMedia__GetProfileResponse;
class _onvifMedia__GetProfiles;
class _onvifMedia__GetProfilesResponse;
class _onvifMedia__GetStreamUri;
class _onvifMedia__GetStreamUriResponse;
class _onvifMedia__GetVideoEncoderConfigurationOptions;
class _onvifMedia__GetVideoEncoderConfigurationOptionsResponse;
class _onvifMedia__GetVideoEncoderConfiguration;
class _onvifMedia__GetVideoEncoderConfigurationResponse;
class _onvifMedia__GetVideoEncoderConfigurations;
class _onvifMedia__GetVideoEncoderConfigurationsResponse;
class _onvifMedia__GetVideoSourceConfigurationOptions;
class _onvifMedia__GetVideoSourceConfigurationOptionsResponse;
class _onvifMedia__GetVideoSourceConfigurations;
class _onvifMedia__GetVideoSourceConfigurationsResponse;
class _onvifMedia__SetAudioEncoderConfiguration;
class _onvifMedia__SetAudioEncoderConfigurationResponse;
class _onvifMedia__SetAudioSourceConfiguration;
class _onvifMedia__SetAudioSourceConfigurationResponse;
class _onvifMedia__SetVideoEncoderConfiguration;
class _onvifMedia__SetVideoEncoderConfigurationResponse;
class _onvifMedia__SetVideoSourceConfiguration;
class _onvifMedia__SetVideoSourceConfigurationResponse;

typedef _onvifMedia__GetCompatibleMetadataConfigurations CompatibleMetadataConfiguration;
typedef _onvifMedia__GetCompatibleMetadataConfigurationsResponse CompatibleMetadataConfigurationResp;
typedef _onvifMedia__AddAudioOutputConfiguration AddAudioOutputConfigurationReq;
typedef _onvifMedia__AddAudioOutputConfigurationResponse AddAudioOutputConfigurationResp;
typedef _onvifMedia__AddAudioDecoderConfiguration AddAudioDecoderConfigurationReq;
typedef _onvifMedia__AddAudioDecoderConfigurationResponse AddAudioDecoderConfigurationResp;
typedef _onvifMedia__GetCompatibleAudioDecoderConfigurations GetCompatibleAudioDecoderConfigurationsReq;
typedef _onvifMedia__GetCompatibleAudioDecoderConfigurationsResponse GetCompatibleAudioDecoderConfigurationsResp;
typedef _onvifMedia__GetAudioOutputConfigurations GetAudioOutputConfigurationsReq;
typedef _onvifMedia__GetAudioOutputConfigurationsResponse GetAudioOutputConfigurationsResp;
typedef _onvifMedia__AddAudioEncoderConfiguration AddAudioConfigReq;
typedef _onvifMedia__AddAudioEncoderConfigurationResponse AddAudioConfigResp;
typedef _onvifMedia__AddAudioSourceConfiguration AddAudioSrcConfigReq;
typedef _onvifMedia__AddAudioSourceConfigurationResponse AddAudioSrcConfigResp;
typedef _onvifMedia__AddVideoEncoderConfiguration AddVideoConfigReq;
typedef _onvifMedia__AddVideoEncoderConfigurationResponse AddVideoConfigResp;
typedef _onvifMedia__AddPTZConfiguration AddPTZConfigReq;
typedef _onvifMedia__AddPTZConfigurationResponse AddPTZConfigResp;
typedef _onvifMedia__AddVideoSourceConfiguration AddVideoSrcConfigReq;
typedef _onvifMedia__AddVideoSourceConfigurationResponse AddVideoSrcConfigResp;
typedef _onvifMedia__CreateProfile CreateProfileReq;
typedef _onvifMedia__CreateProfileResponse CreateProfileResp;
typedef _onvifMedia__GetAudioEncoderConfigurationOptions AudioOptionsReq;
typedef _onvifMedia__GetAudioEncoderConfigurationOptionsResponse AudioOptionsResp;
typedef _onvifMedia__GetAudioEncoderConfigurations AudioConfigsReq;
typedef _onvifMedia__GetAudioEncoderConfigurationsResponse AudioConfigsResp;
typedef _onvifMedia__GetAudioSourceConfigurations AudioSrcConfigsReq;
typedef _onvifMedia__GetAudioSourceConfigurationsResponse AudioSrcConfigsResp;
typedef _onvifMedia__GetProfile ProfileReq;
typedef _onvifMedia__GetProfileResponse ProfileResp;
typedef _onvifMedia__GetProfiles ProfilesReq;
typedef _onvifMedia__GetProfilesResponse ProfilesResp;
typedef _onvifMedia__GetStreamUri StreamUriReq;
typedef _onvifMedia__GetStreamUriResponse StreamUriResp;
typedef _onvifMedia__GetVideoEncoderConfigurationOptions VideoOptionsReq;
typedef _onvifMedia__GetVideoEncoderConfigurationOptionsResponse VideoOptionsResp;
typedef _onvifMedia__GetVideoEncoderConfiguration VideoConfigReq;
typedef _onvifMedia__GetVideoEncoderConfigurationResponse VideoConfigResp;
typedef _onvifMedia__GetVideoEncoderConfigurations VideoConfigsReq;
typedef _onvifMedia__GetVideoEncoderConfigurationsResponse VideoConfigsResp;
typedef _onvifMedia__GetVideoSourceConfigurationOptions VideoSrcOptionsReq;
typedef _onvifMedia__GetVideoSourceConfigurationOptionsResponse VideoSrcOptionsResp;
typedef _onvifMedia__GetVideoSourceConfigurations VideoSrcConfigsReq;
typedef _onvifMedia__GetVideoSourceConfigurationsResponse VideoSrcConfigsResp;
typedef _onvifMedia__SetAudioEncoderConfiguration SetAudioConfigReq;
typedef _onvifMedia__SetAudioEncoderConfigurationResponse SetAudioConfigResp;
typedef _onvifMedia__SetAudioSourceConfiguration SetAudioSrcConfigReq;
typedef _onvifMedia__SetAudioSourceConfigurationResponse SetAudioSrcConfigResp;
typedef _onvifMedia__SetVideoEncoderConfiguration SetVideoConfigReq;
typedef _onvifMedia__SetVideoEncoderConfigurationResponse SetVideoConfigResp;
typedef _onvifMedia__SetVideoSourceConfiguration SetVideoSrcConfigReq;
typedef _onvifMedia__SetVideoSourceConfigurationResponse SetVideoSrcConfigResp;
//-------------------------------------------------------------------------------------------------
class _onvifImg__GetImagingSettings;
class _onvifImg__GetImagingSettingsResponse;
class _onvifImg__GetOptions;
class _onvifImg__GetOptionsResponse;
class _onvifImg__GetMoveOptions;
class _onvifImg__GetMoveOptionsResponse;
class _onvifImg__Move;
class _onvifImg__MoveResponse;
class _onvifImg__SetImagingSettings;
class _onvifImg__SetImagingSettingsResponse;

typedef _onvifImg__GetOptions ImagingOptionsReq;
typedef _onvifImg__GetOptionsResponse ImagingOptionsResp;
typedef _onvifImg__GetImagingSettings ImagingSettingsReq;
typedef _onvifImg__GetImagingSettingsResponse ImagingSettingsResp;
typedef _onvifImg__SetImagingSettings SetImagingSettingsReq;
typedef _onvifImg__SetImagingSettingsResponse SetImagingSettingsResp;
//-------------------------------------------------------------------------------------------------
class _onvifPtz__GetServiceCapabilities;
class _onvifPtz__GetServiceCapabilitiesResponse;
class _onvifPtz__AbsoluteMove;
class _onvifPtz__AbsoluteMoveResponse;
class _onvifPtz__RelativeMove;
class _onvifPtz__RelativeMoveResponse;
class _onvifPtz__GetStatus;
class _onvifPtz__GetStatusResponse;
class _onvifPtz__GetNode;
class _onvifPtz__GetNodeResponse;
class _onvifPtz__GetNodes;
class _onvifPtz__GetNodesResponse;
class _onvifPtz__GetConfigurations;
class _onvifPtz__GetConfigurationsResponse;
class _onvifPtz__GotoPreset;
class _onvifPtz__GotoPresetResponse;
class _onvifPtz__GetPresets;
class _onvifPtz__GetPresetsResponse;
class _onvifPtz__SetPreset;
class _onvifPtz__SetPresetResponse;
class _onvifPtz__RemovePreset;
class _onvifPtz__RemovePresetResponse;
class _onvifPtz__ContinuousMove;
class _onvifPtz__ContinuousMoveResponse;
class _onvifPtz__Stop;
class _onvifPtz__StopResponse;

typedef _onvifPtz__AbsoluteMove AbsoluteMoveReq;
typedef _onvifPtz__AbsoluteMoveResponse AbsoluteMoveResp;
typedef _onvifPtz__RelativeMove RelativeMoveReq;
typedef _onvifPtz__RelativeMoveResponse RelativeMoveResp;
typedef _onvifPtz__GetServiceCapabilities PtzGetServiceCapabilitiesReq;
typedef _onvifPtz__GetServiceCapabilitiesResponse PtzPtzGetServiceCapabilitiesResp;
typedef _onvifPtz__GotoPreset GotoPresetReq;
typedef _onvifPtz__GotoPresetResponse GotoPresetResp;
typedef _onvifPtz__SetPreset SetPresetReq;
typedef _onvifPtz__SetPresetResponse SetPresetResp;
typedef _onvifPtz__GetPresets GetPresetsReq;
typedef _onvifPtz__GetPresetsResponse GetPresetsResp;
typedef _onvifPtz__RemovePreset RemovePresetReq;
typedef _onvifPtz__RemovePresetResponse RemovePresetResp;
//-------------------------------------------------------------------------------------------------
class _oasisWsnB2__Subscribe;
class _oasisWsnB2__SubscribeResponse;
class _oasisWsnB2__CreatePullPoint;
class _oasisWsnB2__CreatePullPointResponse;
class _oasisWsnB2__Renew;
class _oasisWsnB2__RenewResponse;
class _oasisWsnB2__Unsubscribe;
class _oasisWsnB2__UnsubscribeResponse;
//-------------------------------------------------------------------------------------------------
class _onvifEvents__CreatePullPointSubscription;
class _onvifEvents__CreatePullPointSubscriptionResponse;
class _onvifEvents__PullMessages;
class _onvifEvents__PullMessagesResponse;
//-------------------------------------------------------------------------------------------------
