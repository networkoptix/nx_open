#pragma once

#ifdef ENABLE_ACTI

#include "acti_system_info_checker.h"

#include <QtCore/QElapsedTimer>
#include <plugins/resource/upnp/upnp_resource_searcher.h>
#include <nx/network/mac_address.h>

class QnActiResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public nx_upnp::SearchAutoHandler
{
    using base_type = nx_upnp::SearchAutoHandler;
public:
    QnActiResourceSearcher(QnCommonModule* commonModule);
    virtual ~QnActiResourceSearcher();

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params);

    virtual QString manufacture() const;

    virtual QnResourceList findResources() override;
    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

    static const QString kSystemInfoProductionIdParamName;
protected:
    virtual bool processPacket(
        const QHostAddress& discoveryAddr,
        const nx::network::SocketAddress& deviceEndpoint,
        const nx::network::upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo) override;


private:
    struct CacheInfo
    {
        QElapsedTimer timer;
        QByteArray xml;
    };

    struct CachedDevInfo
    {
        QElapsedTimer timer;
        nx::network::upnp::DeviceInfo info;
        nx::network::QnMacAddress mac;
    };

    QMap<QString, CacheInfo> m_cachedXml;
    QMap<QString, CachedDevInfo> m_cachedDevInfo;

    QMap<QString, nx::network::http::AsyncHttpClientPtr > m_httpInProgress;
    QnMutex m_mutex;

    nx::network::upnp::DeviceInfo parseDeviceXml(const QByteArray& rawData, bool* outStatus) const;
    nx::network::upnp::DeviceInfo getDeviceInfoSync(const nx::utils::Url& url, bool* outStatus) const;

    bool isNxDevice(const nx::network::upnp::DeviceInfo& devInfo) const;

    QString chooseProperPhysicalId(
        const QString& hostAddress,
        const QString& serialNumber,
        const QString& macAddress);
    QnNetworkResourcePtr findExistingResource(
        const QString& hostAddress,
        const QString& serialNumber,
        const QString& macAddress);

    void createResource(
        const nx::network::upnp::DeviceInfo& devInfo,
        const nx::network::QnMacAddress& mac,
        const QAuthenticator &auth,
        QnResourceList& result );

    boost::optional<QnActiResource::ActiSystemInfo> getActiSystemInfo(
        const QnActiResourcePtr& actiResource);

    QString retreiveModel(const QString& model, const QString& serialNumber) const;

private:
    QnUuid m_resTypeId;
    QMap<QString, std::shared_ptr<QnActiSystemInfoChecker>> m_systemInfoCheckers;
    QnResourceList m_foundUpnpResources;
    QSet<QString> m_alreadFoundMacAddresses;
};

#endif // #ifdef ENABLE_ACTI
