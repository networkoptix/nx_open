#pragma once
#if defined(ENABLE_ONVIF)

#include <QtCore/QCoreApplication>

#include "core/resource_management/resource_searcher.h"
#include "onvif_helper.h"
#include "soap_wrapper.h"
#include "onvif_resource.h"

struct EndpointAdditionalInfo
{
    QString name;
    QString manufacturer;
    QString location;
    QString mac;
    QString uniqId;
    QString discoveryIp;

    QString defaultLogin;
    QString defaultPassword;

    EndpointAdditionalInfo() {}

    EndpointAdditionalInfo(
        const QString& newName,
        const QString& newManufacturer,
        const QString& newLocation,
        const QString& newMac,
        const QString& newUniqId,
        const QString& newDiscoveryIp):

        name(newName),
        manufacturer(newManufacturer),
        location(newLocation),
        mac(newMac),
        uniqId(newUniqId),
        discoveryIp(newDiscoveryIp)
    {

    }

    EndpointAdditionalInfo(const EndpointAdditionalInfo& src) :
        name(src.name),
        manufacturer(src.manufacturer),
        location(src.location),
        mac(src.mac),
        uniqId(src.uniqId),
        discoveryIp(src.discoveryIp)
    {

    }
};

class EndpointInfoHookChain
{
public:
    void registerHook(std::function<void(EndpointAdditionalInfo*)> hook)
    {
        m_hookChain.push_back(hook);
    };
    void applyHooks(EndpointAdditionalInfo* outInfo) const
    {
        for (const auto& hook: m_hookChain)
        {
            hook(outInfo);
        }
    };

private:
    std::vector<std::function<void(EndpointAdditionalInfo*)>> m_hookChain;
};

typedef QHash<QString, EndpointAdditionalInfo> EndpointInfoHash;

class OnvifResourceInformationFetcher: public QnCommonModuleAware
{
    Q_DECLARE_TR_FUNCTIONS(OnvifResourceInformationFetcher)
public:
    OnvifResourceInformationFetcher(QnCommonModule* commonModule);

    void findResources(const EndpointInfoHash& endpointInfo, QnResourceList& result, DiscoveryMode discoveryMode) const;
    void findResources(const QString& endpoint, const EndpointAdditionalInfo& info, QnResourceList& result, DiscoveryMode discoveryMode) const;
    static QnPlOnvifResourcePtr createOnvifResourceByManufacture (const QString& manufacture);
    QnUuid getOnvifResourceType(const QString& manufacturer, const QString&  model) const;

    void pleaseStop();
    static bool ignoreCamera(const QString& manufacturer, const QString& name);
    static bool isModelSupported(const QString& manufacturer, const QString& modelNamee);
private:


    bool findSpecialResource(const EndpointAdditionalInfo& info, const QHostAddress& sender, const QString& manufacturer, QnResourceList& result) const;

    QnPlOnvifResourcePtr createResource(const QString& manufacturer, const QString& firmware, const QHostAddress& sender, const QHostAddress& discoveryIp, const QString& model, const QString& mac,
        const QString& uniqId, const QString& login, const QString& passwd, const QString& deviceUrl) const;

    bool isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const;
    QString fetchSerial(const DeviceInfoResp& response) const;
    static bool isAnalogOnvifResource(const QString& vendor, const QString& model);
    static bool isModelContainVendor(const QString& vendor, const QString& model);

    static bool needIgnoreCamera(
        const QString& host,
        const QString& manufacturer,
        const QString& model);
private:
    static const char *ONVIF_RT;
    QnUuid onvifTypeId;
    QnUuid onvifAnalogTypeId;
    //PasswordHelper& passwordsData;
    NameHelper &camersNamesData;
    bool m_shouldStop;
    EndpointInfoHookChain m_hookChain;
};

#endif // defined(ENABLE_ONVIF)
