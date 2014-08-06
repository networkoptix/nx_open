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

    void addUrl(const QUrl &url, const QnId &id);
    void removeUrl(const QUrl &url, const QnId &id);
    void addAddress(const QHostAddress &address, quint16 port, const QnId &id);
    void removeAddress(const QHostAddress &address, quint16 port, const QnId &id);

    void addIgnoredModule(const QUrl &url, const QnId &id);
    void removeIgnoredModule(const QUrl &url, const QnId &id);
    void addIgnoredModule(const QHostAddress &address, quint16 port, const QnId &id);
    void removeIgnoredModule(const QHostAddress &address, quint16 port, const QnId &id);

    void addIgnoredUrl(const QUrl &url);
    void removeIgnoredUrl(const QUrl &url);
    void addIgnoredAddress(const QHostAddress &address, quint16 port);
    void removeIgnoredAddress(const QHostAddress &address, quint16 port);

    void checkUrl(const QUrl &url);

    void start();
    void stop();

    QList<QnModuleInformation> foundModules() const;
    QnModuleInformation moduleInformation(const QnId &id) const;

    //! Urls for periodical check
    QMultiHash<QUrl, QnId> urls() const;
    //! Modules to be ignored when found
    QMultiHash<QUrl, QnId> ignoredModules() const;
    //! Urls which may appear in url check list but must be ignored
    QSet<QUrl> ignoredUrls() const;

signals:
    //!Emitted when new enterprise controller has been found
    void moduleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress, const QUrl &url);

    //!Emmited when previously found module did not respond to request in predefined timeout
    void moduleLost(const QnModuleInformation &moduleInformation);

private:
    void enqueRequest(const QUrl &url);

    void dropModule(const QnId &id, bool emitSignal = true);
    void dropModule(const QUrl &url, bool emitSignal = true);

private slots:
    void activateRequests();

    void at_reply_finished(QNetworkReply *reply);
    void at_periodicalCheckTimer_timeout();

private:
    QMultiHash<QUrl, QnId> m_urls;
    QMultiHash<QUrl, QnId> m_ignoredModules;
    QSet<QUrl> m_ignoredUrls;

    int m_maxConnections;
    QQueue<QUrl> m_requestQueue;
    QSet<QUrl> m_activeRequests;

    QHash<QnId, QnModuleInformation> m_foundModules;
    QHash<QnId, qint64> m_lastPingById;
    QHash<QUrl, QnId> m_moduleByUrl;

    bool m_compatibilityMode;

    QNetworkAccessManager *m_networkAccessManager;
    QTimer *m_periodicalCheckTimer;
};

#endif // DIRECT_MODULE_FINDER_H
