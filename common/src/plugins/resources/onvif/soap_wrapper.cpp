#include "openssl/evp.h"

#include "soap_wrapper.h"
#include "onvif/Onvif.nsmap"
#include "onvif/soapDeviceBindingProxy.h"
#include "onvif/soapMediaBindingProxy.h"
#include "onvif/soapPTZBindingProxy.h"
#include "onvif/soapImagingBindingProxy.h"
#include "onvif/wsseapi.h"

#include <QtGlobal>

namespace {

/** Size of the random nonce */
#define SOAP_WSSE_NONCELEN	(20)

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

} // anonymous namespace



const int SOAP_RECEIVE_TIMEOUT = 4; // "+" in seconds, "-" in mseconds
const int SOAP_SEND_TIMEOUT = 4; // "+" in seconds, "-" in mseconds
const int SOAP_CONNECT_TIMEOUT = 4; // "+" in seconds, "-" in mseconds
const int SOAP_ACCEPT_TIMEOUT = 4; // "+" in seconds, "-" in mseconds
const std::string DEFAULT_ONVIF_LOGIN = "admin";
const std::string DEFAULT_ONVIF_PASSWORD = "admin";
static const int DIGEST_TIMEOUT_SEC = 60;

template <class T>
SoapWrapper<T>::SoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd, int _timeDrift):
    m_login(0),
    m_passwd(0),
    invoked(false),
    m_timeDrift(_timeDrift)
{
    //Q_ASSERT(!endpoint.empty());
	Q_ASSERT_X(!endpoint.empty(), Q_FUNC_INFO, "Onvif URL is empty!!! It is debug only check.");
    m_endpoint = new char[endpoint.size() + 1];
    strcpy(m_endpoint, endpoint.c_str());
    m_endpoint[endpoint.size()] = '\0';

    setLoginPassword(login, passwd);

    m_soapProxy = new T();
    m_soapProxy->soap->send_timeout = SOAP_SEND_TIMEOUT;
    m_soapProxy->soap->recv_timeout = SOAP_RECEIVE_TIMEOUT;
    m_soapProxy->soap->connect_timeout = SOAP_CONNECT_TIMEOUT;
    m_soapProxy->soap->accept_timeout = SOAP_ACCEPT_TIMEOUT;

    soap_register_plugin(m_soapProxy->soap, soap_wsse);
}

template <class T>
SoapWrapper<T>::~SoapWrapper()
{
    if (invoked) 
    {
        soap_destroy(m_soapProxy->soap);
        soap_end(m_soapProxy->soap);
    }

    cleanLoginPassword();

    delete[] m_endpoint;
    delete m_soapProxy;
}

template <class T>
void SoapWrapper<T>::cleanLoginPassword()
{
    if (m_login) {
        delete[] m_login;
        delete[] m_passwd;
    }

    m_login = 0;
    m_passwd = 0;
}

template <class T>
void SoapWrapper<T>::setLoginPassword(const std::string& login, const std::string& passwd)
{
    cleanLoginPassword();

    m_login = new char[login.size() + 1];
    strcpy(m_login, login.c_str());
    m_login[login.size()] = '\0';

    m_passwd = new char[passwd.size() + 1];
    strcpy(m_passwd, passwd.c_str());
    m_passwd[passwd.size()] = '\0';
}

template <class T>
soap* SoapWrapper<T>::getSoap()
{
    return m_soapProxy->soap;
}

void correctTimeInternal(char* buffer, const QDateTime& dt)
{
    if (strlen(buffer) > 19) {
        QByteArray datetime = dt.toString(Qt::ISODate).toLocal8Bit();
        memcpy(buffer, datetime.data(), 19);
    }
}

template <class T>
void SoapWrapper<T>::beforeMethodInvocation()
{
    if (invoked)
    {
        soap_destroy(m_soapProxy->soap);
        soap_end(m_soapProxy->soap);
    }
    else
    {
        invoked = true;
    }

    if (m_login && *m_login)
        soap_wsse_add_UsernameTokenDigest(m_soapProxy->soap, NULL, m_login, m_passwd, time(NULL) + m_timeDrift);
}

template <class T>
const char* SoapWrapper<T>::getLogin()
{
    return m_login;
}

template <class T>
const char* SoapWrapper<T>::getPassword()
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

//
// DeviceSoapWrapper
//

DeviceSoapWrapper::DeviceSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd, int _timeDrift):
    SoapWrapper<DeviceBindingProxy>(endpoint, login, passwd, _timeDrift),
    passwordsData(PasswordHelper::instance())
{

}

DeviceSoapWrapper::~DeviceSoapWrapper()
{

}

bool DeviceSoapWrapper::fetchLoginPassword(const QString& manufacturer)
{
    PasswordList passwords = passwordsData.getPasswordsByManufacturer(manufacturer);
    PasswordList::ConstIterator passwdIter = passwords.begin();

    std::string login;
    std::string passwd;
    int soapRes = SOAP_OK;
    do {
        qDebug() << "Trying login = " << login.c_str() << ", password = " << passwd.c_str();
        setLoginPassword(login, passwd);

        NetIfacesReq request1;
        NetIfacesResp response1;
        soapRes = getNetworkInterfaces(request1, response1);

        if (soapRes == SOAP_OK || !isNotAuthenticated()) {
            qDebug() << "Finished picking password";

            return soapRes == SOAP_OK;
        }

        if (passwdIter == passwords.end()) {
            //If we had no luck in picking a password, let's try to create a user
            qDebug() << "Trying to create a user admin/admin";
            setLoginPassword(std::string(), std::string());

            onvifXsd__User newUser;
            newUser.Username = DEFAULT_ONVIF_LOGIN;
            newUser.Password = const_cast<std::string*>(&DEFAULT_ONVIF_PASSWORD);

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
                    setLoginPassword(DEFAULT_ONVIF_LOGIN, DEFAULT_ONVIF_PASSWORD);
                    return true;
                }

                qDebug() << "User is NOT created";
            }

            setLoginPassword(std::string(), std::string());
            return false;
        }

        login = passwdIter->first;
        passwd = passwdIter->second;
        ++passwdIter;

    } while (true);

    setLoginPassword(std::string(), std::string());
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

//
// MediaSoapWrapper
//

MediaSoapWrapper::MediaSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd, int _timeDrift):
    SoapWrapper<MediaBindingProxy>(endpoint, login, passwd, _timeDrift),
    passwordsData(PasswordHelper::instance())
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

//
// PtzSoapWrapper
//

PtzSoapWrapper::PtzSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd, int _timeDrift):
    SoapWrapper<PTZBindingProxy>(endpoint, login, passwd, _timeDrift),
    passwordsData(PasswordHelper::instance())
{

}

PtzSoapWrapper::~PtzSoapWrapper()
{

}

//
// ImagingSoapWrapper
//

ImagingSoapWrapper::ImagingSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd, int _timeDrift):
    SoapWrapper<ImagingBindingProxy>(endpoint, login, passwd, _timeDrift),
    passwordsData(PasswordHelper::instance())
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

// ---------------------------------------- PtzSoapWrapper -------------------------------------------

int PtzSoapWrapper::doAbsoluteMove(AbsoluteMoveReq& request, AbsoluteMoveResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->AbsoluteMove(m_endpoint, NULL, &request, &response);
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

//
// Explicit instantiating
//

template soap* SoapWrapper<DeviceBindingProxy>::getSoap();
template const char* SoapWrapper<DeviceBindingProxy>::getLogin();
template const char* SoapWrapper<DeviceBindingProxy>::getPassword();
template int SoapWrapper<DeviceBindingProxy>::getTimeDrift();
template const QString SoapWrapper<DeviceBindingProxy>::getLastError();
template const QString SoapWrapper<DeviceBindingProxy>::getEndpointUrl();
template bool SoapWrapper<DeviceBindingProxy>::isNotAuthenticated();
template bool SoapWrapper<DeviceBindingProxy>::isConflictError();

template soap* SoapWrapper<MediaBindingProxy>::getSoap();
template const char* SoapWrapper<MediaBindingProxy>::getLogin();
template const char* SoapWrapper<MediaBindingProxy>::getPassword();
template int SoapWrapper<MediaBindingProxy>::getTimeDrift();
template const QString SoapWrapper<MediaBindingProxy>::getLastError();
template const QString SoapWrapper<MediaBindingProxy>::getEndpointUrl();
template bool SoapWrapper<MediaBindingProxy>::isNotAuthenticated();
template bool SoapWrapper<MediaBindingProxy>::isConflictError();

template soap* SoapWrapper<PTZBindingProxy>::getSoap();
template const char* SoapWrapper<PTZBindingProxy>::getLogin();
template const char* SoapWrapper<PTZBindingProxy>::getPassword();
template int SoapWrapper<PTZBindingProxy>::getTimeDrift();
template const QString SoapWrapper<PTZBindingProxy>::getLastError();
template const QString SoapWrapper<PTZBindingProxy>::getEndpointUrl();
template bool SoapWrapper<PTZBindingProxy>::isNotAuthenticated();
template bool SoapWrapper<PTZBindingProxy>::isConflictError();

template soap* SoapWrapper<ImagingBindingProxy>::getSoap();
template const char* SoapWrapper<ImagingBindingProxy>::getLogin();
template const char* SoapWrapper<ImagingBindingProxy>::getPassword();
template int SoapWrapper<ImagingBindingProxy>::getTimeDrift();
template const QString SoapWrapper<ImagingBindingProxy>::getLastError();
template const QString SoapWrapper<ImagingBindingProxy>::getEndpointUrl();
template bool SoapWrapper<ImagingBindingProxy>::isNotAuthenticated();
template bool SoapWrapper<ImagingBindingProxy>::isConflictError();
