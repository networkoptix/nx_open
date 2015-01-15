#ifndef _ACTI_RESOURCE_SEARCHER_H__
#define _ACTI_RESOURCE_SEARCHER_H__

#ifdef ENABLE_ACTI

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtCore/QElapsedTimer>
#include "plugins/resource/upnp/upnp_resource_searcher.h"


class QnMacAddress;

class QnActiResourceSearcher : public QObject, public QnUpnpResourceSearcher
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
        const QString& host,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        const QAuthenticator &auth,
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

    QMap<QString, std::shared_ptr<nx_http::AsyncHttpClient> > m_httpInProgress;
    QMutex m_mutex;

    QByteArray getDeviceXml(const QUrl& url);

    void createResource(
        const UpnpDeviceInfo& devInfo,
        const QnMacAddress& mac,
        const QAuthenticator &auth,
        QnResourceList& result );

private slots:
    void at_httpConnectionDone(nx_http::AsyncHttpClientPtr reply);
};

#endif // #ifdef ENABLE_ACTI
#endif // _ACTI_RESOURCE_SEARCHER_H__
