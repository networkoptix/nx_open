#ifndef onvif_resource_information_fetcher_h
#define onvif_resource_information_fetcher_h

#include "core/resourcemanagment/resource_searcher.h"
#include "onvif_helper.h"
#include "onvif_special_resource.h"

class _onvifDevice__GetNetworkInterfacesResponse;
class _onvifDevice__GetDeviceInformationResponse;
class SOAP_ENV__Fault;

struct EndpointAdditionalInfo
{
    enum Source {
        MDNS,
        WSDD
    };

    Source src;
    QByteArray data;
    QString discoveryIp;

    EndpointAdditionalInfo(Source newSrc, const QByteArray& newData, const QString& newDiscoveryIp):
        src(newSrc),
        data(newData),
        discoveryIp(newDiscoveryIp)
    {

    }
};

typedef QHash<QString, EndpointAdditionalInfo> EndpointInfoHash;

class OnvifResourceInformationFetcher
{
    static const char* ONVIF_RT;
    QnId onvifTypeId;
    PasswordHelper passwordsData;
    ManufacturerHelper manufacturersData;

public:

    static OnvifResourceInformationFetcher& instance();

    void findResources(const EndpointInfoHash& endpointInfo, QnResourceList& result,
        const OnvifSpecialResourceCreatorPtr& creator) const;

private:

    OnvifResourceInformationFetcher();

    void findResources(const QString& endpoint, const EndpointAdditionalInfo& info,
        const OnvifSpecialResourceCreatorPtr& creator, QnResourceList& result) const;

    bool findSpecialResource(const EndpointAdditionalInfo& info, const QHostAddress& sender, const QString& manufacturer,
        const OnvifSpecialResourceCreatorPtr& creator, QnResourceList& result) const;

    const QString fetchMacAddress(const _onvifDevice__GetNetworkInterfacesResponse& response,
        const QString& senderIpAddress) const;

    void createResource(const OnvifSpecialResourceCreatorPtr& creator, const QString& manufacturer, const QHostAddress& sender,
        const QHostAddress& discoveryIp, const QString& name, const QString& mac, const char* login, const char* passwd,
        const QString& mediaUrl, const QString& deviceUrl, QnResourceList& result) const;

    const bool isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const;
    const QString fetchName(const _onvifDevice__GetDeviceInformationResponse& response) const;
    const QString fetchManufacturer(const _onvifDevice__GetDeviceInformationResponse& response) const;
    const QString fetchSerialConvertToMac(const _onvifDevice__GetDeviceInformationResponse& response) const;
    const QString generateRandomPassword() const;
    QHostAddress hostAddressFromEndpoint(const QString& endpoint) const;
};

#endif // onvif_resource_information_fetcher_h
