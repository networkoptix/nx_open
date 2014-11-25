#ifndef DIRECT_MODULE_FINDER_H
#define DIRECT_MODULE_FINDER_H

#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtNetwork/QHostAddress>

#include <utils/network/module_information.h>
#include <utils/network/network_address.h>

class QTimer;
class QnAsyncHttpClientReply;

class QnDirectModuleFinder : public QObject {
    Q_OBJECT
public:
    explicit QnDirectModuleFinder(QObject *parent = 0);

    void setCompatibilityMode(bool compatibilityMode);
    bool isCompatibilityMode() const;

    void addUrl(const QUrl &url, const QnUuid &id);
    void removeUrl(const QUrl &url, const QnUuid &id);

    void addIgnoredModule(const QUrl &url, const QnUuid &id);
    void removeIgnoredModule(const QUrl &url, const QnUuid &id);

    void addIgnoredUrl(const QUrl &url);
    void removeIgnoredUrl(const QUrl &url);

    void checkUrl(const QUrl &url);

    void start();
    void stop();
    void pleaseStop();

    QList<QnModuleInformation> foundModules() const;
    QnModuleInformation moduleInformation(const QnUuid &id) const;

    //! Urls for periodical check
    QMultiHash<QUrl, QnUuid> urls() const;
    //! Modules to be ignored when found
    QMultiHash<QUrl, QnUuid> ignoredModules() const;
    //! Urls which may appear in url check list but must be ignored
    QSet<QUrl> ignoredUrls() const;

signals:
    void moduleChanged(const QnModuleInformation &moduleInformation);
    void moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url);
    void moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url);

private:
    void enqueRequest(const QUrl &url);
    void processFailedReply(const QUrl &url);

private slots:
    void activateRequests();

    void at_reply_finished(QnAsyncHttpClientReply *reply);
    void at_discoveryCheckTimer_timeout();
    void at_aliveCheckTimer_timeout();

private:
    QMultiHash<QUrl, QnUuid> m_urls;
    QMultiHash<QUrl, QnUuid> m_ignoredModules;
    QSet<QUrl> m_ignoredUrls;

    int m_maxConnections;
    QQueue<QUrl> m_requestQueue;
    QSet<QUrl> m_activeRequests;

    QHash<QnUuid, QnModuleInformation> m_foundModules;
    QHash<QUrl, qint64> m_lastPingByUrl;
    QHash<QUrl, QnUuid> m_moduleByUrl;

    bool m_compatibilityMode;

    QTimer *m_discoveryCheckTimer;
    QTimer *m_aliveCheckTimer;
};

#endif // DIRECT_MODULE_FINDER_H
