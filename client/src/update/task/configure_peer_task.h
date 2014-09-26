#ifndef CONFIGURE_PEER_TASK_H
#define CONFIGURE_PEER_TASK_H

#include <update/task/network_peer_task.h>

struct QnConfigureReply;

class QnConfigurePeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    enum ErrorCode {
        NoError = 0,
        AuthentificationFailed,
        UnknownError
    };

    explicit QnConfigurePeerTask(QObject *parent = 0);

    QString systemName() const;
    void setSystemName(const QString &systemName);

    int port() const;
    void setPort(int port);

    QString password() const;
    void setPassword(const QString &password);

    QByteArray passwordHash() const;
    QByteArray passwordDigest() const;
    void setPasswordHash(const QByteArray &hash, const QByteArray &digest);

    bool wholeSystem() const;
    void setWholeSystem(bool wholeSystem);

protected:
    virtual void doStart() override;

private slots:
    void processReply(int status, const QnConfigureReply &reply, int handle);

private:
    bool m_wholeSystem;
    QString m_systemName;
    int m_port;
    QString m_password;
    QByteArray m_passwordHash;
    QByteArray m_passwordDigest;
    int m_error;

    QHash<int, QnUuid> m_pendingPeers;
    QSet<QnUuid> m_failedPeers;
};

#endif // CONFIGURE_PEER_TASK_H
