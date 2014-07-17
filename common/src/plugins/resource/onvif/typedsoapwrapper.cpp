
#if 0

#include "openssl/evp.h"

#include <onvif/Onvif.nsmap>
#include <onvif/soapDeviceBindingProxy.h>
#include <onvif/soapMediaBindingProxy.h>
#include <onvif/soapPTZBindingProxy.h>
#include <onvif/soapImagingBindingProxy.h>
#include <onvif/wsseapi.h>

#include "typedsoapwrapper.h"


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

#endif
