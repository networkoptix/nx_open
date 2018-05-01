#pragma once

#include <QtCore/QSharedPointer>

#include <nx/network/abstract_socket.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/elapsed_timer.h>

namespace nx {
namespace mediaserver {

class ReverseConnectionManager
{
public:
    ReverseConnectionManager();


    bool registerProxyReceiverConnection(
        const QString& url, QSharedPointer<nx::network::AbstractStreamSocket> socket);

    typedef std::function<void(int count)> SocketRequest;

    /**
     * @return Tcp socket connected to specified media server.
     * @param guid Media server guid.
     * @param timeoutMs total timeout to establish reverse connection.
     */
    QSharedPointer<nx::network::AbstractStreamSocket> getProxySocket(
        const QString& guid, int timeoutMs, const SocketRequest& socketRequest);
private:
    void doPeriodicTasks();
private:
    struct AwaitProxyInfo
    {
        explicit AwaitProxyInfo(const QSharedPointer<nx::network::AbstractStreamSocket>& socket):
            socket(socket)
        {
            timer.restart();
        }

        QSharedPointer<nx::network::AbstractStreamSocket> socket;
        nx::utils::ElapsedTimer timer;
    };

    struct ServerProxyPool
    {
        ServerProxyPool() : requested(0) {}

        size_t requested;
        QList<AwaitProxyInfo> available;
        QElapsedTimer timer;
    };

    QnMutex m_proxyMutex;
    QMap<QString, ServerProxyPool> m_proxyPool;
    QnWaitCondition m_proxyCondition;
};

} // namespace mediaserver
} // namespace nx
