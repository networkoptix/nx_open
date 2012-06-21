#ifndef onvif_soap_wrapper_h
#define onvif_soap_wrapper_h

#include "onvif_helper.h"

struct soap;
class DeviceBindingProxy;
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

template <class T>
class SoapWrapper
{
    bool invoked;

protected:

    T* m_soapProxy;
    char* m_endpoint;
    char* m_login;
    char* m_passwd;

public:

    SoapWrapper(const std::string& endpoint, const std::string& login, const std::string& passwd);
    virtual ~SoapWrapper();

    soap* getSoap();
    const char* getLogin();
    const char* getPassword();
    QString getLastError();

private:
    SoapWrapper();
    SoapWrapper(const SoapWrapper<T>&);
    void cleanLoginPassword();

protected:
    void beforeMethodInvocation();
    void setLoginPassword(const std::string& login, const std::string& passwd);
};

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



#endif //onvif_soap_wrapper_h
