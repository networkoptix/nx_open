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
    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

    static const QString kSystemInfoProductionIdParamName;
protected:
    virtual bool processPacket(
        const QHostAddress& discoveryAddr,
        const SocketAddress& deviceEndpoint,
        const nx_upnp::DeviceInfo& devInfo,
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
        nx_upnp::DeviceInfo info;
        QnMacAddress mac;
    };

    QMap<QString, CacheInfo> m_cachedXml;
    QMap<QString, CachedDevInfo> m_cachedDevInfo;

    QMap<QString, nx_http::AsyncHttpClientPtr > m_httpInProgress;
    QnMutex m_mutex;

    nx_upnp::DeviceInfo parseDeviceXml(const QByteArray& rawData, bool* outStatus) const;
    nx_upnp::DeviceInfo getDeviceInfoSync(const QUrl& url, bool* outStatus) const;

    bool isNxDevice(const nx_upnp::DeviceInfo& devInfo) const;

    QString chooseProperPhysicalId(
        const QString& hostAddress,
        const QString& serialNumber,
        const QString& macAddress);
    QnNetworkResourcePtr findExistingResource(
        const QString& hostAddress,
        const QString& serialNumber,
        const QString& macAddress);

    void createResource(
        const nx_upnp::DeviceInfo& devInfo,
        const QnMacAddress& mac,
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
