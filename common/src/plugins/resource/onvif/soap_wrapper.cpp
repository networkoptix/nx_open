
#ifdef ENABLE_ONVIF

#include "openssl/evp.h"

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
#include <onvif/wsseapi.h>
#include <utils/common/log.h>
#include <QtCore/QtGlobal>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>


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



const int SOAP_RECEIVE_TIMEOUT = 30; // "+" in seconds, "-" in mseconds
const int SOAP_SEND_TIMEOUT = 30; // "+" in seconds, "-" in mseconds
const int SOAP_CONNECT_TIMEOUT = 5; // "+" in seconds, "-" in mseconds
const int SOAP_ACCEPT_TIMEOUT = 5; // "+" in seconds, "-" in mseconds
const QLatin1String DEFAULT_ONVIF_LOGIN = QLatin1String("admin");
const QLatin1String DEFAULT_ONVIF_PASSWORD = QLatin1String("admin");
static const int DIGEST_TIMEOUT_SEC = 60;


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
    //Q_ASSERT(!endpoint.empty());
    Q_ASSERT_X(!endpoint.empty(), Q_FUNC_INFO, "Onvif URL is empty!!! It is debug only check.");
    m_endpoint = new char[endpoint.size() + 1];
    strcpy(m_endpoint, endpoint.c_str());
    m_endpoint[endpoint.size()] = '\0';

    if( !tcpKeepAlive )
        m_soapProxy = new T();
    else
        m_soapProxy = new T( SOAP_IO_KEEPALIVE, SOAP_IO_KEEPALIVE );
    m_soapProxy->soap->send_timeout = SOAP_SEND_TIMEOUT;
    m_soapProxy->soap->recv_timeout = SOAP_RECEIVE_TIMEOUT;
    m_soapProxy->soap->connect_timeout = SOAP_CONNECT_TIMEOUT;
    m_soapProxy->soap->accept_timeout = SOAP_ACCEPT_TIMEOUT;

    soap_register_plugin(m_soapProxy->soap, soap_wsse);
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
    SoapWrapper<DeviceBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive),
    m_passwordsData(PasswordHelper::instance())
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

bool DeviceSoapWrapper::fetchLoginPassword(const QString& manufacturer)
{
    calcTimeDrift();

    PasswordList passwords = m_passwordsData.getPasswordsByManufacturer(manufacturer);
    PasswordList::ConstIterator passwdIter = passwords.begin();

    QString login;
    QString passwd;
    int soapRes = SOAP_OK;
    do {
        NX_LOG( QString::fromLatin1("Trying login = %1 password = %2)").arg(login).arg(passwd), cl_logDEBUG2 );
        setLogin(login);
        setPassword(passwd);

        NetIfacesReq request1;
        NetIfacesResp response1;
        soapRes = getNetworkInterfaces(request1, response1);

        if (soapRes == SOAP_OK || !isNotAuthenticated()) {
            NX_LOG( lit("Finished picking password"), cl_logDEBUG2 );
            return soapRes == SOAP_OK;
        }

        if (passwdIter == passwords.end()) {
            //If we had no luck in picking a password, let's try to create a user
            qDebug() << "Trying to create a user admin/admin";
            setLogin(QString());
            setPassword(QString());

            onvifXsd__User newUser;
            newUser.Username = QString(DEFAULT_ONVIF_LOGIN).toStdString();
            std::string pass = QString(DEFAULT_ONVIF_PASSWORD).toStdString();
            newUser.Password = const_cast<std::string*>(&pass);

            for (int i = 0; i <= 2; ++i) {
                CreateUsersReq request2;
                CreateUsersResp response2;

                switch (i) {
                    case 0: newUser.UserLevel = onvifXsd__UserLevel__Administrator; qDebug() << "Trying to create Admin"; break;
                    case 1: newUser.UserLevel = onvifXsd__UserLevel__Operator; qDebug() << "Trying to create Operator"; break;
                    case 2: newUser.UserLevel = onvifXsd__UserLevel__User; qDebug() << "Trying to create Regular User"; break;
                    default: newUser.UserLevel = onvifXsd__UserLevel__Administrator; qWarning() << "OnvifResourceInformationFetcher::findResources: unknown user index."; break;
                }
                request2.User.push_back(&newUser);

                soapRes = createUsers(request2, response2);
                if (soapRes == SOAP_OK) {
                    qDebug() << "User created!";
                    setLogin(DEFAULT_ONVIF_LOGIN);
                    setPassword(DEFAULT_ONVIF_PASSWORD);
                    return true;
                }

                qDebug() << "User is NOT created";
            }

            setLogin(QString());
            setPassword(QString());
            return false;
        }

        login = QString::fromUtf8(passwdIter->first);
        passwd = QString::fromUtf8(passwdIter->second);
        ++passwdIter;

    } while (true);

    setLogin(QString());
    setPassword(QString());
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
    SoapWrapper<MediaBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive),
    m_passwordsData(PasswordHelper::instance())
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
    SoapWrapper<ImagingBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive),
    m_passwordsData(PasswordHelper::instance())
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
    SoapWrapper<PTZBindingProxy>(endpoint, login, passwd, timeDrift, tcpKeepAlive),
    m_passwordsData(PasswordHelper::instance())
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
