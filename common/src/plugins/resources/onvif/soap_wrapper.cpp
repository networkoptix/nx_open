#ifdef WIN32
#include "openssl/evp.h"
#else
#include "evp.h"
#endif

#include "soap_wrapper.h"
#include "onvif/Onvif.nsmap"
#include "onvif/soapDeviceBindingProxy.h"
#include "onvif/soapMediaBindingProxy.h"
#include "onvif/wsseapi.h"

#include <QtGlobal>

const int SOAP_RECEIVE_TIMEOUT = 1; //in seconds
const int SOAP_SEND_TIMEOUT = 1; //in seconds
const std::string DEFAULT_ONVIF_LOGIN = "admin";
const std::string DEFAULT_ONVIF_PASSWORD = "admin";

template <class T>
SoapWrapper<T>::SoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd):
    m_login(0),
    m_passwd(0),
    invoked(false)
{
    Q_ASSERT(!endpoint.empty());
    m_endpoint = new char[endpoint.size() + 1];
    strcpy(m_endpoint, endpoint.c_str());
    m_endpoint[endpoint.size()] = '\0';

    setLoginPassword(login, passwd);

    m_soapProxy = new T();
    m_soapProxy->soap->send_timeout = SOAP_SEND_TIMEOUT;
    m_soapProxy->soap->recv_timeout = SOAP_RECEIVE_TIMEOUT;

    soap_register_plugin(m_soapProxy->soap, soap_wsse);
}

template <class T>
SoapWrapper<T>::~SoapWrapper()
{
    if (invoked) soap_end(m_soapProxy->soap);

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
    if (login.empty()) {
        return;
    }

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

template <class T>
void SoapWrapper<T>::beforeMethodInvocation()
{
    if (invoked)
        soap_end(m_soapProxy->soap);
    else
        invoked = true;

    if (m_login) soap_wsse_add_UsernameTokenDigest(m_soapProxy->soap, "Id", m_login, m_passwd);
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
QString SoapWrapper<T>::getLastError()
{
    return SoapErrorHelper::fetchDescription(m_soapProxy->soap_fault());
}

template <class T>
bool SoapWrapper<T>::isNotAuthenticated()
{
    return PasswordHelper::isNotAuthenticated(m_soapProxy->soap_fault());
}

//
// DeviceSoapWrapper
//

DeviceSoapWrapper::DeviceSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd):
    SoapWrapper<DeviceBindingProxy>(endpoint, login, passwd),
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

            return false;
        }

        login = passwdIter->first;
        passwd = passwdIter->second;
        ++passwdIter;

    } while (true);

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
    return m_soapProxy->GetCapabilities(m_endpoint, NULL, &request, &response);
}

//
// MediaSoapWrapper
//

MediaSoapWrapper::MediaSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd):
    SoapWrapper<MediaBindingProxy>(endpoint, login, passwd),
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

int MediaSoapWrapper::getVideoSourceConfigurations(VideoSrcConfigsReq& request, VideoSrcConfigsResp& response)
{
    beforeMethodInvocation();
    return m_soapProxy->GetVideoSourceConfigurations(m_endpoint, NULL, &request, &response);
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

//
// Explicit instantiating
//

template soap* SoapWrapper<DeviceBindingProxy>::getSoap();
template const char* SoapWrapper<DeviceBindingProxy>::getLogin();
template const char* SoapWrapper<DeviceBindingProxy>::getPassword();
template QString SoapWrapper<DeviceBindingProxy>::getLastError();
template bool SoapWrapper<DeviceBindingProxy>::isNotAuthenticated();

template soap* SoapWrapper<MediaBindingProxy>::getSoap();
template const char* SoapWrapper<MediaBindingProxy>::getLogin();
template const char* SoapWrapper<MediaBindingProxy>::getPassword();
template QString SoapWrapper<MediaBindingProxy>::getLastError();
template bool SoapWrapper<MediaBindingProxy>::isNotAuthenticated();
