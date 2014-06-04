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
        NoError,
        Timeout,
        HostLookupError,
        VersionError,
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
    void updateDiscoveryInformation();
    void finish(int errorCode);

private slots:
    void at_resource_added(const QnResourcePtr &resource);
    void at_resource_statusChanged(const QnResourcePtr &resource);
    void at_timer_timeout();
    void at_hostLookedUp(const QHostInfo &hostInfo);
    void at_targetServer_systemNameChanged(int handle, int status);

private:
    bool m_running;

    QUrl m_targetUrl;
    QString m_password;
    QList<QHostAddress> m_possibleAddresses;
    QnMediaServerResourcePtr m_targetServer;

    QTimer *m_timer;
};

#endif // JOIN_SYSTEM_TOOL_H
