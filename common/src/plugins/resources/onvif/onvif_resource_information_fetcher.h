#ifndef onvif_resource_information_fetcher_h
#define onvif_resource_information_fetcher_h

#include "core/resourcemanagment/resource_searcher.h"
#include "onvif_helper.h"

class _onvifDevice__GetNetworkInterfacesResponse;
class _onvifDevice__GetDeviceInformationResponse;
struct SOAP_ENV__Fault;

struct EndpointAdditionalInfo
{
    QString name;
    QString manufacturer;
    QString mac;
    QString uniqId;
    QString discoveryIp;

    EndpointAdditionalInfo(const QString& newName, const QString& newManufacturer, const QString& newMac, 
            const QString& newUniqId, const QString& newDiscoveryIp):
        name(newName),
        manufacturer(newManufacturer),
        mac(newMac),
        uniqId(newUniqId),
        discoveryIp(newDiscoveryIp)
    {

    }
};

typedef QHash<QString, EndpointAdditionalInfo> EndpointInfoHash;

class OnvifResourceInformationFetcher
{
    static const char* ONVIF_RT;
	static std::string& STD_ONVIF_USER;
	static std::string& STD_ONVIF_PASSWORD;
    QnId onvifTypeId;
    PasswordHelper& passwordsData;
    NameHelper& camersNamesData;

public:

    static OnvifResourceInformationFetcher& instance();

    void findResources(const EndpointInfoHash& endpointInfo, QnResourceList& result) const;
    static QnNetworkResourcePtr createOnvifResourceByManufacture (const QString& manufacture);
private:

    OnvifResourceInformationFetcher();

    void findResources(const QString& endpoint, const EndpointAdditionalInfo& info, QnResourceList& result) const;

    bool findSpecialResource(const EndpointAdditionalInfo& info, const QHostAddress& sender, const QString& manufacturer, QnResourceList& result) const;

    void createResource(const QString& manufacturer, const QHostAddress& sender, const QHostAddress& discoveryIp, const QString& name, const QString& mac,
        const QString& uniqId, const char* login, const char* passwd, const QString& mediaUrl, const QString& deviceUrl, QnResourceList& result) const;

    const bool isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const;
    const QString fetchName(const _onvifDevice__GetDeviceInformationResponse& response) const;
    const QString fetchManufacturer(const _onvifDevice__GetDeviceInformationResponse& response) const;
    const QString fetchSerial(const _onvifDevice__GetDeviceInformationResponse& response) const;
    //const QString generateRandomPassword() const;

};

#endif // onvif_resource_information_fetcher_h
