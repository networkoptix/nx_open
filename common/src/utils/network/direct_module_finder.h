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

    void addManualUrl(const QUrl &url, const QnId &id);
    void removeManualUrl(const QUrl &url, const QnId &id);
    void addManualAddress(const QHostAddress &address, quint16 port, const QnId &id);
    void removeManualAddress(const QHostAddress &address, quint16 port, const QnId &id);

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

    QMultiHash<QUrl, QnId> autoUrls() const;
    QMultiHash<QUrl, QnId> manualUrls() const;
    QMultiHash<QUrl, QnId> ignoredModules() const;
    QSet<QUrl> ignoredUrls() const;

signals:
    //!Emitted when new enterprise controller has been found
    void moduleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress);

    //!Emmited when previously found module did not respond to request in predefined timeout
    void moduleLost(const QnModuleInformation &moduleInformation);

private:
    void enqueRequest(const QUrl &url);

    void checkAndAddUrl(const QUrl &url, const QnId &id, QMultiHash<QUrl, QnId> *urls);
    void checkAndAddAddress(const QHostAddress &address, quint16 port, const QnId &id, QMultiHash<QUrl, QnId> *urls);

    QnIdSet expectedModuleIds(const QUrl &url) const;

private slots:
    void activateRequests();

    void at_reply_finished(QNetworkReply *reply);
    void at_manualCheckTimer_timeout();

private:
    QMultiHash<QUrl, QnId> m_autoUrls;
    QMultiHash<QUrl, QnId> m_manualUrls;
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
    QTimer *m_manualCheckTimer;
};

#endif // DIRECT_MODULE_FINDER_H
