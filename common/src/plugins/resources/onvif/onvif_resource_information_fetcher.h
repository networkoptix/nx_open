#ifndef onvif_resource_information_fetcher_h
#define onvif_resource_information_fetcher_h

#include "core/resource_managment/resource_searcher.h"
#include "onvif_helper.h"
#include "soap_wrapper.h"
#include "onvif_resource.h"

struct EndpointAdditionalInfo
{
    QString name;
    QString manufacturer;
    QString mac;
    QString uniqId;
    QString discoveryIp;

    QString defaultLogin;
    QString defaultPassword;

    EndpointAdditionalInfo() {}

    EndpointAdditionalInfo(const QString& newName, const QString& newManufacturer, const QString& newMac, 
            const QString& newUniqId, const QString& newDiscoveryIp):
        name(newName),
        manufacturer(newManufacturer),
        mac(newMac),
        uniqId(newUniqId),
        discoveryIp(newDiscoveryIp)
    {

    }

    EndpointAdditionalInfo(const EndpointAdditionalInfo& src) :
        name(src.name),
        manufacturer(src.manufacturer),
        mac(src.mac),
        uniqId(src.uniqId),
        discoveryIp(src.discoveryIp)
    {

    }
};

typedef QHash<QString, EndpointAdditionalInfo> EndpointInfoHash;

class OnvifResourceInformationFetcher
{
    Q_DECLARE_TR_FUNCTIONS(OnvifResourceInformationFetcher)
public:
    OnvifResourceInformationFetcher();

    static OnvifResourceInformationFetcher& instance();

    void findResources(const EndpointInfoHash& endpointInfo, QnResourceList& result) const;
    void findResources(const QString& endpoint, const EndpointAdditionalInfo& info, QnResourceList& result) const;
    static QnPlOnvifResourcePtr createOnvifResourceByManufacture (const QString& manufacture);

    void pleaseStop();
private:


    bool findSpecialResource(const EndpointAdditionalInfo& info, const QHostAddress& sender, const QString& manufacturer, QnResourceList& result) const;

    QnPlOnvifResourcePtr createResource(const QString& manufacturer, const QString& firmware, const QHostAddress& sender, const QHostAddress& discoveryIp, const QString& model, const QString& mac,
        const QString& uniqId, const QString& login, const QString& passwd, const QString& deviceUrl) const;

    bool isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const;
    QString fetchSerial(const DeviceInfoResp& response) const;
    static bool isAnalogOnvifResource(const QString& vendor, const QString& model);
    static bool isModelContainVendor(const QString& vendor, const QString& model);
private:
    static const char *ONVIF_RT;
    QnId onvifTypeId;
    QnId onvifAnalogTypeId;
    //PasswordHelper& passwordsData;
    NameHelper &camersNamesData;
    bool m_shouldStop;
};

#endif // onvif_resource_information_fetcher_h
