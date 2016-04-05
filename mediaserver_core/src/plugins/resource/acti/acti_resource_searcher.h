#ifndef _ACTI_RESOURCE_SEARCHER_H__
#define _ACTI_RESOURCE_SEARCHER_H__

#ifdef ENABLE_ACTI

#include <QtCore/QElapsedTimer>
#include "plugins/resource/upnp/upnp_resource_searcher.h"


class QnMacAddress;

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
        const HostAddress& host,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result) override;

private:
    struct CasheInfo
    {
        QElapsedTimer timer;
        QByteArray xml;
    };

    struct CashedDevInfo
    {
        QElapsedTimer timer;
        UpnpDeviceInfo info;
    };

    QMap<QString, CasheInfo> m_cachedXml;
    QMap<QString, CashedDevInfo> m_cashedDevInfo;

    QMap<QString, nx_http::AsyncHttpClientPtr > m_httpInProgress;
    QnMutex m_mutex;

    QByteArray getDeviceXml(const QUrl& url);

    bool isNxDevice(const UpnpDeviceInfo& devInfo) const;

    void createResource(
        const UpnpDeviceInfo& devInfo,
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
