#ifndef CONFIGURE_PEER_TASK_H
#define CONFIGURE_PEER_TASK_H

#include <utils/network_peer_task.h>

class QnConfigurePeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
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
    void processReply(int status, int handle);

private:
    bool m_wholeSystem;
    QString m_systemName;
    int m_port;
    QString m_password;
    QByteArray m_passwordHash;
    QByteArray m_passwordDigest;

    QHash<int, QnId> m_pendingPeers;
    QSet<QnId> m_failedPeers;
};

#endif // CONFIGURE_PEER_TASK_H
