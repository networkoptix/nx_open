#ifndef DIRECT_MODULE_FINDER_H
#define DIRECT_MODULE_FINDER_H

#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtNetwork/QHostAddress>

#include <utils/common/id.h>
#include <utils/network/module_information.h>
#include <utils/network/full_network_address.h>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class QnDirectModuleFinder : public QObject {
    Q_OBJECT
public:
    explicit QnDirectModuleFinder(QObject *parent = 0);

    void setCompatibilityMode(bool compatibilityMode);

    void addUrl(const QUrl &url, const QUuid &id);
    void removeUrl(const QUrl &url, const QUuid &id);

    void addIgnoredModule(const QUrl &url, const QUuid &id);
    void removeIgnoredModule(const QUrl &url, const QUuid &id);

    void addIgnoredUrl(const QUrl &url);
    void removeIgnoredUrl(const QUrl &url);

    void checkUrl(const QUrl &url);

    void start();
    void stop();
    void pleaseStop();

    QList<QnModuleInformation> foundModules() const;
    QnModuleInformation moduleInformation(const QUuid &id) const;

    //! Urls for periodical check
    QMultiHash<QUrl, QUuid> urls() const;
    //! Modules to be ignored when found
    QMultiHash<QUrl, QUuid> ignoredModules() const;
    //! Urls which may appear in url check list but must be ignored
    QSet<QUrl> ignoredUrls() const;

signals:
    void moduleChanged(const QnModuleInformation &moduleInformation);
    void moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url);
    void moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url);

private:
    void enqueRequest(const QUrl &url);

private slots:
    void activateRequests();

    void at_reply_finished(QNetworkReply *reply);
    void at_periodicalCheckTimer_timeout();

private:
    QMultiHash<QUrl, QUuid> m_urls;
    QMultiHash<QUrl, QUuid> m_ignoredModules;
    QSet<QUrl> m_ignoredUrls;

    int m_maxConnections;
    QQueue<QUrl> m_requestQueue;
    QSet<QUrl> m_activeRequests;

    QHash<QUuid, QnModuleInformation> m_foundModules;
    QHash<QUrl, qint64> m_lastPingByUrl;
    QHash<QUrl, QUuid> m_moduleByUrl;

    bool m_compatibilityMode;

    QNetworkAccessManager *m_networkAccessManager;
    QTimer *m_periodicalCheckTimer;
};

#endif // DIRECT_MODULE_FINDER_H
