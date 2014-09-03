#ifndef JOIN_SYSTEM_TOOL_H
#define JOIN_SYSTEM_TOOL_H

#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>

class QTimer;
class QHostInfo;

class QnJoinSystemTool : public QObject {
    Q_OBJECT
public:
    enum ErrorCode {
        InternalError = -1,
        NoError,
        Timeout,
        HostLookupError,
        VersionError,
        AuthentificationError,
        JoinError
    };

    explicit QnJoinSystemTool(QObject *parent = 0);

    bool isRunning() const;

    void start(const QUrl &url, const QString &password);

signals:
    void finished(int errorCode);

private:
    void findResource();
    void joinResource();
    void rediscoverPeer();
    void updateDiscoveryInformation();
    void finish(int errorCode);

private slots:
    void at_resource_added(const QnResourcePtr &resource);
    void at_resource_statusChanged(const QnResourcePtr &resource);
    void at_timer_timeout();
    void at_hostLookedUp(const QHostInfo &hostInfo);
    void at_targetServer_configured(int status, int handle);

private:
    bool m_running;

    QUrl m_targetUrl;
    QUrl m_oldApiUrl;
    QString m_password;
    QSet<QHostAddress> m_possibleAddresses;
    QnMediaServerResourcePtr m_targetServer;
    QUuid m_targetId;

    QTimer *m_timer;
};

#endif // JOIN_SYSTEM_TOOL_H
