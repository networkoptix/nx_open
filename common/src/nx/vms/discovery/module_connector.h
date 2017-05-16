#pragma once

#include <network/module_information.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace vms {
namespace discovery {

/**
 * Verifies module access posibility by maintaining stream connection to first avaliable address.
 */
class ModuleConnector:
    public network::aio::BasicPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(
        QnModuleInformation, SocketAddress /*endpoint*/, HostAddress /*ip*/)> ConnectedHandler;
    typedef nx::utils::MoveOnlyFunc<void(QnUuid)> DisconnectedHandler;

    ModuleConnector(network::aio::AbstractAioThread* thread = nullptr);
    void setReconnectInterval(std::chrono::milliseconds interval);
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

    private:
        enum Priority { kDefault, kLocalHost, kLocalNetwork, kIp, kOther };
        typedef std::map<Priority, std::set<SocketAddress>> Endpoints;

        void connect(Endpoints::iterator endpointsGroup);
        void connect(const SocketAddress& endpoint, Endpoints::iterator endpointsGroup);
        boost::optional<QnModuleInformation> getInformation(nx_http::AsyncHttpClientPtr client);
        bool saveConnection(SocketAddress endpoint, nx_http::AsyncHttpClientPtr client,
            const QnModuleInformation& information);

    private:
        ModuleConnector* const m_parent;
        const QnUuid m_id;
        Endpoints m_endpoints;
        std::set<SocketAddress> m_forbiddenEndpoints;
        network::aio::Timer m_timer;
        std::set<nx_http::AsyncHttpClientPtr> m_httpClients;
        std::unique_ptr<AbstractStreamSocket> m_socket;
    };

    Module* getModule(const QnUuid& id);

private:
    bool m_isPassiveMode = true;
    std::chrono::milliseconds m_reconnectInterval;
    ConnectedHandler m_connectedHandler;
    DisconnectedHandler m_disconnectedHandler;
    std::map<QnUuid, std::unique_ptr<Module>> m_modules;
};

} // namespace discovery
} // namespace vms
} // namespace nx
