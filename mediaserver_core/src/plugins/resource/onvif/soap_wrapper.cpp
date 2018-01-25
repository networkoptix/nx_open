#ifdef ENABLE_ONVIF

#include <set>

#include "openssl/evp.h"

#include <QElapsedTimer>

#include "soap_wrapper.h"
#include <onvif/Onvif.nsmap>
#include <onvif/soapDeviceBindingProxy.h>
#include <onvif/soapDeviceIOBindingProxy.h>
#include <onvif/soapMediaBindingProxy.h>
#include <onvif/soapPTZBindingProxy.h>
#include <onvif/soapImagingBindingProxy.h>
#include <onvif/soapNotificationProducerBindingProxy.h>
#include <onvif/soapCreatePullPointBindingProxy.h>
#include <onvif/soapEventBindingProxy.h>
#include <onvif/soapSubscriptionManagerBindingProxy.h>
#include <onvif/soapPullPointSubscriptionBindingProxy.h>
#include <gsoap/wsseapi.h>
#include <nx/utils/log/log.h>
#include <QtCore/QtGlobal>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include "core/resource/resource.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"
#include "media_server/settings.h"
#include <common/static_common_module.h>
#include <media_server/media_server_module.h>

using nx::common::utils::Credentials;

namespace {

/** Size of the random nonce */
#define SOAP_WSSE_NONCELEN (20)

/**
@fn static void calc_nonce(struct soap *soap, char nonce[SOAP_WSSE_NONCELEN])
@brief Calculates randomized nonce (also uses time() in case a poorly seeded PRNG is used)
@param soap context
@param[out] nonce value
*/
static void
calc_nonce(struct soap *soap, char nonce[SOAP_WSSE_NONCELEN])
{
    Q_UNUSED(soap)
    int i;
    time_t r = time(NULL);
    memcpy(nonce, &r, 4);
    for (i = 4; i < SOAP_WSSE_NONCELEN; i += 4){
        r = soap_random;
        memcpy(nonce + i, &r, 4);
    }
}

/**
@fn static void calc_digest(struct soap *soap, const char *created, const char *nonce, int noncelen, const char *password, char hash[SOAP_SMD_SHA1_SIZE])
@brief Calculates digest value SHA1(created, nonce, password)
@param soap context
@param[in] created string (XSD dateTime format)
@param[in] nonce value
@param[in] noncelen length of nonce value
@param[in] password string
@param[out] hash SHA1 digest
*/
static void
    calc_digest(struct soap *soap, const char *created, const char *nonce, int noncelen, const char *password, char hash[SOAP_SMD_SHA1_SIZE])
{ struct soap_smd_data context;
/* use smdevp engine */
soap_smd_init(soap, &context, SOAP_SMD_DGST_SHA1, NULL, 0);
soap_smd_update(soap, &context, nonce, noncelen);
soap_smd_update(soap, &context, created, strlen(created));
soap_smd_update(soap, &context, password, strlen(password));
soap_smd_final(soap, &context, hash, NULL);
}


/**
@fn int soap_wsse_add_UsernameTokenDigest(struct soap *soap, const char *id, const char *username, const char *password)
@brief Adds UsernameToken element for digest authentication.
@param soap context
@param[in] id string for signature referencing or NULL
@param[in] username string
@param[in] password string
@return SOAP_OK

Computes SHA1 digest of the time stamp, a nonce, and the password. The digest
provides the authentication credentials. Passwords are NOT sent in the clear.
Note: this release supports the use of at most one UsernameToken in the header.
*/
int
soap_wsse_add_UsernameTokenDigest(struct soap *soap, const char *id, const char *username, const char *password, time_t now)
{ _wsse__Security *security = soap_wsse_add_Security(soap);
  const char *created = soap_dateTime2s(soap, now);
  char HA[SOAP_SMD_SHA1_SIZE], HABase64[29];
  char nonce[SOAP_WSSE_NONCELEN], *nonceBase64;
  DBGFUN2("soap_wsse_add_UsernameTokenDigest", "id=%s", id?id:"", "username=%s", username?username:"");
  /* generate a nonce */
  calc_nonce(soap, nonce);
  nonceBase64 = soap_s2base64(soap, (unsigned char*)nonce, NULL, SOAP_WSSE_NONCELEN);
  /* The specs are not clear: compute digest over binary nonce or base64 nonce? */
  /* compute SHA1(created, nonce, password) */
  calc_digest(soap, created, nonce, SOAP_WSSE_NONCELEN, password, HA);
  /*
  calc_digest(soap, created, nonceBase64, strlen(nonceBase64), password, HA);
  */
  soap_s2base64(soap, (unsigned char*)HA, HABase64, SOAP_SMD_SHA1_SIZE);
  /* populate the UsernameToken with digest */
  soap_wsse_add_UsernameTokenText(soap, id, username, HABase64);
  /* populate the remainder of the password, nonce, and created */
  security->UsernameToken->Password->Type = (char*)wsse_PasswordDigestURI;
  security->UsernameToken->Nonce = nonceBase64;
  security->UsernameToken->wsu__Created = soap_strdup(soap, created);
  return SOAP_OK;
}

const int kSoapDefaultSendTimeoutSeconds = 10;
const int kSoapDefaultRecvTimeoutSeconds = 10;
const int kSoapDefaultConnectTimeoutSeconds = 5;
const int kSoapDefaultAcceptTimeoutSeconds = 5;


struct SoapTimeouts
{
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

SoapTimeouts getSoapTimeouts()
{
    auto serializedTimeouts = qnServerModule->roSettings()
        ->value( nx_ms_conf::ONVIF_TIMEOUTS, QString()).toString();

    return SoapTimeouts(serializedTimeouts);
}

/*
int soap_wsse_add_PlainTextAuth(struct soap *soap, const char *id, const char *username, const char *password, time_t now)
{
    _wsse__Security *security = soap_wsse_add_Security(soap);
    soap_wsse_add_UsernameTokenText(soap, id, username, password);
    security->UsernameToken->Password->Type = (char*) wsse_PasswordTextURI;
    return SOAP_OK;
}
*/

} // anonymous namespace


const QLatin1String DEFAULT_ONVIF_LOGIN = QLatin1String("admin");
const QLatin1String DEFAULT_ONVIF_PASSWORD = QLatin1String("admin");


SOAP_NMAC struct Namespace onvifOverriddenNamespaces[] =
{
    { "SOAP-ENV", "http://www.w3.org/2003/05/soap-envelope", "http://schemas.xmlsoap.org/soap/envelope/", NULL },
    { "SOAP-ENC", "http://www.w3.org/2003/05/soap-encoding", "http://schemas.xmlsoap.org/soap/encoding/", NULL },
    { "xsi", "http://www.w3.org/2001/XMLSchema-instance", "http://www.w3.org/*/XMLSchema-instance", NULL },
    { "xsd", "http://www.w3.org/2001/XMLSchema", "http://www.w3.org/*/XMLSchema", NULL },
    { "c14n", "http://www.w3.org/2001/10/xml-exc-c14n#", NULL, NULL },
    { "wsu", "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd", NULL, NULL },
    { "xenc", "http://www.w3.org/2001/04/xmlenc#", NULL, NULL },
    { "ds", "http://www.w3.org/2000/09/xmldsig#", NULL, NULL },
    { "wsse", "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd", "http://docs.oasis-open.org/wss/oasis-wss-wssecurity-secext-1.1.xsd", NULL },
    { "wsa", "http://www.w3.org/2005/08/addressing", NULL, NULL },
    { "wsdd", "http://schemas.xmlsoap.org/ws/2005/04/discovery", NULL, NULL },
    { "ns1", "http://www.w3.org/2005/08/addressing", NULL, NULL },
    { "oasisWsrf", "http://docs.oasis-open.org/wsrf/bf-2", NULL, NULL },
    { "xmime", "http://tempuri.org/xmime.xsd", NULL, NULL },
    { "ns5", "http://www.w3.org/2004/08/xop/include", NULL, NULL },
    { "onvifXsd", "http://www.onvif.org/ver10/schema", NULL, NULL },
    { "oasisWsnT1", "http://docs.oasis-open.org/wsn/t-1", NULL, NULL },
    { "oasisWsrfR2", "http://docs.oasis-open.org/wsrf/r-2", NULL, NULL },
    { "ns2", "http://www.onvif.org/ver10/events/wsdl/PausableSubscriptionManagerBinding", NULL, NULL },
    { "onvifDevice", "http://www.onvif.org/ver10/device/wsdl", NULL, NULL },
    { "onvifDeviceIO", "http://www.onvif.org/ver10/deviceIO/wsdl", NULL, NULL },
    { "onvifImg", "http://www.onvif.org/ver20/imaging/wsdl", NULL, NULL },
    { "onvifMedia", "http://www.onvif.org/ver10/media/wsdl", NULL, NULL },
    { "onvifPtz", "http://www.onvif.org/ver20/ptz/wsdl", NULL, NULL },
    { "tev-cpb", "http://www.onvif.org/ver10/events/wsdl/CreatePullPointBinding", NULL, NULL },
    { "tev-eb", "http://www.onvif.org/ver10/events/wsdl/EventBinding", NULL, NULL },
    { "tev-ncb", "http://www.onvif.org/ver10/events/wsdl/NotificationConsumerBinding", NULL, NULL },
    { "tev-npb", "http://www.onvif.org/ver10/events/wsdl/NotificationProducerBinding", NULL, NULL },
    { "oasisWsnB2", "http://docs.oasis-open.org/wsn/b-2", NULL, NULL },
    { "tev-ppb", "http://www.onvif.org/ver10/events/wsdl/PullPointBinding", NULL, NULL },
    { "tev-pps", "http://www.onvif.org/ver10/events/wsdl/PullPointSubscriptionBinding", NULL, NULL },
    { "onvifEvents", "http://www.onvif.org/ver10/events/wsdl", NULL, NULL },
    { "tev-smb", "http://www.onvif.org/ver10/events/wsdl/SubscriptionManagerBinding", NULL, NULL },
    { NULL, NULL, NULL, NULL }
};


// -------------------------------------------------------------------------- //
// SoapWrapper
// -------------------------------------------------------------------------- //
template <class T>
SoapWrapper<T>::SoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive):
    m_soapProxy(nullptr),
    m_endpoint(nullptr),
    m_timeDrift(timeDrift),
    m_login(login),
    m_passwd(passwd),
    m_invoked(false)
{
    //NX_ASSERT(!endpoint.empty());
    NX_ASSERT(!endpoint.empty(), Q_FUNC_INFO, "Onvif URL is empty!!! It is debug only check.");
    m_endpoint = new char[endpoint.size() + 1];
    strcpy(m_endpoint, endpoint.c_str());
    m_endpoint[endpoint.size()] = '\0';

    if( !tcpKeepAlive )
        m_soapProxy = new T();
    else
        m_soapProxy = new T( SOAP_IO_KEEPALIVE, SOAP_IO_KEEPALIVE );

    auto timeouts = getSoapTimeouts();

    m_soapProxy->soap->send_timeout = timeouts.sendTimeoutSeconds.count();
    m_soapProxy->soap->recv_timeout = timeouts.recvTimeoutSeconds.count();
    m_soapProxy->soap->connect_timeout = timeouts.connectTimeoutSeconds.count();
    m_soapProxy->soap->accept_timeout = timeouts.acceptTimeoutSeconds.count();

    soap_register_plugin(m_soapProxy->soap, soap_wsse);

    m_soapProxy->soap->namespaces = onvifOverriddenNamespaces;
}

template <class T>
SoapWrapper<T>::~SoapWrapper()
{
    if (m_invoked)
    {
        soap_destroy(m_soapProxy->soap);
        soap_end(m_soapProxy->soap);
    }
    delete[] m_endpoint;
    delete m_soapProxy;
}

template <class T>
void SoapWrapper<T>::setLogin(const QString &login) {
    m_login = login;
}

template <class T>
void SoapWrapper<T>::setPassword(const QString &password) {
    m_passwd = password;
}

void correctTimeInternal(char* buffer, const QDateTime& dt)
{
    if (strlen(buffer) > 19) {
        QByteArray datetime = dt.toString(Qt::ISODate).toLatin1();
        memcpy(buffer, datetime.data(), 19);
    }
}

template <class T>
void SoapWrapper<T>::beforeMethodInvocation()
{
    if (m_invoked)
    {
        soap_destroy(m_soapProxy->soap);
        soap_end(m_soapProxy->soap);
    }
    else
    {
        m_invoked = true;
    }

    if (!m_login.isEmpty())
        soap_wsse_add_UsernameTokenDigest(m_soapProxy->soap, NULL, m_login.toUtf8().constData(), m_passwd.toUtf8().constData(), time(NULL) + m_timeDrift);
}

template <class T>
QString SoapWrapper<T>::getLogin()
{
    return m_login;
}

template <class T>
QString SoapWrapper<T>::getPassword()
{
    return m_passwd;
}

template <class T>
int SoapWrapper<T>::getTimeDrift()
{
    return m_timeDrift;
}

template <class T>
const QString SoapWrapper<T>::getLastError()
{
    return SoapErrorHelper::fetchDescription(m_soapProxy->soap_fault());
}

template <class T>
const QString SoapWrapper<T>::getEndpointUrl()
{
    return QLatin1String(m_endpoint);
}

template <class T>
bool SoapWrapper<T>::isNotAuthenticated()
{
    return PasswordHelper::isNotAuthenticated(m_soapProxy->soap_fault());
}

template <class T>
bool SoapWrapper<T>::isConflictError()
{
    return PasswordHelper::isConflictError(m_soapProxy->soap_fault());
}


// -------------------------------------------------------------------------- //
// DeviceSoapWrapper
// -------------------------------------------------------------------------- //
DeviceSoapWrapper::DeviceSoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive):
    SoapWrapper<DeviceBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

DeviceSoapWrapper::~DeviceSoapWrapper()
{
}

void DeviceSoapWrapper::calcTimeDrift()
{
    _onvifDevice__GetSystemDateAndTime request;
    _onvifDevice__GetSystemDateAndTimeResponse response;
    int soapRes = GetSystemDateAndTime(request, response);

    if (soapRes == SOAP_OK && response.SystemDateAndTime && response.SystemDateAndTime->UTCDateTime)
    {
        onvifXsd__Date* date = response.SystemDateAndTime->UTCDateTime->Date;
        onvifXsd__Time* time = response.SystemDateAndTime->UTCDateTime->Time;

        QDateTime datetime(QDate(date->Year, date->Month, date->Day), QTime(time->Hour, time->Minute, time->Second), Qt::UTC);
        m_timeDrift = datetime.toMSecsSinceEpoch()/1000 - QDateTime::currentMSecsSinceEpoch()/1000;
    }
}

QAuthenticator DeviceSoapWrapper::getDefaultPassword(const QString& manufacturer, const QString& model) const
{
    QAuthenticator result;

    QnResourceData resourceData = qnStaticCommon->dataPool()->data(manufacturer, model);
    QString credentials = resourceData.value<QString>(lit("defaultCredentials"));
    QStringList parts = credentials.split(L':');
    if (parts.size() == 2) {
        result.setUser(parts[0]);
        result.setPassword(parts[1]);
    }

    return result;
}

std::list<nx::common::utils::Credentials> DeviceSoapWrapper::getPossibleCredentials(
    const QString& manufacturer,
    const QString& model) const
{
    QnResourceData resData = qnStaticCommon->dataPool()->data(manufacturer, model);
    auto credentials = resData.value<QList<nx::common::utils::Credentials>>(
        Qn::POSSIBLE_DEFAULT_CREDENTIALS_PARAM_NAME);

    return credentials.toStdList();
}

nx::common::utils::Credentials DeviceSoapWrapper::getForcedCredentials(
    const QString& manufacturer,
    const QString& model)
{
    QnResourceData resData = qnStaticCommon->dataPool()->data(manufacturer, model);
    auto credentials = resData.value<nx::common::utils::Credentials>(
        Qn::FORCED_DEFAULT_CREDENTIALS_PARAM_NAME);

    return credentials;
}

bool DeviceSoapWrapper::fetchLoginPassword(const QString& manufacturer, const QString& model)
{
    auto forcedCredentials = getForcedCredentials(manufacturer, model);

    if (!forcedCredentials.user.isEmpty())
    {
        setLogin(forcedCredentials.user);
        setPassword(forcedCredentials.password);
        return true;
    }

    const auto oldCredentials =
        PasswordHelper::instance()->getCredentialsByManufacturer(manufacturer);

    auto possibleCredentials = oldCredentials;

    const auto credentialsFromResourceData = getPossibleCredentials(manufacturer, model);
    for (const auto& credentials: credentialsFromResourceData)
    {
        if (!oldCredentials.contains(credentials))
            possibleCredentials.append(credentials);
    }

    QnResourceData resData = qnStaticCommon->dataPool()->data(manufacturer, model);
    int timeoutSec = resData.value<int>(Qn::kUnauthrizedTimeoutParamName);
    if (timeoutSec > 0 && possibleCredentials.size() > 1)
    {
        possibleCredentials.erase(possibleCredentials.begin() + 1, possibleCredentials.end());
        NX_WARNING(this,
            lm("Discovery----: strict credentials list for camera %1 to 1 record. because of non zero '%2' parameter.").
            arg(getEndpointUrl()).
            arg(Qn::kUnauthrizedTimeoutParamName));
    }

    if (possibleCredentials.size() <= 1)
    {
        nx::common::utils::Credentials credentials;
        if (!possibleCredentials.isEmpty())
            credentials = possibleCredentials.first();
        setLogin(credentials.user);
        setPassword(credentials.password);
        return true;
    }

    // Start logging timeouts as soon as network requests appear.
    QElapsedTimer timer;
    timer.restart();
    auto logTimeout = [&](bool found)
    {
        NX_LOG(lit("Discovery----: autodetect credentials for camera %1 took %2 ms. Credentials found: %3").
            arg(getEndpointUrl()).
            arg(timer.elapsed()).
            arg(found),
            cl_logDEBUG1);
    };

    calcTimeDrift();
    for (const auto& credentials: possibleCredentials)
    {
        if (QnResource::isStopping())
            return false;

        setLogin(credentials.user);
        setPassword(credentials.password);

        NetIfacesReq request;
        NetIfacesResp response;
        auto soapRes = getNetworkInterfaces(request, response);

        if (soapRes == SOAP_OK || !isNotAuthenticated())
        {
            logTimeout(soapRes == SOAP_OK);
            return soapRes == SOAP_OK;
        }
    }
    logTimeout(false);

    const auto& credentials = possibleCredentials.first();
    setLogin(credentials.user);
    setPassword(credentials.password);

    return false;
}

int DeviceSoapWrapper::getNetworkInterfaces(NetIfacesReq& request, NetIfacesResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetNetworkInterfaces(m_endpoint, NULL, &request, &response);
}

int DeviceSoapWrapper::createUsers(CreateUsersReq& request, CreateUsersResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->CreateUsers(m_endpoint, NULL, &request, &response);
}

int DeviceSoapWrapper::getDeviceInformation(DeviceInfoReq& request, DeviceInfoResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetDeviceInformation(m_endpoint, NULL, &request, &response);
}

int DeviceSoapWrapper::getServiceCapabilities( _onvifDevice__GetServiceCapabilities& request, _onvifDevice__GetServiceCapabilitiesResponse& response )
{
    return invokeMethod( &DeviceBindingProxy::GetServiceCapabilities, &request, &response );
}

int DeviceSoapWrapper::getRelayOutputs( _onvifDevice__GetRelayOutputs& request, _onvifDevice__GetRelayOutputsResponse& response )
{
    return invokeMethod( &DeviceBindingProxy::GetRelayOutputs, &request, &response );
}

int DeviceSoapWrapper::setRelayOutputState( _onvifDevice__SetRelayOutputState& request, _onvifDevice__SetRelayOutputStateResponse& response )
{
    return invokeMethod( &DeviceBindingProxy::SetRelayOutputState, &request, &response );
}

int DeviceSoapWrapper::setRelayOutputSettings( _onvifDevice__SetRelayOutputSettings& request, _onvifDevice__SetRelayOutputSettingsResponse& response )
{
    return invokeMethod( &DeviceBindingProxy::SetRelayOutputSettings, &request, &response );
}

int DeviceSoapWrapper::getCapabilities(CapabilitiesReq& request, CapabilitiesResp& response)
{
    beforeMethodInvocation();
    int rez = m_soapProxy->GetCapabilities(m_endpoint, NULL, &request, &response);
    return rez;
}

int DeviceSoapWrapper::GetSystemDateAndTime(_onvifDevice__GetSystemDateAndTime& request, _onvifDevice__GetSystemDateAndTimeResponse& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetSystemDateAndTime(m_endpoint, NULL, &request, &response);
}


int DeviceSoapWrapper::systemReboot(RebootReq& request, RebootResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->SystemReboot(m_endpoint, NULL, &request, &response);
}

int DeviceSoapWrapper::systemFactoryDefaultHard(FactoryDefaultReq& request, FactoryDefaultResp& response)
{
    beforeMethodInvocation();
    request.FactoryDefault = onvifXsd__FactoryDefaultType__Hard;
    return m_soapProxy->SetSystemFactoryDefault(m_endpoint, NULL, &request, &response);
}

int DeviceSoapWrapper::systemFactoryDefaultSoft(FactoryDefaultReq& request, FactoryDefaultResp& response)
{
    beforeMethodInvocation();
    request.FactoryDefault = onvifXsd__FactoryDefaultType__Soft;
    return m_soapProxy->SetSystemFactoryDefault(m_endpoint, NULL, &request, &response);
}



// -------------------------------------------------------------------------- //
// DeviceIOWrapper
// -------------------------------------------------------------------------- //
DeviceIOWrapper::DeviceIOWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive):
    SoapWrapper<DeviceIOBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

DeviceIOWrapper::~DeviceIOWrapper()
{
}

int DeviceIOWrapper::getDigitalInputs( _onvifDeviceIO__GetDigitalInputs& request, _onvifDeviceIO__GetDigitalInputsResponse& response )
{
    return invokeMethod( &DeviceIOBindingProxy::GetDigitalInputs, &request, &response );
}

int DeviceIOWrapper::getRelayOutputs( _onvifDevice__GetRelayOutputs& request, _onvifDevice__GetRelayOutputsResponse& response )
{
    return invokeMethod( &DeviceIOBindingProxy::GetRelayOutputs, &request, &response );
}

int DeviceIOWrapper::getRelayOutputOptions( _onvifDeviceIO__GetRelayOutputOptions& request, _onvifDeviceIO__GetRelayOutputOptionsResponse& response )
{
    return invokeMethod( &DeviceIOBindingProxy::GetRelayOutputOptions, &request, &response );
}

int DeviceIOWrapper::setRelayOutputSettings( _onvifDeviceIO__SetRelayOutputSettings& request, _onvifDeviceIO__SetRelayOutputSettingsResponse& response )
{
    return invokeMethod( &DeviceIOBindingProxy::SetRelayOutputSettings, &request, &response );
}


// -------------------------------------------------------------------------- //
// MediaSoapWrapper
// -------------------------------------------------------------------------- //
MediaSoapWrapper::MediaSoapWrapper(const std::string& endpoint, const QString &login, const QString &passwd, int timeDrift, bool tcpKeepAlive):
    SoapWrapper<MediaBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive)
{

}

MediaSoapWrapper::~MediaSoapWrapper()
{

}

int MediaSoapWrapper::getVideoEncoderConfigurationOptions(VideoOptionsReq& request, VideoOptionsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetVideoEncoderConfigurationOptions(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getAudioEncoderConfigurationOptions(AudioOptionsReq& request, AudioOptionsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetAudioEncoderConfigurationOptions(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getVideoSourceConfigurations(VideoSrcConfigsReq& request, VideoSrcConfigsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetVideoSourceConfigurations(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getAudioOutputs( _onvifMedia__GetAudioOutputs& request, _onvifMedia__GetAudioOutputsResponse& response )
{
    beforeMethodInvocation();
    return m_soapProxy->GetAudioOutputs(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getVideoSources(_onvifMedia__GetVideoSources& request, _onvifMedia__GetVideoSourcesResponse& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetVideoSources(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getCompatibleMetadataConfigurations(CompatibleMetadataConfiguration& request, CompatibleMetadataConfigurationResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetCompatibleMetadataConfigurations(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getVideoEncoderConfigurations(VideoConfigsReq& request, VideoConfigsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetVideoEncoderConfigurations(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getProfiles(ProfilesReq& request, ProfilesResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetProfiles(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::addVideoSourceConfiguration(AddVideoSrcConfigReq& request, AddVideoSrcConfigResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->AddVideoSourceConfiguration(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::createProfile(CreateProfileReq& request, CreateProfileResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->CreateProfile(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::addVideoEncoderConfiguration(AddVideoConfigReq& request, AddVideoConfigResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->AddVideoEncoderConfiguration(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::addPTZConfiguration(AddPTZConfigReq& request, AddPTZConfigResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->AddPTZConfiguration(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::setVideoEncoderConfiguration(SetVideoConfigReq& request, SetVideoConfigResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->SetVideoEncoderConfiguration(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getProfile(ProfileReq& request, ProfileResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetProfile(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getStreamUri(StreamUriReq& request, StreamUriResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetStreamUri(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::setVideoSourceConfiguration(SetVideoSrcConfigReq& request, SetVideoSrcConfigResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->SetVideoSourceConfiguration(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getAudioEncoderConfigurations(AudioConfigsReq& request, AudioConfigsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetAudioEncoderConfigurations(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::addAudioEncoderConfiguration(AddAudioConfigReq& request, AddAudioConfigResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->AddAudioEncoderConfiguration(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::setAudioEncoderConfiguration(SetAudioConfigReq& request, SetAudioConfigResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->SetAudioEncoderConfiguration(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getAudioSourceConfigurations(AudioSrcConfigsReq& request, AudioSrcConfigsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetAudioSourceConfigurations(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::setAudioSourceConfiguration(SetAudioSrcConfigReq& request, SetAudioSrcConfigResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->SetAudioSourceConfiguration(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::addAudioSourceConfiguration(AddAudioSrcConfigReq& request, AddAudioSrcConfigResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->AddAudioSourceConfiguration(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getVideoSourceConfigurationOptions(VideoSrcOptionsReq& request, VideoSrcOptionsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetVideoSourceConfigurationOptions(m_endpoint, NULL, &request, &response);
}

int MediaSoapWrapper::getVideoEncoderConfiguration(VideoConfigReq& request, VideoConfigResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetVideoEncoderConfiguration(m_endpoint, NULL, &request, &response);
}


// -------------------------------------------------------------------------- //
// ImagingSoapWrapper
// -------------------------------------------------------------------------- //
ImagingSoapWrapper::ImagingSoapWrapper(const std::string& endpoint, const QString &login, const QString &passwd, int timeDrift, bool tcpKeepAlive):
    SoapWrapper<ImagingBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

ImagingSoapWrapper::~ImagingSoapWrapper()
{
}

int ImagingSoapWrapper::getOptions(ImagingOptionsReq& request, ImagingOptionsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetOptions(m_endpoint, NULL, &request, &response);
}

int ImagingSoapWrapper::getImagingSettings(ImagingSettingsReq& request, ImagingSettingsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetImagingSettings(m_endpoint, NULL, &request, &response);
}

int ImagingSoapWrapper::setImagingSettings(SetImagingSettingsReq& request, SetImagingSettingsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->SetImagingSettings(m_endpoint, NULL, &request, &response);
}

int ImagingSoapWrapper::getMoveOptions(_onvifImg__GetMoveOptions &request, _onvifImg__GetMoveOptionsResponse &response)
{
    return invokeMethod(&ImagingBindingProxy::GetMoveOptions, &request, &response);
}

int ImagingSoapWrapper::move(_onvifImg__Move &request, _onvifImg__MoveResponse &response)
{
    return invokeMethod(&ImagingBindingProxy::Move, &request, &response);
}


// -------------------------------------------------------------------------- //
// PtzSoapWrapper
// -------------------------------------------------------------------------- //
PtzSoapWrapper::PtzSoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive):
    SoapWrapper<PTZBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

PtzSoapWrapper::~PtzSoapWrapper()
{
}

int PtzSoapWrapper::doAbsoluteMove(AbsoluteMoveReq& request, AbsoluteMoveResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->AbsoluteMove(m_endpoint, NULL, &request, &response);
}

int PtzSoapWrapper::gotoPreset(GotoPresetReq& request, GotoPresetResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GotoPreset(m_endpoint, NULL, &request, &response);
}

int PtzSoapWrapper::setPreset(SetPresetReq& request, SetPresetResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->SetPreset(m_endpoint, NULL, &request, &response);
}

int PtzSoapWrapper::getPresets(GetPresetsReq& request, GetPresetsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetPresets(m_endpoint, NULL, &request, &response);
}

int PtzSoapWrapper::removePreset(RemovePresetReq& request, RemovePresetResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->RemovePreset(m_endpoint, NULL, &request, &response);
}

int PtzSoapWrapper::doGetNode(_onvifPtz__GetNode& request, _onvifPtz__GetNodeResponse& response)
{
    beforeMethodInvocation();
    int rez = m_soapProxy->GetNode(m_endpoint, NULL, &request, &response);
    if (rez != SOAP_OK)
    {
        qWarning() << "PTZ settings reading error: " << m_endpoint <<  ". " << getLastError();
    }
    return rez;
}

int PtzSoapWrapper::doGetNodes(_onvifPtz__GetNodes& request, _onvifPtz__GetNodesResponse& response)
{
    beforeMethodInvocation();
    int rez = m_soapProxy->GetNodes(m_endpoint, NULL, &request, &response);
    return rez;
}

int PtzSoapWrapper::doGetConfigurations(_onvifPtz__GetConfigurations& request, _onvifPtz__GetConfigurationsResponse& response)
{
    beforeMethodInvocation();
    int rez = m_soapProxy->GetConfigurations(m_endpoint, NULL, &request, &response);
    return rez;
}

int PtzSoapWrapper::doContinuousMove(_onvifPtz__ContinuousMove& request, _onvifPtz__ContinuousMoveResponse& response)
{
    beforeMethodInvocation();
    return m_soapProxy->ContinuousMove(m_endpoint, NULL, &request, &response);
}

int PtzSoapWrapper::doGetStatus(_onvifPtz__GetStatus& request, _onvifPtz__GetStatusResponse& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetStatus(m_endpoint, NULL, &request, &response);
}

int PtzSoapWrapper::doStop(_onvifPtz__Stop& request, _onvifPtz__StopResponse& response)
{
    beforeMethodInvocation();
    return m_soapProxy->Stop(m_endpoint, NULL, &request, &response);
}

int PtzSoapWrapper::doGetServiceCapabilities(PtzGetServiceCapabilitiesReq& request, PtzPtzGetServiceCapabilitiesResp& response)
{
    beforeMethodInvocation();
    int rez = m_soapProxy->GetServiceCapabilities(m_endpoint, NULL, &request, &response);
    return rez;
}


// -------------------------------------------------------------------------- //
// NotificationProducerSoapWrapper
// -------------------------------------------------------------------------- //
NotificationProducerSoapWrapper::NotificationProducerSoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive):
    SoapWrapper<NotificationProducerBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

int NotificationProducerSoapWrapper::Subscribe( _oasisWsnB2__Subscribe* const request, _oasisWsnB2__SubscribeResponse* const response )
{
    return invokeMethod( &NotificationProducerBindingProxy::Subscribe, request, response );
}


// -------------------------------------------------------------------------- //
// CreatePullPointSoapWrapper
// -------------------------------------------------------------------------- //
CreatePullPointSoapWrapper::CreatePullPointSoapWrapper(const std::string& endpoint, const QString &login, const QString &passwd, int timeDrift, bool tcpKeepAlive):
    SoapWrapper<CreatePullPointBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

int CreatePullPointSoapWrapper::createPullPoint( _oasisWsnB2__CreatePullPoint& request, _oasisWsnB2__CreatePullPointResponse& response )
{
    return invokeMethod( &CreatePullPointBindingProxy::CreatePullPoint, &request, &response );
}


// -------------------------------------------------------------------------- //
// CreatePullPointSoapWrapper
// -------------------------------------------------------------------------- //
PullPointSubscriptionWrapper::PullPointSubscriptionWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive):
    SoapWrapper<PullPointSubscriptionBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

int PullPointSubscriptionWrapper::pullMessages( _onvifEvents__PullMessages& request, _onvifEvents__PullMessagesResponse& response )
{
    return invokeMethod( &PullPointSubscriptionBindingProxy::PullMessages, &request, &response );
}


// -------------------------------------------------------------------------- //
// EventSoapWrapper
// -------------------------------------------------------------------------- //
EventSoapWrapper::EventSoapWrapper(const std::string& endpoint, const QString &login, const QString &passwd, int timeDrift, bool tcpKeepAlive):
    SoapWrapper<EventBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

int EventSoapWrapper::createPullPointSubscription(
    _onvifEvents__CreatePullPointSubscription& request,
    _onvifEvents__CreatePullPointSubscriptionResponse& response )
{
    return invokeMethod( &EventBindingProxy::CreatePullPointSubscription, &request, &response );
}


// -------------------------------------------------------------------------- //
// SubscriptionManagerSoapWrapper
// -------------------------------------------------------------------------- //
SubscriptionManagerSoapWrapper::SubscriptionManagerSoapWrapper(const std::string& endpoint, const QString& login, const QString& passwd, int timeDrift, bool tcpKeepAlive):
    SoapWrapper<SubscriptionManagerBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive)
{
}

int SubscriptionManagerSoapWrapper::renew( _oasisWsnB2__Renew& request, _oasisWsnB2__RenewResponse& response )
{
    return invokeMethod( &SubscriptionManagerBindingProxy::Renew, &request, &response );
}

int SubscriptionManagerSoapWrapper::unsubscribe( _oasisWsnB2__Unsubscribe& request, _oasisWsnB2__UnsubscribeResponse& response )
{
    return invokeMethod( &SubscriptionManagerBindingProxy::Unsubscribe, &request, &response );
}



//
// Explicit instantiating
//
template QString SoapWrapper<DeviceBindingProxy>::getLogin();
template QString SoapWrapper<DeviceBindingProxy>::getPassword();
template void SoapWrapper<DeviceBindingProxy>::setLogin(const QString &login);
template void SoapWrapper<DeviceBindingProxy>::setPassword(const QString &password);
template int SoapWrapper<DeviceBindingProxy>::getTimeDrift();
template const QString SoapWrapper<DeviceBindingProxy>::getLastError();
template const QString SoapWrapper<DeviceBindingProxy>::getEndpointUrl();
template bool SoapWrapper<DeviceBindingProxy>::isNotAuthenticated();
template bool SoapWrapper<DeviceBindingProxy>::isConflictError();

template QString SoapWrapper<MediaBindingProxy>::getLogin();
template QString SoapWrapper<MediaBindingProxy>::getPassword();
template int SoapWrapper<MediaBindingProxy>::getTimeDrift();
template const QString SoapWrapper<MediaBindingProxy>::getLastError();
template const QString SoapWrapper<MediaBindingProxy>::getEndpointUrl();
template bool SoapWrapper<MediaBindingProxy>::isNotAuthenticated();
template bool SoapWrapper<MediaBindingProxy>::isConflictError();

template QString SoapWrapper<PTZBindingProxy>::getLogin();
template QString SoapWrapper<PTZBindingProxy>::getPassword();
template int SoapWrapper<PTZBindingProxy>::getTimeDrift();
template const QString SoapWrapper<PTZBindingProxy>::getLastError();
template const QString SoapWrapper<PTZBindingProxy>::getEndpointUrl();
template bool SoapWrapper<PTZBindingProxy>::isNotAuthenticated();
template bool SoapWrapper<PTZBindingProxy>::isConflictError();

template QString SoapWrapper<ImagingBindingProxy>::getLogin();
template QString SoapWrapper<ImagingBindingProxy>::getPassword();
template int SoapWrapper<ImagingBindingProxy>::getTimeDrift();
template const QString SoapWrapper<ImagingBindingProxy>::getLastError();
template const QString SoapWrapper<ImagingBindingProxy>::getEndpointUrl();
template bool SoapWrapper<ImagingBindingProxy>::isNotAuthenticated();
template bool SoapWrapper<ImagingBindingProxy>::isConflictError();

#endif //ENABLE_ONVIF
