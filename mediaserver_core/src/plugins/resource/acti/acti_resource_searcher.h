#pragma once

#ifdef ENABLE_ACTI

#include "acti_system_info_checker.h"

#include <QtCore/QElapsedTimer>
#include <plugins/resource/upnp/upnp_resource_searcher.h>
#include <nx/network/mac_address.h>

class QnActiResourceSearcher:
    public QObject,
    public QnUpnpResourceSearcherAsync
{
    Q_OBJECT;
    using base_type = QnUpnpResourceSearcherAsync;
public:
    QnActiResourceSearcher(QnCommonModule* commonModule);
    virtual ~QnActiResourceSearcher();

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params);

    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

    virtual QnResourceList findResources(void) override;

    static const QString kSystemInfoProductionIdParamName;
protected:
    virtual void processPacket(
        const QHostAddress& discoveryAddr,
        const SocketAddress& deviceEndpoint,
        const nx_upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result) override;

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
    QByteArray getDeviceXmlAsync(const nx::utils::Url &url);
    nx_upnp::DeviceInfo getDeviceInfoSync(const nx::utils::Url& url, bool* outStatus) const;

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

    void processDeviceXml(
        const QByteArray& foundDeviceDescription,
        const HostAddress& host,
        const HostAddress& sender,
        QnResourceList& result );

    boost::optional<QnActiResource::ActiSystemInfo> getActiSystemInfo(
        const QnActiResourcePtr& actiResource);

    QString retreiveModel(const QString& model, const QString& serialNumber) const;

private slots:
    void at_httpConnectionDone(nx_http::AsyncHttpClientPtr reply);

private:
    QnUuid m_resTypeId;
    QMap<QString, std::shared_ptr<QnActiSystemInfoChecker>> m_systemInfoCheckers;
};

#endif // #ifdef ENABLE_ACTI
