#ifndef onvif_soap_wrapper_h
#define onvif_soap_wrapper_h

#include "onvif_helper.h"

struct soap;
class DeviceBindingProxy;
class MediaBindingProxy;

class _onvifDevice__GetNetworkInterfaces;
class _onvifDevice__GetNetworkInterfacesResponse;
class _onvifDevice__CreateUsers;
class _onvifDevice__CreateUsersResponse;
class _onvifDevice__GetDeviceInformation;
class _onvifDevice__GetDeviceInformationResponse;
class _onvifDevice__GetCapabilities;
class _onvifDevice__GetCapabilitiesResponse;

typedef _onvifDevice__GetNetworkInterfaces NetIfacesReq;
typedef _onvifDevice__GetNetworkInterfacesResponse NetIfacesResp;
typedef _onvifDevice__CreateUsers CreateUsersReq;
typedef _onvifDevice__CreateUsersResponse CreateUsersResp;
typedef _onvifDevice__GetDeviceInformation DeviceInfoReq;
typedef _onvifDevice__GetDeviceInformationResponse DeviceInfoResp;
typedef _onvifDevice__GetCapabilities CapabilitiesReq;
typedef _onvifDevice__GetCapabilitiesResponse CapabilitiesResp;

class _onvifMedia__GetVideoEncoderConfigurationOptions;
class _onvifMedia__GetVideoEncoderConfigurationOptionsResponse;
class _onvifMedia__GetVideoSourceConfigurations;
class _onvifMedia__GetVideoSourceConfigurationsResponse;
class _onvifMedia__GetVideoEncoderConfigurations;
class _onvifMedia__GetVideoEncoderConfigurationsResponse;
class _onvifMedia__GetProfiles;
class _onvifMedia__GetProfilesResponse;
class _onvifMedia__AddVideoSourceConfiguration;
class _onvifMedia__AddVideoSourceConfigurationResponse;
class _onvifMedia__CreateProfile;
class _onvifMedia__CreateProfileResponse;
class _onvifMedia__AddVideoEncoderConfiguration;
class _onvifMedia__AddVideoEncoderConfigurationResponse;
class _onvifMedia__SetVideoEncoderConfiguration;
class _onvifMedia__SetVideoEncoderConfigurationResponse;
class _onvifMedia__GetProfile;
class _onvifMedia__GetProfileResponse;
class _onvifMedia__GetStreamUri;
class _onvifMedia__GetStreamUriResponse;

typedef _onvifMedia__GetVideoEncoderConfigurationOptions VideoOptionsReq;
typedef _onvifMedia__GetVideoEncoderConfigurationOptionsResponse VideoOptionsResp;
typedef _onvifMedia__GetVideoSourceConfigurations VideoSrcConfigsReq;
typedef _onvifMedia__GetVideoSourceConfigurationsResponse VideoSrcConfigsResp;
typedef _onvifMedia__GetVideoEncoderConfigurations VideoConfigsReq;
typedef _onvifMedia__GetVideoEncoderConfigurationsResponse VideoConfigsResp;
typedef _onvifMedia__GetProfiles ProfilesReq;
typedef _onvifMedia__GetProfilesResponse ProfilesResp;
typedef _onvifMedia__AddVideoSourceConfiguration AddVideoSrcConfigReq;
typedef _onvifMedia__AddVideoSourceConfigurationResponse AddVideoSrcConfigResp;
typedef _onvifMedia__CreateProfile CreateProfileReq;
typedef _onvifMedia__CreateProfileResponse CreateProfileResp;
typedef _onvifMedia__AddVideoEncoderConfiguration AddVideoConfigReq;
typedef _onvifMedia__AddVideoEncoderConfigurationResponse AddVideoConfigResp;
typedef _onvifMedia__SetVideoEncoderConfiguration SetVideoConfigReq;
typedef _onvifMedia__SetVideoEncoderConfigurationResponse SetVideoConfigResp;
typedef _onvifMedia__GetProfile ProfileReq;
typedef _onvifMedia__GetProfileResponse ProfileResp;
typedef _onvifMedia__GetStreamUri StreamUriReq;
typedef _onvifMedia__GetStreamUriResponse StreamUriResp;

//
// SoapWrapper
//

template <class T>
class SoapWrapper
{
    char* m_login;
    char* m_passwd;
    bool invoked;

protected:

    T* m_soapProxy;
    char* m_endpoint;

public:

    SoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd);
    virtual ~SoapWrapper();

    soap* getSoap();
    const char* getLogin();
    const char* getPassword();
    QString getLastError();
    bool isNotAuthenticated();

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

    DeviceSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd);
    virtual ~DeviceSoapWrapper();

    //Input: normalized manufacturer
    bool fetchLoginPassword(const QString& manufacturer);

    int getNetworkInterfaces(NetIfacesReq& request, NetIfacesResp& response);
    int createUsers(CreateUsersReq& request, CreateUsersResp& response);
    int getDeviceInformation(DeviceInfoReq& request, DeviceInfoResp& response);
    int getCapabilities(CapabilitiesReq& request, CapabilitiesResp& response);

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

    MediaSoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd);
    virtual ~MediaSoapWrapper();

    int getVideoEncoderConfigurationOptions(VideoOptionsReq& request, VideoOptionsResp& response);
    int getVideoSourceConfigurations(VideoSrcConfigsReq& request, VideoSrcConfigsResp& response);
    int getVideoEncoderConfigurations(VideoConfigsReq& request, VideoConfigsResp& response);
    int getProfiles(ProfilesReq& request, ProfilesResp& response);
    int addVideoSourceConfiguration(AddVideoSrcConfigReq& request, AddVideoSrcConfigResp& response);
    int createProfile(CreateProfileReq& request, CreateProfileResp& response);
    int addVideoEncoderConfiguration(AddVideoConfigReq& request, AddVideoConfigResp& response);
    int setVideoEncoderConfiguration(SetVideoConfigReq& request, SetVideoConfigResp& response);
    int getProfile(ProfileReq& request, ProfileResp& response);
    int getStreamUri(StreamUriReq& request, StreamUriResp& response);

private:
    MediaSoapWrapper();
    MediaSoapWrapper(const MediaSoapWrapper&);
};

#endif //onvif_soap_wrapper_h
