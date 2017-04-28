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
    typedef nx::utils::MoveOnlyFunc<void(QnModuleInformation, SocketAddress)> ConnectedHandler;
    typedef nx::utils::MoveOnlyFunc<void(QnUuid)> DisconnectedHandler;

    ModuleConnector(network::aio::AbstractAioThread* thread = nullptr);
    void setConnectHandler(ConnectedHandler handler);
    void setDisconnectHandler(DisconnectedHandler handler);

    void newEndpoint(const SocketAddress& endpoint, const QnUuid& id = QnUuid());
    void ignoreEndpoints(std::set<SocketAddress> endpoints, const QnUuid& id);

    void activate();
    void diactivate();

protected:
    virtual void stopWhileInAioThread() override;

private:
    class Module
    {
    public:
        Module(ModuleConnector* parent, const QnUuid& id);
        ~Module();

        void addEndpoint(const SocketAddress& endpoint);
        void ensureConnect();
        void forbidEndpoints(std::set<SocketAddress> endpoints);

    private:
        enum Priority { kLocal, kIp, kOther };
        typedef std::map<Priority, std::set<SocketAddress>> Endpoints;

        void connect(Endpoints::iterator endpointsGroup);
        boost::optional<QnModuleInformation> getInformation(nx_http::AsyncHttpClientPtr client);
        void saveConnection(SocketAddress endpoint, nx_http::AsyncHttpClientPtr client,
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
    ConnectedHandler m_connectedHandler;
    DisconnectedHandler m_disconnectedHandler;
    std::map<QnUuid, std::unique_ptr<Module>> m_modules;
};

} // namespace discovery
} // namespace vms
} // namespace nx
