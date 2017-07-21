#pragma once

#include <set>
#include <map>

#include <network/module_information.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/retry_timer.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace vms {
namespace discovery {

/**
 * Verifies module access possibility by maintaining stream connection to first available address.
 */
class ModuleConnector:
    public network::aio::BasicPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(
        QnModuleInformation, SocketAddress /*endpoint*/, HostAddress /*ip*/)> ConnectedHandler;
    typedef nx::utils::MoveOnlyFunc<void(QnUuid)> DisconnectedHandler;

    ModuleConnector(network::aio::AbstractAioThread* thread = nullptr);
    void setReconnectPolicy(network::RetryPolicy value);
    void setConnectHandler(ConnectedHandler handler);
    void setDisconnectHandler(DisconnectedHandler handler);

    void newEndpoints(std::set<SocketAddress> endpoints, const QnUuid& id = QnUuid());
    void setForbiddenEndpoints(std::set<SocketAddress> endpoints, const QnUuid& id);

    void activate();
    void deactivate();

protected:
    virtual void stopWhileInAioThread() override;

private:
    class Module
    {
    public:
        Module(ModuleConnector* parent, const QnUuid& id);
        ~Module();

        void addEndpoints(std::set<SocketAddress> endpoints);
        void ensureConnection();
        void setForbiddenEndpoints(std::set<SocketAddress> endpoints);
        QString idForToStringFromPtr() const;

    private:
        enum Priority { kDefault, kLocalHost, kLocalNetwork, kIp, kOther };
        typedef std::map<Priority, std::set<SocketAddress>> Endpoints;

        boost::optional<Endpoints::iterator> saveEndpoint(SocketAddress endpoint);
        void connectToGroup(Endpoints::iterator endpointsGroup);
        void connectToEndpoint(const SocketAddress& endpoint, Endpoints::iterator endpointsGroup);
        boost::optional<QnModuleInformation> getInformation(nx_http::AsyncHttpClientPtr client);
        bool saveConnection(SocketAddress endpoint, nx_http::AsyncHttpClientPtr client,
            const QnModuleInformation& information);

    private:
        ModuleConnector* const m_parent;
        const QnUuid m_id;
        Endpoints m_endpoints;
        std::set<SocketAddress> m_forbiddenEndpoints;
        network::RetryTimer m_timer;
        std::set<nx_http::AsyncHttpClientPtr> m_httpClients;
        std::unique_ptr<AbstractStreamSocket> m_socket;
        std::unique_ptr<network::aio::Timer> m_disconnectTimer;
    };

    Module* getModule(const QnUuid& id);

private:
    bool m_isPassiveMode = true;
    network::RetryPolicy m_retryPolicy;
    ConnectedHandler m_connectedHandler;
    DisconnectedHandler m_disconnectedHandler;
    std::map<QnUuid, std::unique_ptr<Module>> m_modules;
};

} // namespace discovery
} // namespace vms
} // namespace nx
