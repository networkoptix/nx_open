#ifndef onvif_resource_searcher_mdns_h
#define onvif_resource_searcher_mdns_h

//#include "core/resource_management/resource_searcher.h"
//#include "../onvif/onvif_device_searcher.h"
#include "onvif_resource.h"
#include "onvif_resource_information_fetcher.h"
//#include "onvif_special_resource.h"

class _onvifDevice__GetNetworkInterfacesResponse;
class _onvifDevice__GetDeviceInformationResponse;
class SOAP_ENV__Fault;

class OnvifResourceSearcherMdns
{/*
    OnvifResourceInformationFetcher& onvifFetcher;

public:
    static OnvifResourceSearcherMdns& instance();

    void findResources(QnResourceList& result, const OnvifSpecialResourceCreatorPtr& creator) const;

private:

    OnvifResourceSearcherMdns();
    void checkSocket(QUdpSocket& sock, QHostAddress localAddress, EndpointInfoHash& endpointInfo) const;
    const QString fetchMacAddress(const _onvifDevice__GetNetworkInterfacesResponse& response, const QString& senderIpAddress) const;
    const bool isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const;
    const QString fetchName(const _onvifDevice__GetDeviceInformationResponse& response) const;
    const QString fetchSerialConvertToMac(const _onvifDevice__GetDeviceInformationResponse& response) const;
    const QString generateRandomPassword() const;
*/};

#endif // onvif_resource_searcher_mdns_h
