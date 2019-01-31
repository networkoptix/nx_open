#pragma once

#include <update/task/network_peer_task.h>
#include <core/resource/resource_fwd.h>

class QnRestartPeerTask: public QnNetworkPeerTask
{
    Q_OBJECT

public:
    enum ErrorCode
    {
        NoError = 0,
        RestartError
    };

    explicit QnRestartPeerTask(QObject* parent = nullptr);

protected:
    virtual void doStart() override;

private:
    void restartNextServer();
    void finishPeer();

private slots:

    void at_commandSent(int status, int handle);
    void at_resourceStatusChanged();
    void at_timeout();

private:
    QList<QnMediaServerResourcePtr> m_restartingServers;
    bool m_serverWasOffline = false;
    QTimer* const m_timer;
};
