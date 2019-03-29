#pragma once

#include <list>
#include <string>
#include <map>
#include <ctime>

#include <QSharedPointer>

#include <utils/common/credentials.h>
#include <utils/camera/camera_diagnostics.h>
#include <plugins/resource/onvif/onvif_helper.h>
#include <plugins/resource/onvif/onvif_namespace_registrar.h>
#include <plugins/resource/onvif/soap_helpers.h>

#include <gsoap/wsseapi.h>
// Soap Binding Proxy classes:
#include <onvif/soapDeviceBindingProxy.h>
#include <onvif/soapDeviceIOBindingProxy.h>
#include <onvif/soapMediaBindingProxy.h>
#include <onvif/soapMedia2BindingProxy.h>
#include <onvif/soapPTZBindingProxy.h>
#include <onvif/soapImagingBindingProxy.h>
#include <onvif/soapNotificationProducerBindingProxy.h>
#include <onvif/soapCreatePullPointBindingProxy.h>
#include <onvif/soapEventBindingProxy.h>
#include <onvif/soapSubscriptionManagerBindingProxy.h>
#include <onvif/soapPullPointSubscriptionBindingProxy.h>

// Instead of including onvif/soapStub.h we use forward declaration of needed classes:
#include "soap_forward.h"

class QnCommonModule;

struct SoapTimeouts
{
    static constexpr int kDefaultSendTimeoutSeconds = 10;
    static constexpr int kDefaultRecvTimeoutSeconds = 10;
    static constexpr int kDefaultConnectTimeoutSeconds = 10;
    static constexpr int kDefaultAcceptTimeoutSeconds = 10;

    static SoapTimeouts minivalValue();

    SoapTimeouts() = default;
    SoapTimeouts(const QString& serialized);
    QString serialize() const;
    void assignTo(struct soap* soap) const;

    std::chrono::seconds sendTimeout = std::chrono::seconds(kDefaultSendTimeoutSeconds);
    std::chrono::seconds recvTimeout = std::chrono::seconds(kDefaultRecvTimeoutSeconds);
    std::chrono::seconds connectTimeout = std::chrono::seconds(kDefaultConnectTimeoutSeconds);
    std::chrono::seconds acceptTimeout = std::chrono::seconds(kDefaultAcceptTimeoutSeconds);
};

struct SoapParams
{
    SoapTimeouts timeouts;
    std::string endpoint;
    QString login;
    QString passwd;
    int timeDrift = 0;
    bool tcpKeepAlive = false;

    SoapParams() = default;
    SoapParams(const SoapTimeouts& timeouts, std::string endpoint, QString login,
        QString passwd, int timeDrift, bool tcpKeepAlive = false)
        :
        timeouts(timeouts), endpoint(std::move(endpoint)), login(std::move(login)),
        passwd(std::move(passwd)), timeDrift(timeDrift), tcpKeepAlive(tcpKeepAlive)
    {
    }

    SoapParams(const SoapTimeouts& timeouts, std::string endpoint,
        const QAuthenticator& auth, int timeDrift, bool tcpKeepAlive = false)
        :
        timeouts(timeouts), endpoint(std::move(endpoint)), login(auth.user()),
        passwd(auth.password()), timeDrift(timeDrift), tcpKeepAlive(tcpKeepAlive)
    {
    }
};

enum class OnvifWebService { Media, Media2, Ptz, Imaging, DeviceIO };

/**
 * Base class for SoapWrapper.
 * Contains members that are independent of BindingProxy type
 */
class BaseSoapWrapper
{
public:
    BaseSoapWrapper(
        std::string endpoint,
        QString login,
        QString passwd,
        int timeDrift)
        :
        m_endpointHolder(std::move(endpoint)),
        m_endpoint(m_endpointHolder.c_str()),
        m_login(std::move(login)),
        m_passwd(std::move(passwd)),
        m_timeDrift(timeDrift)
    {
    }

    //BaseSoapWrapper(SoapParams soapParams) :
    //    m_endpointHolder(std::move(soapParams.endpoint)),
    //    m_endpoint(m_endpointHolder.c_str()),
    //    m_timeDrift(soapParams.timeDrift),
    //    m_login(std::move(soapParams.login)),
    //    m_passwd(std::move(soapParams.passwd))
    //{
    //}

    virtual ~BaseSoapWrapper() = default;

    const char* endpoint() const { return m_endpoint; }
    QString login() const { return m_login; }
    QString password() const { return m_passwd; }
    int timeDrift() const { return m_timeDrift; }
    void setLogin(const QString& login) { m_login = login; }
    void setPassword(const QString& password) { m_passwd = password; }
    bool invoked() const { return m_invoked; }
    operator bool() const { return !m_endpointHolder.empty(); }

    virtual QString getLastErrorDescription() = 0;
    virtual bool lastErrorIsNotAuthenticated() = 0;
    virtual bool lastErrorIsConflict() = 0;

protected:
    const std::string m_endpointHolder;
    const char* const m_endpoint; //< points to m_endpointHolder data
    QString m_login;
    QString m_passwd;
    int m_timeDrift;
    bool m_invoked = false;
};

template <class BindingProxyT>
class SoapWrapper: public BaseSoapWrapper
{
public:
    using BindingProxy = BindingProxyT;
    SoapWrapper(
        const SoapTimeouts& timeouts,
        std::string endpoint,
        QString login,
        QString passwd,
        int timeDrift,
        bool tcpKeepAlive);

    SoapWrapper(SoapParams soapParams);

    virtual ~SoapWrapper() override;

    SoapWrapper(const SoapWrapper&) = delete;
    SoapWrapper(SoapWrapper&&) = delete;
    SoapWrapper& operator=(const SoapWrapper&) = delete;
    SoapWrapper& operator=(SoapWrapper&&) = delete;

    BindingProxy& bindingProxy() { return m_bindingProxy; }
    const BindingProxy& bindingProxy() const { return m_bindingProxy; }

    struct soap* soap() { return m_bindingProxy.soap; }
    const struct soap* soap() const { return m_bindingProxy.soap; }

    virtual QString getLastErrorDescription() override
    {
        return SoapErrorHelper::fetchDescription(m_bindingProxy.soap_fault());
    }
    virtual bool lastErrorIsNotAuthenticated() override
    {
        return PasswordHelper::isNotAuthenticated(m_bindingProxy.soap_fault());
    }
    virtual bool lastErrorIsConflict() override
    {
        return PasswordHelper::isConflict(m_bindingProxy.soap_fault());
    }

    // Invokes method \a methodToInvoke, which is member of \a BindingProxy,
    // with pre-supplied endpoint, username and password
    template<class RequestType, class ResponseType>
    int invokeMethod(
        int (BindingProxy::*methodToInvoke)(const char*, const char*, RequestType*, ResponseType&),
        RequestType* const request,
        ResponseType& response )
    {
        beforeMethodInvocation<RequestType>();
        return (m_bindingProxy.*methodToInvoke)(m_endpoint, NULL, request, response);
    }

    template<typename Request>
    void beforeMethodInvocation()
    {
        //using namespace nx::vms::server::plugins;
        if (m_invoked)
        {
            soap_destroy(m_bindingProxy.soap);
            soap_end(m_bindingProxy.soap);
        }
        else
        {
            m_invoked = true;
        }

        const auto namespaces = nx::vms::server::plugins::onvif::requestNamespaces<Request>();
        //########################################################
        if (namespaces != nullptr)
            soap_set_namespaces(m_bindingProxy.soap, namespaces);

        if (!m_login.isEmpty())
        {
            nx::vms::server::plugins::onvif::soapWsseAddUsernameTokenDigest(
                m_bindingProxy.soap,
                NULL,
                m_login.toUtf8().constData(),
                m_passwd.toUtf8().constData(),
                time(NULL) + m_timeDrift);
        }
    }

protected:

    BindingProxy m_bindingProxy;
};

template <class BindingProxyT>
SoapWrapper<BindingProxyT>::SoapWrapper(
    const SoapTimeouts& timeouts,
    std::string endpoint,
    QString login,
    QString passwd,
    int timeDrift,
    bool tcpKeepAlive)
    :
    BaseSoapWrapper(std::move(endpoint), std::move(login), std::move(passwd), timeDrift)
{
    NX_ASSERT(!m_endpointHolder.empty());
    if (tcpKeepAlive)
    {
        soap_imode(m_bindingProxy.soap, SOAP_IO_KEEPALIVE);
        soap_omode(m_bindingProxy.soap, SOAP_IO_KEEPALIVE);

    }

    timeouts.assignTo(m_bindingProxy.soap);
    soap_register_plugin(m_bindingProxy.soap, soap_wsse);
}

template <class BindingProxyT>
SoapWrapper<BindingProxyT>::SoapWrapper(SoapParams soapParams):
    BaseSoapWrapper(
        std::move(soapParams.endpoint),
        std::move(soapParams.login),
        std::move(soapParams.passwd),
        std::move(soapParams.timeDrift))

{
    if (soapParams.tcpKeepAlive)
    {
        soap_imode(m_bindingProxy.soap, SOAP_IO_KEEPALIVE);
        soap_omode(m_bindingProxy.soap, SOAP_IO_KEEPALIVE);

    }

    soapParams.timeouts.assignTo(m_bindingProxy.soap);
    soap_register_plugin(m_bindingProxy.soap, soap_wsse);
}

template <class BindingProxyT>
SoapWrapper<BindingProxyT>::~SoapWrapper()
{
    if (m_invoked)
    {
        soap_destroy(m_bindingProxy.soap);
        soap_end(m_bindingProxy.soap);
    }
}

template<class RequestT, class ResponseT>
class RequestTraits;

/**
Generate code of the traits class
The example of code generated:
template<>
class RequestTraits<_onvifDeviceIO__GetDigitalInputs, _onvifDeviceIO__GetDigitalInputsResponse>
{
public:
    using BindingProxy = DeviceIOBindingProxy;
    using Request = _onvifDeviceIO__GetDigitalInputs;
    using Response = _onvifDeviceIO__GetDigitalInputsResponse;
    static const OnvifWebService kOnvifWebService = OnvifWebService::DeviceIO;
    using RequestFunc =
        int (DeviceIOBindingProxy::*)(
            const char* soap_endpoint,
            const char* soap_action,
            _onvifDeviceIO__GetDigitalInputs* request,
            _onvifDeviceIO__GetDigitalInputsResponse& response);
    static const RequestFunc kRequestFunc; //< = &DeviceIOBindingProxy::GetDigitalInputs;
    static const char kFuncName[64];
};
*/

#define NX_MAKE_BINDINGPROXY_LEXEME(WEBSERVICE) WEBSERVICE##BindingProxy
#define NX_MAKE_REQUEST_LEXEME(WEBSERVICE, FUNCTION) _onvif##WEBSERVICE##__##FUNCTION
#define NX_MAKE_RESPONSE_LEXEME(WEBSERVICE, FUNCTION) _onvif##WEBSERVICE##__##FUNCTION##Response

#define NX_DECLARE_RESPONSE_TRAITS(WEBSERVICE, FUNCTION) \
template<> \
class RequestTraits<NX_MAKE_REQUEST_LEXEME(WEBSERVICE, FUNCTION), NX_MAKE_RESPONSE_LEXEME(WEBSERVICE, FUNCTION)> \
{ \
public: \
    using BindingProxy = NX_MAKE_BINDINGPROXY_LEXEME(WEBSERVICE); \
    using Request = NX_MAKE_REQUEST_LEXEME(WEBSERVICE, FUNCTION); \
    using Response = NX_MAKE_RESPONSE_LEXEME(WEBSERVICE, FUNCTION); \
    static const OnvifWebService kOnvifWebService = OnvifWebService::WEBSERVICE; \
    using RequestFunc = \
        int (BindingProxy::*)( \
            const char* soap_endpoint, \
            const char* soap_action, \
            Request* request, \
            Response& response); \
    static const RequestFunc kRequestFunc;  \
    static const char kFuncName[64]; \
};

#define NX_DECLARE_RESPONSE_TRAITS_IRREGULAR(WEBSERVICE, REQUEST, RESPONSE) \
template<> \
class RequestTraits<REQUEST, RESPONSE> \
{ \
public: \
    using BindingProxy = NX_MAKE_BINDINGPROXY_LEXEME(WEBSERVICE); \
    using Request = REQUEST; \
    using Response = RESPONSE; \
    static const OnvifWebService kOnvifWebService = OnvifWebService::WEBSERVICE; \
    using RequestFunc = \
        int (BindingProxy::*)( \
            const char* soap_endpoint, \
            const char* soap_action, \
            Request* request, \
            Response& response); \
    static const RequestFunc kRequestFunc;  \
    static const char kFuncName[64]; \
};

NX_DECLARE_RESPONSE_TRAITS(DeviceIO, GetDigitalInputs)
NX_DECLARE_RESPONSE_TRAITS_IRREGULAR(DeviceIO,
    _onvifDevice__GetRelayOutputs, _onvifDevice__GetRelayOutputsResponse)
NX_DECLARE_RESPONSE_TRAITS(DeviceIO, SetRelayOutputSettings)

NX_DECLARE_RESPONSE_TRAITS(Media, GetVideoEncoderConfiguration)
NX_DECLARE_RESPONSE_TRAITS(Media, GetVideoEncoderConfigurations)
NX_DECLARE_RESPONSE_TRAITS(Media, GetVideoEncoderConfigurationOptions)
NX_DECLARE_RESPONSE_TRAITS(Media, SetVideoEncoderConfiguration)
NX_DECLARE_RESPONSE_TRAITS(Media, GetAudioEncoderConfigurations)
NX_DECLARE_RESPONSE_TRAITS(Media, SetAudioEncoderConfiguration)
NX_DECLARE_RESPONSE_TRAITS(Media, GetProfiles)
NX_DECLARE_RESPONSE_TRAITS(Media, CreateProfile)

NX_DECLARE_RESPONSE_TRAITS_IRREGULAR(Media2,
    onvifMedia2__GetConfiguration, _onvifMedia2__GetVideoEncoderConfigurationsResponse)
NX_DECLARE_RESPONSE_TRAITS_IRREGULAR(Media2,
    onvifMedia2__GetConfiguration, _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse)
NX_DECLARE_RESPONSE_TRAITS_IRREGULAR(Media2,
    _onvifMedia2__SetVideoEncoderConfiguration, onvifMedia2__SetConfigurationResponse)
NX_DECLARE_RESPONSE_TRAITS(Media2, GetProfiles)
NX_DECLARE_RESPONSE_TRAITS(Media2, CreateProfile)

///////////////////////////////////////////////////////////////////////////////////////////////////
template<class RequestT, class ResponseT>
class RequestWrapper;

template<class RequestT, class ResponseT>
class ResponseHolder
{
public:
    using Request = RequestT;
    using Response = ResponseT;
    ResponseHolder(RequestWrapper<Request, Response>* owner):
        m_responseOwner(owner)
    {
        ++m_responseOwner->responseHolderCount;
    }

    ResponseHolder(const ResponseHolder&) = delete;
    ResponseHolder& operator=(const ResponseHolder&) = delete;

    ResponseHolder(ResponseHolder&& other): m_responseOwner(other.m_responseOwner)
    {
        other.m_responseOwner = nullptr;
    }
    ResponseHolder& operator=(ResponseHolder&& other)
    {
        m_responseOwner = other.m_responseOwner;
        other.m_responseOwner = nullptr;
        return *this;
    }

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
    Response* operator->()
    {
        NX_CRITICAL(m_responseOwner);
        return &m_responseOwner->m_response;
    }
    const Response* operator->() const
    {
        NX_CRITICAL(m_responseOwner);
        return &m_responseOwner->m_response;
    }
    // Temporary forbidden in order to suppress error-prone code.
    //const Response& operator*() const
    //{
    //    NX_CRITICAL(m_responseOwner);
    //    return m_responseOwner->m_response;
    //}
private:
    RequestWrapper<Request, Response>* m_responseOwner;
};

template<class RequestT, class ResponseT>
class RequestWrapper
{
public:
    using Request = RequestT;
    using Response = ResponseT;
    friend class ResponseHolder<Request, Response>;
    using BindingProxy = typename RequestTraits<Request, Response>::BindingProxy;
    static const OnvifWebService kOnvifWebService = RequestTraits<Request, Response>::kOnvifWebService;

    explicit RequestWrapper(SoapParams params):
        m_wrapper(params)
        //m_wrapper(params.timeouts, std::move(params.endpoint), std::move(params.login),
        //std::move(params.passwd), params.timeDrift, params.tcpKeepAlive)
    {
        m_response.soap_default(m_response.soap);
    }

    // Resource should be QnPlOnvifResource class, template is used to eliminate dependency
    // between soap_wrapper and onvif_resource. Usually "this" will be passed as a parameter.
    template<class Resource>
    explicit RequestWrapper(const Resource& resource):
        RequestWrapper(resource->makeSoapParams(kOnvifWebService))
    {
    }

    RequestWrapper(const RequestWrapper&) = delete;
    RequestWrapper(RequestWrapper&&) = delete;
    RequestWrapper& operator=(const RequestWrapper&) = delete;
    RequestWrapper& operator=(RequestWrapper&&) = delete;

    ~RequestWrapper()
    {
        NX_CRITICAL(responseHolderCount == 0);
        if (m_wrapper.invoked())
        {
            soap_destroy(m_wrapper.soap());
            soap_end(m_wrapper.soap());
        }
    }

    bool receiveBySoap()
    {
        Request emptyRequest;
        return receiveBySoap(emptyRequest);
    }

    bool receiveBySoap(Request& request) //< gsoap does not guarantee immutability of "request"
    {
        NX_CRITICAL(responseHolderCount == 0);

        m_wrapper.template beforeMethodInvocation<Request>();

        std::invoke(RequestTraits<Request, Response>::kRequestFunc,
            m_wrapper.bindingProxy(), m_wrapper.endpoint(), nullptr, &request, m_response);

        return soapError() == SOAP_OK;
    }

    bool performRequest(Request& request) //< Just another name.
    {
        return receiveBySoap(request);
    }

    void reset()
    {
        if (m_wrapper.m_invoked)
        {
            soap_destroy(m_wrapper.bindingProxy()->soap);
            soap_end(m_wrapper.bindingProxy()->soap);

            m_response.soap_default(m_response.soap);

            m_wrapper.m_invoked = false;
        }

    }

    ResponseHolder<Request, Response> get()
    {
        return ResponseHolder<Request, Response>(this);
    }

    const char* requestFunctionName() const
    {
        return RequestTraits<Request, Response>::kFuncName;
    }

    int soapError() const noexcept { return m_wrapper.soap()->error; }

    QString soapErrorAsString() const
    {
        return SoapErrorHelper::fetchDescription(m_wrapper.soap()->fault);
    }
    CameraDiagnostics::RequestFailedResult requestFailedResult() const
    {
        return CameraDiagnostics::RequestFailedResult(
            QString(requestFunctionName()), soapErrorAsString());
    }

    operator bool() const noexcept { return soapError() == SOAP_OK; }
    SoapWrapper<BindingProxy>& innerWrapper() noexcept { return m_wrapper; }
    QString endpoint() const { return QString::fromLatin1(m_wrapper.endpoint()); }

protected:
    SoapWrapper<BindingProxy> m_wrapper;
    Response m_response;
    int responseHolderCount = 0;
};
// ------------------------------------------------------------------------------------------------
namespace DeviceIO
{
    using DigitalInputs = RequestWrapper<
        _onvifDeviceIO__GetDigitalInputs,
        _onvifDeviceIO__GetDigitalInputsResponse>;

    /**
     * This request fails on DWC-MV44WiA (192.168.1.81),
     * the response contains empty vector of outputs (with "200 OK" return code).
     * Normally all requests are sent to the endpoint, corresponding the Web Service name,
     * in this case - QnOnvifServiceUrls::deviceioServiceUrl. But this request works only
     * if endpoint is QnOnvifServiceUrls::deviceServiceUrl.

     * Instead of DeviceIO::GetRelayOutputs we use obsolete function Device::GetRelayOutputs.
     */
    using RelayOutputs = RequestWrapper<
        _onvifDevice__GetRelayOutputs,
        _onvifDevice__GetRelayOutputsResponse>;

    using RelayOutputSettingsSettingResult = RequestWrapper<
        _onvifDeviceIO__SetRelayOutputSettings,
        _onvifDeviceIO__SetRelayOutputSettingsResponse>;
}

namespace Media
{
    using VideoEncoderConfiguration = RequestWrapper<
        _onvifMedia__GetVideoEncoderConfiguration,
        _onvifMedia__GetVideoEncoderConfigurationResponse>;

    using VideoEncoderConfigurations = RequestWrapper<
        _onvifMedia__GetVideoEncoderConfigurations,
        _onvifMedia__GetVideoEncoderConfigurationsResponse>;

    using VideoEncoderConfigurationSetter = RequestWrapper<
        _onvifMedia__SetVideoEncoderConfiguration,
        _onvifMedia__SetVideoEncoderConfigurationResponse>;

    using VideoEncoderConfigurationOptions = RequestWrapper<
        _onvifMedia__GetVideoEncoderConfigurationOptions,
        _onvifMedia__GetVideoEncoderConfigurationOptionsResponse>;

    using AudioEncoderConfigurations = RequestWrapper<
        _onvifMedia__GetAudioEncoderConfigurations,
        _onvifMedia__GetAudioEncoderConfigurationsResponse>;

    using AudioEncoderConfigurationSetter = RequestWrapper<
        _onvifMedia__SetAudioEncoderConfiguration,
        _onvifMedia__SetAudioEncoderConfigurationResponse>;

    using Profiles = RequestWrapper<
        _onvifMedia__GetProfiles,
        _onvifMedia__GetProfilesResponse>;

    using ProfileCreator = RequestWrapper<
        _onvifMedia__CreateProfile,
        _onvifMedia__CreateProfileResponse>;
}

namespace Media2
{
    using VideoEncoderConfigurations = RequestWrapper<
        onvifMedia2__GetConfiguration,
        _onvifMedia2__GetVideoEncoderConfigurationsResponse>;

    using VideoEncoderConfigurationSetter = RequestWrapper<
        _onvifMedia2__SetVideoEncoderConfiguration,
        onvifMedia2__SetConfigurationResponse>;

    using VideoEncoderConfigurationOptions =
        RequestWrapper<onvifMedia2__GetConfiguration,
        _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse>;

    using Profiles = RequestWrapper<
        _onvifMedia2__GetProfiles,
        _onvifMedia2__GetProfilesResponse>;

    using ProfileCreator = RequestWrapper<
        _onvifMedia2__CreateProfile,
        _onvifMedia2__CreateProfileResponse>;
}
// ------------------------------------------------------------------------------------------------
class DeviceSoapWrapper : public SoapWrapper<DeviceBindingProxy>
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

    DeviceSoapWrapper(SoapParams soapParams);

    virtual ~DeviceSoapWrapper();

    //Input: normalized manufacturer
    bool fetchLoginPassword(QnCommonModule* commonModule, const QString& manufacturer, const QString& model);

    int getServiceCapabilities(_onvifDevice__GetServiceCapabilities& request, _onvifDevice__GetServiceCapabilitiesResponse& response);
    int getRelayOutputs(_onvifDevice__GetRelayOutputs& request, _onvifDevice__GetRelayOutputsResponse& response);
    int setRelayOutputState(_onvifDevice__SetRelayOutputState& request, _onvifDevice__SetRelayOutputStateResponse& response);
    int setRelayOutputSettings(_onvifDevice__SetRelayOutputSettings& request, _onvifDevice__SetRelayOutputSettingsResponse& response);

    int getCapabilities(_onvifDevice__GetCapabilities& request, _onvifDevice__GetCapabilitiesResponse& response);
    int getServices(GetServicesReq& request, GetServicesResp& response);
    int getDeviceInformation(DeviceInfoReq& request, DeviceInfoResp& response);
    int getNetworkInterfaces(NetIfacesReq& request, NetIfacesResp& response);
    int GetSystemDateAndTime(_onvifDevice__GetSystemDateAndTime& request, _onvifDevice__GetSystemDateAndTimeResponse& response);

    int createUsers(CreateUsersReq& request, CreateUsersResp& response);

    int setSystemFactoryDefaultHard(FactoryDefaultReq& request, FactoryDefaultResp& response);
    int setSystemFactoryDefaultSoft(FactoryDefaultReq& request, FactoryDefaultResp& response);
    int systemReboot(RebootReq& request, RebootResp& response);

private:
    DeviceSoapWrapper();
    DeviceSoapWrapper(const DeviceSoapWrapper&);
    QAuthenticator getDefaultPassword(
        QnCommonModule* commonModule,
        const QString& manufacturer, const QString& model) const;
    std::list<nx::vms::common::Credentials> getPossibleCredentials(
        QnCommonModule* commonModule,
        const QString& manufacturer, const QString& model) const;
    nx::vms::common::Credentials getForcedCredentials(
        QnCommonModule* commonModule,
        const QString& manufacturer, const QString& model);
    void calcTimeDrift();

};
// ------------------------------------------------------------------------------------------------
class MediaSoapWrapper: public SoapWrapper<MediaBindingProxy>
{
public:
    MediaSoapWrapper(SoapParams soapParams):
        SoapWrapper<MediaBindingProxy>(std::move(soapParams))
    {
    }

    template<class Resource>
    explicit MediaSoapWrapper(const Resource& resource, bool tcpKeepAlive = false):
        SoapWrapper<MediaBindingProxy>(resource->makeSoapParams(OnvifWebService::Media, tcpKeepAlive))
    {
    }

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
// ------------------------------------------------------------------------------------------------
class PtzSoapWrapper: public SoapWrapper<PTZBindingProxy>
{
public:
    PtzSoapWrapper(SoapParams soapParams):
        SoapWrapper<PTZBindingProxy>(std::move(soapParams))
    {
    }

    template<class Resource>
    explicit PtzSoapWrapper(const Resource& resource, bool tcpKeepAlive = false):
        SoapWrapper<PTZBindingProxy>(resource->makeSoapParams(OnvifWebService::Ptz, tcpKeepAlive))
    {
    }

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
// ------------------------------------------------------------------------------------------------
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
    ImagingSoapWrapper(SoapParams soapParams);

    template<class Resource>
    explicit ImagingSoapWrapper(const Resource& resource):
        ImagingSoapWrapper(resource->makeSoapParams())
    {
    }
    virtual ~ImagingSoapWrapper();

    int getImagingSettings(ImagingSettingsReq& request, ImagingSettingsResp& response);
    int getOptions(ImagingOptionsReq& request, ImagingOptionsResp& response);

    int setImagingSettings(SetImagingSettingsReq& request, SetImagingSettingsResp& response);

    int getMoveOptions(_onvifImg__GetMoveOptions &request, _onvifImg__GetMoveOptionsResponse &response);
    int move(_onvifImg__Move &request, _onvifImg__MoveResponse &response);
};
// ------------------------------------------------------------------------------------------------
// Onvif event notification - base notification
// http://docs.oasis-open.org/wsn/wsn-ws_base_notification-1.3-spec-os.pdf
// NotificationProducerBindingProxy, SubscriptionManagerBindingProxy.
// ------------------------------------------------------------------------------------------------
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
    NotificationProducerSoapWrapper(SoapParams soapParams);

    template<class Resource>
    explicit NotificationProducerSoapWrapper(const Resource& resource) :
        NotificationProducerSoapWrapper(resource->makeSoapParams())
    {
    }

    int subscribe(_oasisWsnB2__Subscribe* const request, _oasisWsnB2__SubscribeResponse* const response);

    // NotificationProducerBindingProxy also implements GetCurrentMessage,
    // but it is not currently used.
};
// ------------------------------------------------------------------------------------------------
class SubscriptionManagerSoapWrapper : public SoapWrapper<SubscriptionManagerBindingProxy>
{
public:
    SubscriptionManagerSoapWrapper(SoapParams soapParams) :
        SoapWrapper<SubscriptionManagerBindingProxy>(std::move(soapParams))
    {
    }

    template<class Resource>
    explicit SubscriptionManagerSoapWrapper(const Resource& resource,
        std::string endpoint, bool tcpKeepAlive = false)
        :
        SoapWrapper<SubscriptionManagerBindingProxy>(
            resource->makeSoapParams(std::move(endpoint), tcpKeepAlive))
    {
    }

    int renew(_oasisWsnB2__Renew& request, _oasisWsnB2__RenewResponse& response);
    int unsubscribe(_oasisWsnB2__Unsubscribe& request, _oasisWsnB2__UnsubscribeResponse& response);
};
// ------------------------------------------------------------------------------------------------
#if 0
/** CreatePullPoint is used to create new PullPoint resource.
 *  Specification for details:
 *  http://docs.oasis-open.org/wsn/wsn-ws_base_notification-1.3-spec-os.pdf
 *  Chapter 5. Pull-Style Notification. Section 5.2. Create PullPoint Interface.
 *
 *  CreatePullPoint interface currently is not used.
 */
class CreatePullPointSoapWrapper: public SoapWrapper<CreatePullPointBindingProxy>
{
public:
    CreatePullPointSoapWrapper(SoapParams soapParams):
        SoapWrapper<CreatePullPointBindingProxy>(std::move(soapParams))
    {
    }

    template<class Resource>
    explicit CreatePullPointSoapWrapper(const Resource& resource):
        CreatePullPointSoapWrapper(resource->makeSoapParams())
    {
    }

    int createPullPoint(
        _oasisWsnB2__CreatePullPoint& request, _oasisWsnB2__CreatePullPointResponse& response)
    {
        return invokeMethod(&CreatePullPointBindingProxy::CreatePullPoint, &request, response);
    }
};
#endif
// ------------------------------------------------------------------------------------------------
// Onvif event notification - real-time Pull-Point
// https://www.onvif.org/ver10/events/wsdl/event.wsdl:
// EventBindingProxy, PullPointSubscriptionBindingProxy.
// ------------------------------------------------------------------------------------------------
class EventSoapWrapper : public SoapWrapper<EventBindingProxy>
{
public:
    EventSoapWrapper(SoapParams soapParams) :
        SoapWrapper<EventBindingProxy>(std::move(soapParams))
    {
    }

    template<class Resource>
    explicit EventSoapWrapper(const Resource& resource,
        std::string endpoint, bool tcpKeepAlive = false)
        :
        SoapWrapper<EventBindingProxy>(
            resource->makeSoapParams(std::move(endpoint), tcpKeepAlive))
    {
    }

    int createPullPointSubscription(_onvifEvents__CreatePullPointSubscription& request,
        _onvifEvents__CreatePullPointSubscriptionResponse& response);

    // EventBindingProxy also implements GetEventProperties and GetServiceCapabilities,
    // but they are not currently used.
};
// ------------------------------------------------------------------------------------------------
class PullPointSubscriptionWrapper: public SoapWrapper<PullPointSubscriptionBindingProxy>
{
public:
    PullPointSubscriptionWrapper(
        const SoapTimeouts& timeouts,
        const std::string& endpoint, const QString &login, const QString &passwd, int timeDrift, bool tcpKeepAlive = false);
    PullPointSubscriptionWrapper(SoapParams soapParams);

    template<class Resource>
    explicit PullPointSubscriptionWrapper(const Resource& resource):
        PullPointSubscriptionWrapper(resource->makeSoapParams())
    {
    }

    int pullMessages(
        _onvifEvents__PullMessages& request, _onvifEvents__PullMessagesResponse& response);

    // PullPointSubscriptionBindingProxy also implements
    // Seek, SetSynchronizationPoint, Unsubscribe, but they are not currently used.
};
// ------------------------------------------------------------------------------------------------
