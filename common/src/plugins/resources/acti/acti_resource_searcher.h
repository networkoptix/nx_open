#ifndef _ACTI_RESOURCE_SEARCHER_H__
#define _ACTI_RESOURCE_SEARCHER_H__

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QElapsedTimer>
#include "plugins/resources/upnp/upnp_resource_searcher.h"


class QnActiResourceSearcher : public QObject, public QnUpnpResourceSearcher
{
    Q_OBJECT

    QnActiResourceSearcher();
public:
    static QnActiResourceSearcher& instance();
    
    virtual ~QnActiResourceSearcher();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

    virtual QnResourceList findResources(void) override;
protected:
    virtual void processPacket(
        const QHostAddress& discoveryAddr,
        const QString& host,
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

    QSet<QString >m_httpInProgress;
    QMutex m_mutex;

    QByteArray getDeviceXml(const QUrl& url);

private slots:
    void at_replyReceived(nx_http::AsyncHttpClient* reply);
    void at_httpConnectionDone(nx_http::AsyncHttpClient* reply);
};

#endif // _ACTI_RESOURCE_SEARCHER_H__
