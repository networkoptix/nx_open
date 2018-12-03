#pragma once
#if defined(ENABLE_ONVIF)

#include <set>

#include <QtCore/QCoreApplication>

#include "core/resource_management/resource_searcher.h"
#include "onvif_helper.h"
#include "soap_wrapper.h"
#include "onvif_resource.h"
#include <nx/vms/server/server_module_aware.h>

struct EndpointAdditionalInfo
{
    QString name;
    QString manufacturer;
    QString location;
    QString mac;
    QString uniqId;
    QString discoveryIp;
    std::set<QString> additionalManufacturers;

    QString defaultLogin;
    QString defaultPassword;

    EndpointAdditionalInfo() = default;
    EndpointAdditionalInfo(const EndpointAdditionalInfo& src) = default;

    EndpointAdditionalInfo(
        const QString& newName,
        const QString& newManufacturer,
        const QString& newLocation,
        const QString& newMac,
        const QString& newUniqId,
        const QString& newDiscoveryIp,
        const std::set<QString> additionalManufacturers = std::set<QString>())
        :
        name(newName),
        manufacturer(newManufacturer),
        location(newLocation),
        mac(newMac),
        uniqId(newUniqId),
        discoveryIp(newDiscoveryIp),
        additionalManufacturers(additionalManufacturers)
    {
    }
};

class EndpointInfoHookChain
{
public:
    void registerHook(std::function<void(QnResourceDataPool*, EndpointAdditionalInfo*)> hook)
    {
        m_hookChain.push_back(hook);
    };
    void applyHooks(QnResourceDataPool* dataPool, EndpointAdditionalInfo* outInfo) const
    {
        for (const auto& hook: m_hookChain)
        {
            hook(dataPool, outInfo);
        }
    };

private:
    std::vector<std::function<void(QnResourceDataPool*, EndpointAdditionalInfo*)>> m_hookChain;
};

typedef QHash<QString, EndpointAdditionalInfo> EndpointInfoHash;

class OnvifResourceInformationFetcher: public nx::vms::server::ServerModuleAware
{
    Q_DECLARE_TR_FUNCTIONS(OnvifResourceInformationFetcher)
public:
    OnvifResourceInformationFetcher(QnMediaServerModule* serverModule);

    void findResources(const EndpointInfoHash& endpointInfo, QnResourceList& result, DiscoveryMode discoveryMode) const;
    void findResources(const QString& endpoint, const EndpointAdditionalInfo& info, QnResourceList& result, DiscoveryMode discoveryMode) const;
    static QnPlOnvifResourcePtr createOnvifResourceByManufacture(
        QnMediaServerModule* serverModule,
        const QString& manufacture);
    QnUuid getOnvifResourceType(const QString& manufacturer, const QString&  model) const;

    void pleaseStop();
    static bool ignoreCamera(
        QnResourceDataPool* dataPool,
        const QString& manufacturer, const QString& name);
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
        QnResourceDataPool* dataPool,
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
