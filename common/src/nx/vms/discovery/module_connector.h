#pragma once

#include <network/module_information.h>
#include <nx/network/http/asynchttpclient.h>

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
    typedef utils::MoveOnlyFunc<void(QnModuleInformation, SocketAddress)> ConnectedHandler;
    typedef utils::MoveOnlyFunc<void(QnUuid)> DisconnectedHandler;

    ModuleConnector(ConnectedHandler connectedHandler, DisconnectedHandler disconnectedHandler);
    void newEndpoint(const QnUuid& uuid, const SocketAddress& endpoint);

protected:
    virtual void stopWhileInAioThread() override;

private:
    class Module
    {
    public:
        Module(ModuleConnector* parent, const QnUuid& uuid);
        ~Module();
        void addEndpoint(const SocketAddress& endpoint);

    private:
        enum Priority { kLocal, kIp, kOther };
        typedef std::map<Priority, std::set<SocketAddress>> Endpoints;

        void connect(Endpoints::iterator endpointsGroup);
        boost::optional<QnModuleInformation> getInformation(nx_http::AsyncHttpClientPtr client);
        void monitorConnection(nx_http::AsyncHttpClientPtr client);

    private:
        ModuleConnector* const m_parent;
        const QnUuid m_uuid;
        Endpoints m_endpoints;
        network::aio::Timer m_timer;
        std::set<nx_http::AsyncHttpClientPtr> m_httpClients;
        std::unique_ptr<AbstractStreamSocket> m_socket;
    };

private:
    const ConnectedHandler m_connectedHandler;
    const DisconnectedHandler m_disconnectedHandler;
    std::map<QnUuid, std::unique_ptr<Module>> m_modules;
};

} // namespace discovery
} // namespace vms
} // namespace nx
