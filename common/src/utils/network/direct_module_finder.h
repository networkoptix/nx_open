#ifndef DIRECT_MODULE_FINDER_H
#define DIRECT_MODULE_FINDER_H

#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtNetwork/QHostAddress>

#include <utils/common/id.h>
#include <utils/network/module_information.h>

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
    void addAddress(const QHostAddress &address, quint16 port, const QUuid &id);
    void removeAddress(const QHostAddress &address, quint16 port, const QUuid &id);

    void addIgnoredModule(const QUrl &url, const QUuid &id);
    void removeIgnoredModule(const QUrl &url, const QUuid &id);
    void addIgnoredModule(const QHostAddress &address, quint16 port, const QUuid &id);
    void removeIgnoredModule(const QHostAddress &address, quint16 port, const QUuid &id);

    void addIgnoredUrl(const QUrl &url);
    void removeIgnoredUrl(const QUrl &url);
    void addIgnoredAddress(const QHostAddress &address, quint16 port);
    void removeIgnoredAddress(const QHostAddress &address, quint16 port);

    void checkUrl(const QUrl &url);

    void start();
    void stop();

    QList<QnModuleInformation> foundModules() const;
    QnModuleInformation moduleInformation(const QUuid &id) const;

    //! Urls for periodical check
    QMultiHash<QUrl, QUuid> urls() const;
    //! Modules to be ignored when found
    QMultiHash<QUrl, QUuid> ignoredModules() const;
    //! Urls which may appear in url check list but must be ignored
    QSet<QUrl> ignoredUrls() const;

signals:
    //!Emitted when new enterprise controller has been found
    void moduleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress, const QUrl &url);

    //!Emmited when previously found module did not respond to request in predefined timeout
    void moduleLost(const QnModuleInformation &moduleInformation);

private:
    void enqueRequest(const QUrl &url);

    void dropModule(const QUuid &id, bool emitSignal = true);
    void dropModule(const QUrl &url, bool emitSignal = true);

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
    QHash<QUuid, qint64> m_lastPingById;
    QHash<QUrl, QUuid> m_moduleByUrl;

    bool m_compatibilityMode;

    QNetworkAccessManager *m_networkAccessManager;
    QTimer *m_periodicalCheckTimer;
};

#endif // DIRECT_MODULE_FINDER_H
