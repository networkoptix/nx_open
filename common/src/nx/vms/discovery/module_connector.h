#pragma once

#include <set>
#include <map>

#include <network/module_information.h>
#include <nx/network/deprecated/asynchttpclient.h>
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
        QnModuleInformation, nx::network::SocketAddress /*endpoint*/, nx::network::HostAddress /*ip*/)> ConnectedHandler;
    typedef nx::utils::MoveOnlyFunc<void(QnUuid)> DisconnectedHandler;

    ModuleConnector(network::aio::AbstractAioThread* thread = nullptr);

    void setDisconnectTimeout(std::chrono::milliseconds value);
    void setReconnectPolicy(network::RetryPolicy value);

    void setConnectHandler(ConnectedHandler handler);
    void setDisconnectHandler(DisconnectedHandler handler);

    void newEndpoints(std::set<nx::network::SocketAddress> endpoints, const QnUuid& id = QnUuid());
    void setForbiddenEndpoints(std::set<nx::network::SocketAddress> endpoints, const QnUuid& id);

    void activate();
    void deactivate();

protected:
    virtual void stopWhileInAioThread() override;

private:
    class InformationReader
    {
    public:
        InformationReader(const ModuleConnector* parent);
        ~InformationReader();

        void setHandler(std::function<void(boost::optional<QnModuleInformation>, QString)> handler);
        void start(const nx::network::SocketAddress& endpoint);
        nx::network::HostAddress ip() const { return m_socket->getForeignAddress().address; }

    private:
        void readUntilError();

        const ModuleConnector* const m_parent;
        nx::network::http::AsyncHttpClientPtr m_httpClient;
        nx::network::SocketAddress m_endpoint;
        nx::Buffer m_buffer;
        std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;
        std::function<void(boost::optional<QnModuleInformation>, QString)> m_handler;
        nx::utils::ObjectDestructionFlag m_destructionFlag;
    };

    class Module
    {
    public:
        Module(ModuleConnector* parent, const QnUuid& id);
        ~Module();

        void addEndpoints(std::set<nx::network::SocketAddress> endpoints);
        void ensureConnection();
        void setForbiddenEndpoints(std::set<nx::network::SocketAddress> endpoints);
        QString idForToStringFromPtr() const; //< Used by toString(const T*).

    private:
        enum Priority { kDefault, kOther, kLocalHost, kLocalNetwork, kIp, kCloud };
        typedef std::map<Priority, std::set<nx::network::SocketAddress>> Endpoints;

        Priority hostPriority(const nx::network::HostAddress& host) const;
        boost::optional<Endpoints::iterator> saveEndpoint(nx::network::SocketAddress endpoint);
        void connectToGroup(Endpoints::iterator endpointsGroup);
        void connectToEndpoint(const nx::network::SocketAddress& endpoint, Endpoints::iterator endpointsGroup);
        bool saveConnection(nx::network::SocketAddress endpoint, std::unique_ptr<InformationReader> reader,
            const QnModuleInformation& information);

    private:
        ModuleConnector* const m_parent;
        const QnUuid m_id;
        Endpoints m_endpoints;
        std::set<nx::network::SocketAddress> m_forbiddenEndpoints;
        network::RetryTimer m_reconnectTimer;
        std::list<std::unique_ptr<InformationReader>> m_attemptingReaders;
        std::unique_ptr<InformationReader> m_connectedReader;
        network::aio::Timer m_disconnectTimer;
    };

    Module* getModule(const QnUuid& id);

private:
    bool m_isPassiveMode = true;
    std::chrono::milliseconds m_disconnectTimeout;
    network::RetryPolicy m_retryPolicy;

    ConnectedHandler m_connectedHandler;
    DisconnectedHandler m_disconnectedHandler;
    std::map<QnUuid, std::unique_ptr<Module>> m_modules;
};

} // namespace discovery
} // namespace vms
} // namespace nx
