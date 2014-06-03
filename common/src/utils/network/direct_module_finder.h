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

    void addAddress(const QHostAddress &address, quint16 port);
    void removeAddress(const QHostAddress &address, quint16 port);

    void addManualAddress(const QHostAddress &address, quint16 port);
    void removeManualAddress(const QHostAddress &address, quint16 port);

    void addIgnoredAddress(const QHostAddress &address, quint16 port);
    void removeIgnoredAddress(const QHostAddress &address, quint16 port);

    void addIgnoredModule(const QnId &id, const QHostAddress &address, quint16 port);
    void removeIgnoredModule(const QnId &id, const QHostAddress &address, quint16 port);

    void checkUrl(const QUrl &url);

    void start();
    void stop();

    QList<QnModuleInformation> foundModules() const;
    QnModuleInformation moduleInformation(const QnId &id) const;

    QSet<QUrl> autoAddresses() const;
    QSet<QUrl> ignoredAddresses() const;
    QSet<QUrl> manualAddresses() const;
    QMultiHash<QnId, QUrl> ignoredModules() const;

signals:
    //!Emitted when new enterprise controller has been found
    void moduleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress);

    //!Emmited when previously found module did not respond to request in predefined timeout
    void moduleLost(const QnModuleInformation &moduleInformation);

private:
    void enqueRequest(const QUrl &url);

private slots:
    void activateRequests();

    void at_reply_finished(QNetworkReply *reply);
    void at_manualCheckTimer_timeout();

private:
    QSet<QUrl> m_autoAddresses;
    QSet<QUrl> m_ignoredAddresses;
    QSet<QUrl> m_manualAddresses;
    QMultiHash<QnId, QUrl> m_ignoredModules;

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
