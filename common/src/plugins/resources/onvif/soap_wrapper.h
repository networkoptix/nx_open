#ifndef onvif_soap_wrapper_h
#define onvif_soap_wrapper_h

#include "onvif_helper.h"

struct soap;
class DeviceBindingProxy;
class MediaBindingProxy;
class PTZBindingProxy;
class ImagingBindingProxy;

class _onvifDevice__CreateUsers;
class _onvifDevice__CreateUsersResponse;
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
class _onvifDevice__SystemReboot;
class _onvifDevice__SystemRebootResponse;

typedef _onvifDevice__CreateUsers CreateUsersReq;
typedef _onvifDevice__CreateUsersResponse CreateUsersResp;
typedef _onvifDevice__GetCapabilities CapabilitiesReq;
typedef _onvifDevice__GetCapabilitiesResponse CapabilitiesResp;
typedef _onvifDevice__GetDeviceInformation DeviceInfoReq;
typedef _onvifDevice__GetDeviceInformationResponse DeviceInfoResp;
typedef _onvifDevice__GetNetworkInterfaces NetIfacesReq;
typedef _onvifDevice__GetNetworkInterfacesResponse NetIfacesResp;
typedef _onvifDevice__SetSystemFactoryDefault FactoryDefaultReq;
typedef _onvifDevice__SetSystemFactoryDefaultResponse FactoryDefaultResp;
typedef _onvifDevice__SystemReboot RebootReq;
typedef _onvifDevice__SystemRebootResponse RebootResp;


class _onvifMedia__AddAudioEncoderConfiguration;
class _onvifMedia__AddAudioEncoderConfigurationResponse;
class _onvifMedia__AddAudioSourceConfiguration;
class _onvifMedia__AddAudioSourceConfigurationResponse;
class _onvifMedia__AddVideoEncoderConfiguration;
class _onvifMedia__AddVideoEncoderConfigurationResponse;
class _onvifMedia__AddVideoSourceConfiguration;
class _onvifMedia__AddVideoSourceConfigurationResponse;
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

typedef _onvifMedia__AddAudioEncoderConfiguration AddAudioConfigReq;
typedef _onvifMedia__AddAudioEncoderConfigurationResponse AddAudioConfigResp;
typedef _onvifMedia__AddAudioSourceConfiguration AddAudioSrcConfigReq;
typedef _onvifMedia__AddAudioSourceConfigurationResponse AddAudioSrcConfigResp;
typedef _onvifMedia__AddVideoEncoderConfiguration AddVideoConfigReq;
typedef _onvifMedia__AddVideoEncoderConfigurationResponse AddVideoConfigResp;
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


class _onvifImg__GetImagingSettings;
class _onvifImg__GetImagingSettingsResponse;
class _onvifImg__GetOptions;
class _onvifImg__GetOptionsResponse;
class _onvifImg__SetImagingSettings;
class _onvifImg__SetImagingSettingsResponse;

typedef _onvifImg__GetImagingSettings ImagingSettingsReq;
typedef _onvifImg__GetImagingSettingsResponse ImagingSettingsResp;
typedef _onvifImg__GetOptions ImagingOptionsReq;
typedef _onvifImg__GetOptionsResponse ImagingOptionsResp;
typedef _onvifImg__SetImagingSettings SetImagingSettingsReq;
typedef _onvifImg__SetImagingSettingsResponse SetImagingSettingsResp;

class _onvifPtz__GetServiceCapabilities;
class _onvifPtz__GetServiceCapabilitiesResponse;
class _onvifPtz__AbsoluteMove;
class _onvifPtz__AbsoluteMoveResponse;
class _onvifPtz__RelativeMove;
class _onvifPtz__RelativeMoveResponse;

typedef _onvifPtz__AbsoluteMove AbsoluteMoveReq;
typedef _onvifPtz__AbsoluteMoveResponse AbsoluteMoveResp;
typedef _onvifPtz__RelativeMove RelativeMoveReq;
typedef _onvifPtz__RelativeMoveResponse RelativeMoveResp;
typedef _onvifPtz__GetServiceCapabilities PtzGetServiceCapabilitiesReq;
typedef _onvifPtz__GetServiceCapabilitiesResponse PtzPtzGetServiceCapabilitiesResp;

//
// SoapWrapper
//

template <class T>
class SoapWrapper
{
    char* m_login;
    char* m_passwd;
    bool invoked;
    int m_timeDrift;

protected:

    T* m_soapProxy;
    char* m_endpoint;

public:

    SoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd, int _timeDrift);
    virtual ~SoapWrapper();

    soap* getSoap();
    const char* getLogin();
    const char* getPassword();
    int getTimeDrift();
    const QString getLastError();
    const QString getEndpointUrl();
    bool isNotAuthenticated();
    bool isConflictError();

private:
    SoapWrapper();
    SoapWrapper(const SoapWrapper<T>&);
    void cleanLoginPassword();

protected:
    void beforeMethodInvocation();
    void setLoginPassword(const std::string& login, const std::string& passwd);
};

//
// DeviceSoapWrapper
//

class DeviceSoapWrapper: public SoapWrapper<DeviceBindingProxy>
{
    PasswordHelper& passwordsData;

public:

    //TODO:UTF unuse std::string
    DeviceSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd, int _timeDrift);
    virtual ~DeviceSoapWrapper();

    //Input: normalized manufacturer
    bool fetchLoginPassword(const QString& manufacturer);

    int getCapabilities(CapabilitiesReq& request, CapabilitiesResp& response);
    int getDeviceInformation(DeviceInfoReq& request, DeviceInfoResp& response);
    int getNetworkInterfaces(NetIfacesReq& request, NetIfacesResp& response);
    int GetSystemDateAndTime(_onvifDevice__GetSystemDateAndTime& request, _onvifDevice__GetSystemDateAndTimeResponse& response);

    int createUsers(CreateUsersReq& request, CreateUsersResp& response);

    int systemFactoryDefaultHard(FactoryDefaultReq& request, FactoryDefaultResp& response);
    int systemFactoryDefaultSoft(FactoryDefaultReq& request, FactoryDefaultResp& response);
    int systemReboot(RebootReq& request, RebootResp& response);

private:
    DeviceSoapWrapper();
    DeviceSoapWrapper(const DeviceSoapWrapper&);
};

//
// MediaSoapWrapper
//

class MediaSoapWrapper: public SoapWrapper<MediaBindingProxy>
{
    PasswordHelper& passwordsData;

public:

    MediaSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd, int _timeDrift);
    virtual ~MediaSoapWrapper();

    int getAudioEncoderConfigurationOptions(AudioOptionsReq& request, AudioOptionsResp& response);
    int getAudioEncoderConfigurations(AudioConfigsReq& request, AudioConfigsResp& response);
    int getAudioSourceConfigurations(AudioSrcConfigsReq& request, AudioSrcConfigsResp& response);
    int getProfile(ProfileReq& request, ProfileResp& response);
    int getProfiles(ProfilesReq& request, ProfilesResp& response);
    int getStreamUri(StreamUriReq& request, StreamUriResp& response);
    int getVideoEncoderConfigurationOptions(VideoOptionsReq& request, VideoOptionsResp& response);
    int getVideoEncoderConfiguration(VideoConfigReq& request, VideoConfigResp& response);
    int getVideoEncoderConfigurations(VideoConfigsReq& request, VideoConfigsResp& response);
    int getVideoSourceConfigurationOptions(VideoSrcOptionsReq& request, VideoSrcOptionsResp& response);
    int getVideoSourceConfigurations(VideoSrcConfigsReq& request, VideoSrcConfigsResp& response);

    int addAudioEncoderConfiguration(AddAudioConfigReq& request, AddAudioConfigResp& response);
    int addAudioSourceConfiguration(AddAudioSrcConfigReq& request, AddAudioSrcConfigResp& response);
    int addVideoEncoderConfiguration(AddVideoConfigReq& request, AddVideoConfigResp& response);
    int addVideoSourceConfiguration(AddVideoSrcConfigReq& request, AddVideoSrcConfigResp& response);

    int createProfile(CreateProfileReq& request, CreateProfileResp& response);

    int setAudioEncoderConfiguration(SetAudioConfigReq& request, SetAudioConfigResp& response);
    int setAudioSourceConfiguration(SetAudioSrcConfigReq& request, SetAudioSrcConfigResp& response);
    int setVideoEncoderConfiguration(SetVideoConfigReq& request, SetVideoConfigResp& response);
    int setVideoSourceConfiguration(SetVideoSrcConfigReq& request, SetVideoSrcConfigResp& response);

private:
    MediaSoapWrapper();
    MediaSoapWrapper(const MediaSoapWrapper&);
};

typedef QSharedPointer<MediaSoapWrapper> MediaSoapWrapperPtr;

//
// PtzSoapWrapper
//

class PtzSoapWrapper: public SoapWrapper<PTZBindingProxy>
{
    PasswordHelper& passwordsData;

public:

    PtzSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd, int _timeDrift);
    virtual ~PtzSoapWrapper();

    int GetServiceCapabilities(PtzGetServiceCapabilitiesReq& request, PtzPtzGetServiceCapabilitiesResp& response);
    int doAbsoluteMove(AbsoluteMoveReq& request, AbsoluteMoveResp& response);
private:
    PtzSoapWrapper();
    PtzSoapWrapper(const PtzSoapWrapper&);
};

typedef QSharedPointer<PtzSoapWrapper> PtzSoapWrapperPtr;

//
// ImagingSoapWrapper
//

class ImagingSoapWrapper: public SoapWrapper<ImagingBindingProxy>
{
    PasswordHelper& passwordsData;

public:

    ImagingSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd, int _timeDrift);
    virtual ~ImagingSoapWrapper();

    int getImagingSettings(ImagingSettingsReq& request, ImagingSettingsResp& response);
    int getOptions(ImagingOptionsReq& request, ImagingOptionsResp& response);

    int setImagingSettings(SetImagingSettingsReq& request, SetImagingSettingsResp& response);

private:
    ImagingSoapWrapper();
    ImagingSoapWrapper(const PtzSoapWrapper&);
};

typedef QSharedPointer<ImagingSoapWrapper> ImagingSoapWrapperPtr;

#endif //onvif_soap_wrapper_h
