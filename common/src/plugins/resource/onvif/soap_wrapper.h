#ifndef onvif_soap_wrapper_h
#define onvif_soap_wrapper_h

#ifdef ENABLE_ONVIF

#include <QSharedPointer>

#include "onvif_helper.h"

struct soap;
class DeviceBindingProxy;
class DeviceIOBindingProxy;
class MediaBindingProxy;
class PTZBindingProxy;
class ImagingBindingProxy;
class NotificationProducerBindingProxy;
class CreatePullPointBindingProxy;
class PullPointSubscriptionBindingProxy;
class EventBindingProxy;
class SubscriptionManagerBindingProxy;

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
class _onvifMedia__GetCompatibleMetadataConfigurations;
class _onvifMedia__GetCompatibleMetadataConfigurationsResponse;


class _onvifDeviceIO__GetDigitalInputs;
class _onvifDeviceIO__GetDigitalInputsResponse;
class _onvifDeviceIO__GetRelayOutputOptions;
class _onvifDeviceIO__GetRelayOutputOptionsResponse;
class _onvifDeviceIO__SetRelayOutputSettings;
class _onvifDeviceIO__SetRelayOutputSettingsResponse;
class _onvifMedia__GetVideoSources;
class _onvifMedia__GetVideoSourcesResponse;

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
typedef _onvifMedia__GetCompatibleMetadataConfigurations CompatibleMetadataConfiguration;
typedef _onvifMedia__GetCompatibleMetadataConfigurationsResponse CompatibleMetadataConfigurationResp;


class _onvifMedia__AddAudioEncoderConfiguration;
class _onvifMedia__AddAudioEncoderConfigurationResponse;
class _onvifMedia__AddAudioSourceConfiguration;
class _onvifMedia__AddAudioSourceConfigurationResponse;
class _onvifMedia__AddVideoEncoderConfiguration;
class _onvifMedia__AddVideoEncoderConfigurationResponse;
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
class _onvifPtz__GetStatus;
class _onvifPtz__GetStatusResponse;
class _onvifPtz__GetNode;
class _onvifPtz__GetNodeResponse;
class _onvifPtz__GetNodes;
class _onvifPtz__GetNodesResponse;
class _onvifPtz__GetConfigurations;
class _onvifPtz__GetConfigurationsResponse;

typedef _onvifPtz__AbsoluteMove AbsoluteMoveReq;
typedef _onvifPtz__AbsoluteMoveResponse AbsoluteMoveResp;
typedef _onvifPtz__RelativeMove RelativeMoveReq;
typedef _onvifPtz__RelativeMoveResponse RelativeMoveResp;
typedef _onvifPtz__GetServiceCapabilities PtzGetServiceCapabilitiesReq;
typedef _onvifPtz__GetServiceCapabilitiesResponse PtzPtzGetServiceCapabilitiesResp;

class _onvifPtz__GotoPreset;
class _onvifPtz__GotoPresetResponse;
class _onvifPtz__SetPreset;
class _onvifPtz__SetPresetResponse;
class _onvifPtz__GetPresets;
class _onvifPtz__GetPresetsResponse;
class _onvifPtz__RemovePreset;
class _onvifPtz__RemovePresetResponse;
typedef _onvifPtz__GotoPreset GotoPresetReq;
typedef _onvifPtz__GotoPresetResponse GotoPresetResp;
typedef _onvifPtz__SetPreset SetPresetReq;
typedef _onvifPtz__SetPresetResponse SetPresetResp;
typedef _onvifPtz__GetPresets GetPresetsReq;
typedef _onvifPtz__GetPresetsResponse GetPresetsResp;
typedef _onvifPtz__RemovePreset RemovePresetReq;
typedef _onvifPtz__RemovePresetResponse RemovePresetResp;

class _onvifPtz__ContinuousMove;
class _onvifPtz__ContinuousMoveResponse;
class _onvifPtz__Stop;
class _onvifPtz__StopResponse;
class _onvifPtz__Stop;
class onvifPtz__StopResponse;

class _oasisWsnB2__Subscribe;
class _oasisWsnB2__SubscribeResponse;

class _oasisWsnB2__CreatePullPoint;
class _oasisWsnB2__CreatePullPointResponse;

class _onvifEvents__CreatePullPointSubscription;
class _onvifEvents__CreatePullPointSubscriptionResponse;

class _onvifEvents__PullMessages;
class _onvifEvents__PullMessagesResponse;

class _oasisWsnB2__Renew;
class _oasisWsnB2__RenewResponse;

class _oasisWsnB2__Unsubscribe;
class _oasisWsnB2__UnsubscribeResponse;



template <class T>
class SoapWrapper
{
public:
    SoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive);
    virtual ~SoapWrapper();

    const T* getProxy() const { return m_soapProxy; }
    T* getProxy() { return m_soapProxy; }
    const char* endpoint() const { return m_endpoint; }
    QString getLogin();
    QString getPassword();
    int getTimeDrift();
    const QString getLastError();
    const QString getEndpointUrl();
    bool isNotAuthenticated();
    bool isConflictError();
    void setLogin(const QString& login);
    void setPassword(const QString& passwd);

    //!Invokes method \a methodToInvoke, which is member of \a T, with pre-supplied endpoint, username and password
    template<class RequestType, class ResponseType>
    int invokeMethod(
        int (T::*methodToInvoke)( const char*, const char*, RequestType*, ResponseType* ),
        RequestType* const request,
        ResponseType* const response )
    {
        beforeMethodInvocation();
        return (m_soapProxy->*methodToInvoke)( m_endpoint, NULL, request, response );
    }

protected:
    T* m_soapProxy;
    char* m_endpoint;

    void beforeMethodInvocation();

    int m_timeDrift;

private:
    Q_DISABLE_COPY(SoapWrapper);

    QString m_login;
    QString m_passwd;
    bool m_invoked;
};



class DeviceSoapWrapper: public SoapWrapper<DeviceBindingProxy>
{
public:
    //TODO: #vasilenko UTF unuse std::string
    DeviceSoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive = false);
    virtual ~DeviceSoapWrapper();

    //Input: normalized manufacturer
    bool fetchLoginPassword(const QString& manufacturer);

    int getServiceCapabilities( _onvifDevice__GetServiceCapabilities& request, _onvifDevice__GetServiceCapabilitiesResponse& response );
    int getRelayOutputs( _onvifDevice__GetRelayOutputs& request, _onvifDevice__GetRelayOutputsResponse& response );
    int setRelayOutputState( _onvifDevice__SetRelayOutputState& request, _onvifDevice__SetRelayOutputStateResponse& response );
    int setRelayOutputSettings( _onvifDevice__SetRelayOutputSettings& request, _onvifDevice__SetRelayOutputSettingsResponse& response );

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
    void calcTimeDrift();

    PasswordHelper &m_passwordsData;
};



class DeviceIOWrapper: public SoapWrapper<DeviceIOBindingProxy>
{
public:
    DeviceIOWrapper(const std::string& endpoint, const QString &login, const QString &passwd, int timeDrift, bool tcpKeepAlive = false );
    virtual ~DeviceIOWrapper();

    int getDigitalInputs( _onvifDeviceIO__GetDigitalInputs& request, _onvifDeviceIO__GetDigitalInputsResponse& response );
    int getRelayOutputs( _onvifDevice__GetRelayOutputs& request, _onvifDevice__GetRelayOutputsResponse& response );
    int getRelayOutputOptions( _onvifDeviceIO__GetRelayOutputOptions& request, _onvifDeviceIO__GetRelayOutputOptionsResponse& response );
    int setRelayOutputSettings( _onvifDeviceIO__SetRelayOutputSettings& request, _onvifDeviceIO__SetRelayOutputSettingsResponse& response );
};



class MediaSoapWrapper: public SoapWrapper<MediaBindingProxy>
{
public:
    MediaSoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive = false);
    virtual ~MediaSoapWrapper();

    int getAudioEncoderConfigurationOptions(AudioOptionsReq& request, AudioOptionsResp& response);
    int getAudioEncoderConfigurations(AudioConfigsReq& request, AudioConfigsResp& response);
    int getAudioSourceConfigurations(AudioSrcConfigsReq& request, AudioSrcConfigsResp& response);
    int getProfile(ProfileReq& request, ProfileResp& response);
    int getProfiles(ProfilesReq& request, ProfilesResp& response);
    int getStreamUri(StreamUriReq& request, StreamUriResp& response);
    int getVideoEncoderConfigurationOptions(VideoOptionsReq& request, VideoOptionsResp& response);
    int getVideoEncoderConfiguration(VideoConfigReq& request, VideoConfigResp& response);

    int getVideoSources(_onvifMedia__GetVideoSources& request, _onvifMedia__GetVideoSourcesResponse& response);
    int getVideoEncoderConfigurations(VideoConfigsReq& request, VideoConfigsResp& response);
    int getVideoSourceConfigurationOptions(VideoSrcOptionsReq& request, VideoSrcOptionsResp& response);

    int getVideoSourceConfigurations(VideoSrcConfigsReq& request, VideoSrcConfigsResp& response);

    int getCompatibleMetadataConfigurations(CompatibleMetadataConfiguration& request, CompatibleMetadataConfigurationResp& response);

    int addAudioEncoderConfiguration(AddAudioConfigReq& request, AddAudioConfigResp& response);
    int addAudioSourceConfiguration(AddAudioSrcConfigReq& request, AddAudioSrcConfigResp& response);
    int addVideoEncoderConfiguration(AddVideoConfigReq& request, AddVideoConfigResp& response);
    int addPTZConfiguration(AddPTZConfigReq& request, AddPTZConfigResp& response);
    int addVideoSourceConfiguration(AddVideoSrcConfigReq& request, AddVideoSrcConfigResp& response);

    int createProfile(CreateProfileReq& request, CreateProfileResp& response);

    int setAudioEncoderConfiguration(SetAudioConfigReq& request, SetAudioConfigResp& response);
    int setAudioSourceConfiguration(SetAudioSrcConfigReq& request, SetAudioSrcConfigResp& response);
    int setVideoEncoderConfiguration(SetVideoConfigReq& request, SetVideoConfigResp& response);
    int setVideoSourceConfiguration(SetVideoSrcConfigReq& request, SetVideoSrcConfigResp& response);

private:
    PasswordHelper& m_passwordsData;
};

typedef QSharedPointer<MediaSoapWrapper> MediaSoapWrapperPtr;



class PtzSoapWrapper: public SoapWrapper<PTZBindingProxy>
{
public:
    PtzSoapWrapper(const std::string& endpoint, const QString &login, const QString &passwd, int timeDrift, bool tcpKeepAlive = false);
    virtual ~PtzSoapWrapper();

    int doGetConfigurations(_onvifPtz__GetConfigurations& request, _onvifPtz__GetConfigurationsResponse& response);
    int doGetNodes(_onvifPtz__GetNodes& request, _onvifPtz__GetNodesResponse& response);
    int doGetNode(_onvifPtz__GetNode& request, _onvifPtz__GetNodeResponse& response);
    int doGetServiceCapabilities(PtzGetServiceCapabilitiesReq& request, PtzPtzGetServiceCapabilitiesResp& response);
    int doAbsoluteMove(AbsoluteMoveReq& request, AbsoluteMoveResp& response);
    int gotoPreset(GotoPresetReq& request, GotoPresetResp& response);
    int setPreset(SetPresetReq& request, SetPresetResp& response);
    int getPresets(GetPresetsReq& request, GetPresetsResp& response);
    int removePreset(RemovePresetReq& request, RemovePresetResp& response);
    int doContinuousMove(_onvifPtz__ContinuousMove& request, _onvifPtz__ContinuousMoveResponse& response);
    int doGetStatus(_onvifPtz__GetStatus& request, _onvifPtz__GetStatusResponse& response);
    int doStop(_onvifPtz__Stop& request, _onvifPtz__StopResponse& response);

private:
    PasswordHelper& m_passwordsData;
};

typedef QSharedPointer<PtzSoapWrapper> PtzSoapWrapperPtr;



class ImagingSoapWrapper: public SoapWrapper<ImagingBindingProxy>
{
public:
    ImagingSoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive = false);
    virtual ~ImagingSoapWrapper();

    int getImagingSettings(ImagingSettingsReq& request, ImagingSettingsResp& response);
    int getOptions(ImagingOptionsReq& request, ImagingOptionsResp& response);

    int setImagingSettings(SetImagingSettingsReq& request, SetImagingSettingsResp& response);

    int getMoveOptions(_onvifImg__GetMoveOptions &request, _onvifImg__GetMoveOptionsResponse &response);
    int move(_onvifImg__Move &request, _onvifImg__MoveResponse &response);

private:
    PasswordHelper& m_passwordsData;
};

typedef QSharedPointer<ImagingSoapWrapper> ImagingSoapWrapperPtr;



class NotificationProducerSoapWrapper: public SoapWrapper<NotificationProducerBindingProxy>
{
public:
    NotificationProducerSoapWrapper(const std::string& endpoint, const QString &login, const QString &passwd, int timeDrift, bool tcpKeepAlive = false);

    int Subscribe(_oasisWsnB2__Subscribe* const request, _oasisWsnB2__SubscribeResponse* const response);
};



class CreatePullPointSoapWrapper: public SoapWrapper<CreatePullPointBindingProxy>
{
public:
    CreatePullPointSoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive = false);

    int createPullPoint( _oasisWsnB2__CreatePullPoint& request, _oasisWsnB2__CreatePullPointResponse& response );
};



class PullPointSubscriptionWrapper: public SoapWrapper<PullPointSubscriptionBindingProxy>
{
public:
    PullPointSubscriptionWrapper(const std::string& endpoint, const QString &login, const QString &passwd, int timeDrift, bool tcpKeepAlive = false);

    int pullMessages( _onvifEvents__PullMessages& request, _onvifEvents__PullMessagesResponse& response );
};



class EventSoapWrapper: public SoapWrapper<EventBindingProxy>
{
public:
    EventSoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive = false);

    int createPullPointSubscription( _onvifEvents__CreatePullPointSubscription& request, _onvifEvents__CreatePullPointSubscriptionResponse& response );
};



class SubscriptionManagerSoapWrapper: public SoapWrapper<SubscriptionManagerBindingProxy>
{
public:
    SubscriptionManagerSoapWrapper(const std::string& endpoint, const QString &login, const QString &passwd, int _timeDrift, bool tcpKeepAlive = false);

    int renew( _oasisWsnB2__Renew& request, _oasisWsnB2__RenewResponse& response );
    int unsubscribe( _oasisWsnB2__Unsubscribe& request, _oasisWsnB2__UnsubscribeResponse& response );
};

#endif //ENABLE_ONVIF

#endif //onvif_soap_wrapper_h
