#pragma once

#include <deque>

#include <QtCore/QSharedPointer>

#include <nx/network/abstract_socket.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/elapsed_timer.h>
#include <common/common_module_aware.h>

namespace nx {
namespace mediaserver {

class ReverseConnectionManager: public QnCommonModuleAware
{
public:
    ReverseConnectionManager(QnCommonModule* commonModule);

    bool registerProxyReceiverConnection(
        const QString& url, 
        std::unique_ptr<nx::network::AbstractStreamSocket> socket);

    /**
     * @return Tcp socket connected to specified media server.
     * @param guid Media server guid.
     * @param timeoutMs total timeout to establish reverse connection.
     */
    std::unique_ptr<nx::network::AbstractStreamSocket> getProxySocket(
        const QnUuid& guid, std::chrono::milliseconds timeout);
private:
    void doPeriodicTasks();
private:
    struct AwaitProxyInfo
    {
        AwaitProxyInfo(std::unique_ptr<nx::network::AbstractStreamSocket> socket):
            socket(std::move(socket))
        {
            timer.restart();
        }

        std::unique_ptr<nx::network::AbstractStreamSocket> socket;
        nx::utils::ElapsedTimer timer;
    };

    struct ServerProxyPool
    {
        ServerProxyPool() : requested(0) {}

        size_t requested;
        std::deque<AwaitProxyInfo> available;
        nx::utils::ElapsedTimer timer;
    };

    QnMutex m_proxyMutex;
    std::map<QString, ServerProxyPool> m_proxyPool;
    QnWaitCondition m_proxyCondition;
};

} // namespace mediaserver
} // namespace nx
