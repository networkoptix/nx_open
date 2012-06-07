#ifndef onvif_resource_information_fetcher_h
#define onvif_resource_information_fetcher_h

#include "core/resourcemanagment/resource_searcher.h"
#include "onvif_helper.h"

class _onvifDevice__GetNetworkInterfacesResponse;
class _onvifDevice__GetDeviceInformationResponse;
class SOAP_ENV__Fault;

struct EndpointAdditionalInfo
{
    QByteArray manufacture;
    QString discoveryIp;

    EndpointAdditionalInfo(const QString& newManufacture, const QString& newDiscoveryIp):
        manufacture(newManufacture.toStdString().c_str()),
        discoveryIp(newDiscoveryIp)
    {

    }
};

typedef QHash<QString, EndpointAdditionalInfo> EndpointInfoHash;

class OnvifResourceInformationFetcher
{
    static const char* ONVIF_RT;
    QnId onvifTypeId;
    PasswordHelper passHelper;

public:

    static OnvifResourceInformationFetcher& instance();

    void findResources(const EndpointInfoHash& endpointInfo, QnResourceList& result) const;

private:

    OnvifResourceInformationFetcher();
    void findResources(const QString& endpoint, const EndpointAdditionalInfo& info, QnResourceList& result) const;
    const QString fetchMacAddress(const _onvifDevice__GetNetworkInterfacesResponse& response, const QString& senderIpAddress) const;
    const bool isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const;
    const QString fetchName(const _onvifDevice__GetDeviceInformationResponse& response) const;
    const QString fetchSerialConvertToMac(const _onvifDevice__GetDeviceInformationResponse& response) const;
    const QString generateRandomPassword() const;
    QHostAddress hostAddressFromEndpoint(const QString& endpoint) const;
};

#endif // onvif_resource_information_fetcher_h
