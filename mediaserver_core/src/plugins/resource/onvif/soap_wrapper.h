#pragma once

#ifdef ENABLE_ONVIF

#include <list>
#include <map>
#include <ctime>

#include <QSharedPointer>

#include <utils/common/credentials.h>
#include <utils/camera/camera_diagnostics.h>
#include <plugins/resource/onvif/onvif_helper.h>
#include <plugins/resource/onvif/onvif_namespace_registrar.h>
#include <plugins/resource/onvif/soap_helpers.h>
#include <onvif\soapDeviceIOBindingProxy.h>
#include <onvif\soapMediaBindingProxy.h>
#include <onvif\soapMedia2BindingProxy.h>

struct SoapTimeouts
{
    const int kSoapDefaultSendTimeoutSeconds = 5 * 10;
    const int kSoapDefaultRecvTimeoutSeconds = 5 * 10;
    const int kSoapDefaultConnectTimeoutSeconds = 5 * 5;
    const int kSoapDefaultAcceptTimeoutSeconds = 5 * 5;

    SoapTimeouts():
        sendTimeoutSeconds(kSoapDefaultSendTimeoutSeconds),
        recvTimeoutSeconds(kSoapDefaultRecvTimeoutSeconds),
        connectTimeoutSeconds(kSoapDefaultConnectTimeoutSeconds),
        acceptTimeoutSeconds(kSoapDefaultAcceptTimeoutSeconds)
    {};

    SoapTimeouts(const QString& serialized):
        SoapTimeouts()
    {
        if (serialized.isEmpty())
            return;

        const int kTimeoutsCount = 4;

        bool success = false;
        auto timeouts = serialized.split(';');
        auto paramsNum = timeouts.size();

        std::vector<std::chrono::seconds*> fieldsToSet =
        {
            &sendTimeoutSeconds,
            &recvTimeoutSeconds,
            &connectTimeoutSeconds,
            &acceptTimeoutSeconds
        };

        if (paramsNum == 1)
        {
            auto timeout = timeouts[0].toInt(&success);

            if (!success)
                return;

            for (auto i = 0; i < kTimeoutsCount; ++i)
                *(fieldsToSet[i]) = std::chrono::seconds(timeout);
        }
        else if (paramsNum == 4)
        {
            for (auto i = 0; i < kTimeoutsCount; ++i)
            {
                auto timeout = timeouts[i].toInt(&success);

                if (!success)
                    continue;

                *(fieldsToSet[i]) = std::chrono::seconds(timeout);
            }
        }
    }

    QString serialize()
    {
        return lit("%1;%2;%3;%4")
            .arg(sendTimeoutSeconds.count())
            .arg(recvTimeoutSeconds.count())
            .arg(connectTimeoutSeconds.count())
            .arg(acceptTimeoutSeconds.count());
    };

    std::chrono::seconds sendTimeoutSeconds;
    std::chrono::seconds recvTimeoutSeconds;
    std::chrono::seconds connectTimeoutSeconds;
    std::chrono::seconds acceptTimeoutSeconds;
};


struct soap;
class DeviceBindingProxy;
//class DeviceIOBindingProxy;
//class MediaBindingProxy;
//class Media2BindingProxy;
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
class _onvifMedia__GetAudioOutputs;
class _onvifMedia__GetAudioOutputsResponse;

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
typedef _onvifMedia__GetCompatibleMetadataConfigurations CompatibleMetadataConfiguration;
typedef _onvifMedia__GetCompatibleMetadataConfigurationsResponse CompatibleMetadataConfigurationResp;

class _onvifMedia__GetCompatibleAudioDecoderConfigurations;
class _onvifMedia__GetCompatibleAudioDecoderConfigurationsResponse;
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

// Media2 uses universal request type for requesting all configurations
//typedef onvifMedia2__GetConfiguration VideoOptionsReq2;
typedef _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse VideoOptionsResp2;

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
    SoapWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint,
        const QString& login,
        const QString& passwd,
        int timeDrift,
        bool tcpKeepAlive);
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
        int (T::*methodToInvoke)( const char*, const char*, RequestType*, ResponseType& ),
        RequestType* const request,
        ResponseType& response )
    {
        beforeMethodInvocation<RequestType>();
        return (m_soapProxy->*methodToInvoke)( m_endpoint, NULL, request, response );
    }

//protected:
    T* m_soapProxy;
    char* m_endpoint;

    template<typename Request>
    void beforeMethodInvocation()
    {
        using namespace nx::mediaserver_core::plugins;
        if (m_invoked)
        {
            soap_destroy(m_soapProxy->soap);
            soap_end(m_soapProxy->soap);
        }
        else
        {
            m_invoked = true;
        }

        const auto namespaces = onvif::requestNamespaces<Request>();
        //########################################################
        if (namespaces != nullptr)
            soap_set_namespaces(m_soapProxy->soap, namespaces);

        if (!m_login.isEmpty())
        {
            onvif::soapWsseAddUsernameTokenDigest(
                m_soapProxy->soap,
                NULL,
                m_login.toUtf8().constData(),
                m_passwd.toUtf8().constData(),
                time(NULL) + m_timeDrift);
        }
    }

    int m_timeDrift;

//private:
    Q_DISABLE_COPY(SoapWrapper);

    QString m_login;
    QString m_passwd;
    bool m_invoked;
};

class DeviceSoapWrapper: public SoapWrapper<DeviceBindingProxy>
{
public:
    // TODO: #vasilenko UTF unuse std::string
    DeviceSoapWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint,
        const QString& login,
        const QString& passwd,
        int timeDrift,
        bool tcpKeepAlive = false);
    virtual ~DeviceSoapWrapper();

    //Input: normalized manufacturer
    bool fetchLoginPassword(const QString& manufacturer, const QString& model);

    int getServiceCapabilities( _onvifDevice__GetServiceCapabilities& request, _onvifDevice__GetServiceCapabilitiesResponse& response );
    int getRelayOutputs( _onvifDevice__GetRelayOutputs& request, _onvifDevice__GetRelayOutputsResponse& response );
    int setRelayOutputState( _onvifDevice__SetRelayOutputState& request, _onvifDevice__SetRelayOutputStateResponse& response );
    int setRelayOutputSettings( _onvifDevice__SetRelayOutputSettings& request, _onvifDevice__SetRelayOutputSettingsResponse& response );

    int getCapabilities(CapabilitiesReq& request, CapabilitiesResp& response);
    int getServices(GetServicesReq& request, GetServicesResp& response);
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
    QAuthenticator getDefaultPassword(const QString& manufacturer, const QString& model) const;
    std::list<nx::common::utils::Credentials> getPossibleCredentials(
        const QString& manufacturer, const QString& model) const;
    nx::common::utils::Credentials getForcedCredentials(
        const QString& manufacturer, const QString& model);
    void calcTimeDrift();

};

struct RequestParams
{
    std::string endpoint;
    QString login;
    QString passwd;
    int timeDrift = 0;
    bool tcpKeepAlive = false;
    RequestParams() = default;
    RequestParams(std::string endpoint, QString login, QString passwd, int timeDrift, bool tcpKeepAlive = false):
        endpoint(std::move(endpoint)), login(std::move(login)), passwd(std::move(passwd)),
        timeDrift(timeDrift), tcpKeepAlive(tcpKeepAlive) {}
    RequestParams(std::string endpoint, const QAuthenticator& auth, int timeDrift, bool tcpKeepAlive = false):
        endpoint(std::move(endpoint)), login(auth.user()), passwd(auth.password()),
        timeDrift(timeDrift), tcpKeepAlive(tcpKeepAlive) {}
};

template<class Response>
class ResponseTraits;

/**
Generate code of the traits class
The example of code generated:
template<>
class ResponseTraits<_onvifDeviceIO__GetDigitalInputsResponse>
{
public:
    using BindingProxy = DeviceIOBindingProxy;
    using Request = _onvifDeviceIO__GetDigitalInputs;
    using Response = _onvifDeviceIO__GetDigitalInputsResponse;
    using RequestFunc =
        int (DeviceIOBindingProxy::*)(
            const char* soap_endpoint,
            const char* soap_action,
            _onvifDeviceIO__GetDigitalInputs* request,
            _onvifDeviceIO__GetDigitalInputsResponse& response);
    static const RequestFunc requestFunc; //< = &DeviceIOBindingProxy::GetDigitalInputs;
};
*/

#define MAKE_BINDINGPROXY_LEXEME(WEBSERVICE) WEBSERVICE##BindingProxy
#define MAKE_REQUEST_LEXEME(WEBSERVICE, FUNCTION) _onvif##WEBSERVICE##__##FUNCTION
#define MAKE_RESPONSE_LEXEME(WEBSERVICE, FUNCTION) _onvif##WEBSERVICE##__##FUNCTION##Response

#define DECLARE_RESPONSE_TRAITS(WEBSERVICE, FUNCTION) \
template<> \
class ResponseTraits<MAKE_RESPONSE_LEXEME(WEBSERVICE, FUNCTION)> \
{ \
public: \
    using BindingProxy = MAKE_BINDINGPROXY_LEXEME(WEBSERVICE); \
    using Request = MAKE_REQUEST_LEXEME(WEBSERVICE, FUNCTION); \
    using Response = MAKE_RESPONSE_LEXEME(WEBSERVICE, FUNCTION); \
    using RequestFunc = \
        int (BindingProxy::*)( \
            const char* soap_endpoint, \
            const char* soap_action, \
            Request* request, \
            Response& response); \
    static const RequestFunc requestFunc;  \
    static const char funcName[64]; \
};

#define DECLARE_RESPONSE_TRAITS_IRREGULAR(WEBSERVICE, REQUEST, RESPONSE) \
template<> \
class ResponseTraits<RESPONSE> \
{ \
public: \
    using BindingProxy = MAKE_BINDINGPROXY_LEXEME(WEBSERVICE); \
    using Request = REQUEST; \
    using Response = RESPONSE; \
    using RequestFunc = \
        int (BindingProxy::*)( \
            const char* soap_endpoint, \
            const char* soap_action, \
            Request* request, \
            Response& response); \
    static const RequestFunc requestFunc;  \
    static const char funcName[64]; \
};

DECLARE_RESPONSE_TRAITS(DeviceIO, GetDigitalInputs)
DECLARE_RESPONSE_TRAITS_IRREGULAR(DeviceIO, _onvifDevice__GetRelayOutputs, _onvifDevice__GetRelayOutputsResponse)
DECLARE_RESPONSE_TRAITS(DeviceIO, SetRelayOutputSettings)

DECLARE_RESPONSE_TRAITS(Media, GetVideoEncoderConfigurations)
DECLARE_RESPONSE_TRAITS(Media, GetVideoEncoderConfigurationOptions)
DECLARE_RESPONSE_TRAITS(Media, GetProfiles)

DECLARE_RESPONSE_TRAITS_IRREGULAR(Media2, onvifMedia2__GetConfiguration, _onvifMedia2__GetVideoEncoderConfigurationsResponse)
DECLARE_RESPONSE_TRAITS_IRREGULAR(Media2, onvifMedia2__GetConfiguration, _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse)

///////////////////////////////////////////////////////////////////////////////////////////////////
template<class Response>
class ResponseWrapper;

template<class Response>
class ResponseHolder
{
public:
    ResponseHolder(ResponseWrapper<Response>* owner):
        m_responseOwner(owner)
    {
        ++m_responseOwner->responseHolderCount;
    }

    ResponseHolder(const ResponseHolder&) = delete;
    ResponseHolder(ResponseHolder&&) = delete;
    ResponseHolder& operator=(const ResponseHolder&) = delete;
    ResponseHolder& operator=(ResponseHolder&&) = delete;

    ~ResponseHolder()
    {
        if (m_responseOwner)
            --m_responseOwner->responseHolderCount;
    }
    void reset()
    {
        --m_responseOwner->responseHolderCount;
        m_responseOwner = nullptr;
    }
    const Response* operator->() const
    {
        NX_CRITICAL(m_responseOwner);
        return &m_responseOwner->m_response;
    }
    // Forbidden to suppress error-prone code.
    //const Response& operator*() const
    //{
    //    NX_CRITICAL(m_responseOwner);
    //    return m_responseOwner->m_response;
    //}
private:
    ResponseWrapper<Response>* m_responseOwner;
};

template<class Response>
class ResponseWrapper
{
    friend class ResponseHolder<Response>;
public:
    using BindingProxy = typename ResponseTraits<Response>::BindingProxy;
    using Request = typename ResponseTraits<Response>::Request;

    explicit ResponseWrapper(RequestParams params):
        m_wrapper(std::move(params.endpoint), std::move(params.login), std::move(params.passwd),
            params.timeDrift, params.tcpKeepAlive)
    {
        m_response.soap_default(m_response.soap);
    }

    ResponseWrapper(const ResponseWrapper&) = delete;
    ResponseWrapper(ResponseWrapper&&) = delete;
    ResponseWrapper& operator=(const ResponseWrapper&) = delete;
    ResponseWrapper& operator=(ResponseWrapper&&) = delete;

    ~ResponseWrapper()
    {
        NX_CRITICAL(responseHolderCount == 0);
        if (m_wrapper.m_invoked)
        {
            soap_destroy(m_wrapper.m_soapProxy->soap);
            soap_end(m_wrapper.m_soapProxy->soap);
        }
    }

    bool receiveBySoap()
    {
        Request empryRequest;
        return receiveBySoap(empryRequest);
    }

    bool receiveBySoap(/*const*/ Request& request) //< gsoap does not guarantee immutability
    {
        NX_CRITICAL(responseHolderCount == 0);

        m_wrapper.beforeMethodInvocation<Request>();
        const char* ptr = m_wrapper.m_endpoint;

        std::invoke(ResponseTraits<Response>::requestFunc,
            m_wrapper.m_soapProxy, m_wrapper.m_endpoint, nullptr, &request, m_response);

        return soapError() == SOAP_OK;
    }

    void reset()
    {
        if (m_wrapper.m_invoked)
        {
            soap_destroy(m_wrapper.m_soapProxy->soap);
            soap_end(m_wrapper.m_soapProxy->soap);

            m_response.soap_default(m_response.soap);

            m_wrapper.m_invoked = false;
        }

    }

    ResponseHolder<Response> get()
    {
        return ResponseHolder<Response>(this);
    }

    Response& getEphemeralReference()
    {
        return m_response;
    }

    const char* requestFunctionName() const
    {
        return ResponseTraits<Response>::funcName;
    }

    int soapError() const noexcept { return m_wrapper.m_soapProxy->soap->error; }

    QString soapErrorAsString() const
    {
        return SoapErrorHelper::fetchDescription(m_wrapper.m_soapProxy->soap_fault());
    }
    CameraDiagnostics::RequestFailedResult requestFailedResult() const
    {
        return CameraDiagnostics::RequestFailedResult(
            QString(requestFunctionName()), soapErrorAsString());
    }

    operator bool() const noexcept { return soapError() == SOAP_OK; }
    const SoapWrapper<BindingProxy>& innerWrapper() const noexcept { return m_wrapper; }
    QString endpoint() const { return QString::fromLatin1(m_wrapper.endpoint()); }

protected:
    SoapWrapper<BindingProxy> m_wrapper;
    Response m_response;
    int responseHolderCount = 0;
};

namespace DeviceIO {

using DigitalInputs = ResponseWrapper<_onvifDeviceIO__GetDigitalInputsResponse>;
using RelayOutputs = ResponseWrapper<_onvifDevice__GetRelayOutputsResponse>;

using RelayOutputSettingsSettingResult = ResponseWrapper<_onvifDeviceIO__SetRelayOutputSettingsResponse>;
}// namespace DeviceIO

namespace Media {

    using VideoEncoderConfigurations = ResponseWrapper<_onvifMedia__GetVideoEncoderConfigurationsResponse>;
    using VideoEncoderConfigurationOptions = ResponseWrapper<_onvifMedia__GetVideoEncoderConfigurationOptionsResponse>;
    using Profiles = ResponseWrapper<_onvifMedia__GetProfilesResponse>;
}// namespace Media2

namespace Media2 {

using VideoEncoderConfigurations = ResponseWrapper<_onvifMedia2__GetVideoEncoderConfigurationsResponse>;
using VideoEncoderConfigurationOptions = ResponseWrapper<_onvifMedia2__GetVideoEncoderConfigurationOptionsResponse>;
}// namespace Media2

//#TODO szaitsev: Replace DeviceIO::GetRelayOutputs with DeviceIO::GetRelayOutputs and so on.

class DeviceIOWrapper: public SoapWrapper<DeviceIOBindingProxy>
{
public:
    DeviceIOWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint,
        const QString &login,
        const QString &passwd,
        int timeDrift,
        bool tcpKeepAlive = false );
    virtual ~DeviceIOWrapper();

//    int getDigitalInputs( _onvifDeviceIO__GetDigitalInputs& request, _onvifDeviceIO__GetDigitalInputsResponse& response );
//    int getRelayOutputs( _onvifDevice__GetRelayOutputs& request, _onvifDevice__GetRelayOutputsResponse& response );
////    int getRelayOutputOptions( _onvifDeviceIO__GetRelayOutputOptions& request, _onvifDeviceIO__GetRelayOutputOptionsResponse& response );
//    int setRelayOutputSettings_( _onvifDeviceIO__SetRelayOutputSettings& request, _onvifDeviceIO__SetRelayOutputSettingsResponse& response );
};

class MediaSoapWrapper: public SoapWrapper<MediaBindingProxy>
{
public:
    MediaSoapWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint,
        const QString& login,
        const QString& passwd,
        int timeDrift,
        bool tcpKeepAlive = false);
    virtual ~MediaSoapWrapper();

    int getAudioOutputConfigurations(GetAudioOutputConfigurationsReq& request, GetAudioOutputConfigurationsResp& response);
    int addAudioOutputConfiguration(AddAudioOutputConfigurationReq& request, AddAudioOutputConfigurationResp& response);
    int getCompatibleAudioDecoderConfigurations(GetCompatibleAudioDecoderConfigurationsReq& request, GetCompatibleAudioDecoderConfigurationsResp& response);
    int addAudioDecoderConfiguration(AddAudioDecoderConfigurationReq& request, AddAudioDecoderConfigurationResp& response);

    int getAudioEncoderConfigurationOptions(AudioOptionsReq& request, AudioOptionsResp& response);
    int getAudioEncoderConfigurations(AudioConfigsReq& request, AudioConfigsResp& response);
    int getAudioSourceConfigurations(AudioSrcConfigsReq& request, AudioSrcConfigsResp& response);
    int getProfile(ProfileReq& request, ProfileResp& response);
/**/int getProfiles(ProfilesReq& request, ProfilesResp& response);
    int getStreamUri(StreamUriReq& request, StreamUriResp& response);
    int getVideoEncoderConfigurationOptions(VideoOptionsReq& request, VideoOptionsResp& response);
    int getVideoEncoderConfiguration(VideoConfigReq& request, VideoConfigResp& response);

    int getAudioOutputs( _onvifMedia__GetAudioOutputs& request, _onvifMedia__GetAudioOutputsResponse& response );
    int getVideoSources(_onvifMedia__GetVideoSources& request, _onvifMedia__GetVideoSourcesResponse& response);
/**/int getVideoEncoderConfigurations(VideoConfigsReq& request, VideoConfigsResp& response);
/**/int getVideoSourceConfigurationOptions(VideoSrcOptionsReq& request, VideoSrcOptionsResp& response);

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
};

class Media2SoapWrapper : public SoapWrapper<Media2BindingProxy>
{
public:
    Media2SoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive = false);
    virtual ~Media2SoapWrapper();

    int getVideoEncoderConfigurationOptions(VideoOptionsResp2& response);
    int getVideoEncoderConfigurationOptions(onvifMedia2__GetConfiguration& request, VideoOptionsResp2& response);

};

class PtzSoapWrapper: public SoapWrapper<PTZBindingProxy>
{
public:
    PtzSoapWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint,
        const QString &login,
        const QString &passwd,
        int timeDrift,
        bool tcpKeepAlive = false);
    virtual ~PtzSoapWrapper();

    int doGetConfigurations(_onvifPtz__GetConfigurations& request, _onvifPtz__GetConfigurationsResponse& response);
    int doGetNodes(_onvifPtz__GetNodes& request, _onvifPtz__GetNodesResponse& response);
    int doGetNode(_onvifPtz__GetNode& request, _onvifPtz__GetNodeResponse& response);
    int doGetServiceCapabilities(PtzGetServiceCapabilitiesReq& request, PtzPtzGetServiceCapabilitiesResp& response);
    int doAbsoluteMove(AbsoluteMoveReq& request, AbsoluteMoveResp& response);
    int doRelativeMove(RelativeMoveReq& request, RelativeMoveResp& response);
    int gotoPreset(GotoPresetReq& request, GotoPresetResp& response);
    int setPreset(SetPresetReq& request, SetPresetResp& response);
    int getPresets(GetPresetsReq& request, GetPresetsResp& response);
    int removePreset(RemovePresetReq& request, RemovePresetResp& response);
    int doContinuousMove(_onvifPtz__ContinuousMove& request, _onvifPtz__ContinuousMoveResponse& response);
    int doGetStatus(_onvifPtz__GetStatus& request, _onvifPtz__GetStatusResponse& response);
    int doStop(_onvifPtz__Stop& request, _onvifPtz__StopResponse& response);
};

//typedef QSharedPointer<PtzSoapWrapper> PtzSoapWrapperPtr;

class ImagingSoapWrapper: public SoapWrapper<ImagingBindingProxy>
{
public:
    ImagingSoapWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint,
        const QString& login,
        const QString& passwd,
        int timeDrift,
        bool tcpKeepAlive = false);
    virtual ~ImagingSoapWrapper();

    int getImagingSettings(ImagingSettingsReq& request, ImagingSettingsResp& response);
    int getOptions(ImagingOptionsReq& request, ImagingOptionsResp& response);

    int setImagingSettings(SetImagingSettingsReq& request, SetImagingSettingsResp& response);

    int getMoveOptions(_onvifImg__GetMoveOptions &request, _onvifImg__GetMoveOptionsResponse &response);
    int move(_onvifImg__Move &request, _onvifImg__MoveResponse &response);
};

//typedef QSharedPointer<ImagingSoapWrapper> ImagingSoapWrapperPtr;

class NotificationProducerSoapWrapper: public SoapWrapper<NotificationProducerBindingProxy>
{
public:
    NotificationProducerSoapWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint,
        const QString &login,
        const QString &passwd,
        int timeDrift,
        bool tcpKeepAlive = false);

    int Subscribe(_oasisWsnB2__Subscribe* const request, _oasisWsnB2__SubscribeResponse* const response);
};

class CreatePullPointSoapWrapper: public SoapWrapper<CreatePullPointBindingProxy>
{
public:
    CreatePullPointSoapWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint,
        const QString& login,
        const QString& passwd,
        int timeDrift,
        bool tcpKeepAlive = false);

    int createPullPoint( _oasisWsnB2__CreatePullPoint& request, _oasisWsnB2__CreatePullPointResponse& response );
};

class PullPointSubscriptionWrapper: public SoapWrapper<PullPointSubscriptionBindingProxy>
{
public:
    PullPointSubscriptionWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint, const QString &login, const QString &passwd, int timeDrift, bool tcpKeepAlive = false);

    int pullMessages( _onvifEvents__PullMessages& request, _onvifEvents__PullMessagesResponse& response );
};

class EventSoapWrapper: public SoapWrapper<EventBindingProxy>
{
public:
    EventSoapWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive = false);

    int createPullPointSubscription( _onvifEvents__CreatePullPointSubscription& request, _onvifEvents__CreatePullPointSubscriptionResponse& response );
};

class SubscriptionManagerSoapWrapper: public SoapWrapper<SubscriptionManagerBindingProxy>
{
public:
    SubscriptionManagerSoapWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint,
        const QString &login,
        const QString &passwd,
        int _timeDrift,
        bool tcpKeepAlive = false);

    int renew( _oasisWsnB2__Renew& request, _oasisWsnB2__RenewResponse& response );
    int unsubscribe( _oasisWsnB2__Unsubscribe& request, _oasisWsnB2__UnsubscribeResponse& response );
};

#endif //ENABLE_ONVIF
