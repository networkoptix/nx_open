#ifndef _ACTI_RESOURCE_SEARCHER_H__
#define _ACTI_RESOURCE_SEARCHER_H__

#ifdef ENABLE_ACTI

#include <QtCore/QElapsedTimer>
#include <plugins/resource/upnp/upnp_resource_searcher.h>
#include <nx/network/mac_address.h>

class QnActiResourceSearcher : public QObject, public QnUpnpResourceSearcherAsync
{
    Q_OBJECT

public:
    QnActiResourceSearcher();
    virtual ~QnActiResourceSearcher();

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params);

    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

    virtual QnResourceList findResources(void) override;

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

    nx_upnp::UpnpDeviceInfo parseDeviceXml(const QByteArray& rawData, bool* outStatus) const;
    QByteArray getDeviceXmlAsync(const QUrl& url);
    nx_upnp::UpnpDeviceInfo getDeviceInfoSync(const QUrl& url, bool* outStatus) const;

    bool isNxDevice(const nx_upnp::DeviceInfo& devInfo) const;

    QString chooseProperPhysicalId(const QString& serialNumber, const QString& macAddress);
    QnNetworkResourcePtr findExistingResource(const QString& serialNumber, const QString& macAddress);

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

private slots:
    void at_httpConnectionDone(nx_http::AsyncHttpClientPtr reply);

private:
    QnUuid m_resTypeId;
};

#endif // #ifdef ENABLE_ACTI
#endif // _ACTI_RESOURCE_SEARCHER_H__
