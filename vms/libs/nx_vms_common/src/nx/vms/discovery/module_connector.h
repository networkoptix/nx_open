// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>
#include <set>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/retry_timer.h>
#include <nx/utils/move_only_func.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/string.h>

namespace nx {
namespace vms {
namespace discovery {

/**
 * Verifies module access possibility by maintaining stream connection to first available address.
 */
class NX_VMS_COMMON_API ModuleConnector: public nx::network::aio::BasicPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(
        api::ModuleInformationWithAddresses,
        nx::network::SocketAddress /*requestedEndpoint*/,
        nx::network::SocketAddress /*resolvedEndpoint*/
    )> ConnectedHandler;

    typedef nx::utils::MoveOnlyFunc<void(QnUuid)> DisconnectedHandler;
    typedef nx::utils::MoveOnlyFunc<void(api::ModuleInformation, api::ModuleInformation)> ConflictHandler;

    ModuleConnector(nx::network::aio::AbstractAioThread* thread = nullptr);

    void setDisconnectTimeout(std::chrono::milliseconds value);
    void setReconnectPolicy(nx::network::RetryPolicy value);

    void setConnectHandler(ConnectedHandler handler);
    void setDisconnectHandler(DisconnectedHandler handler);
    void setConflictHandler(ConflictHandler handler);

    void forgetModule(const QnUuid& id);
    void newEndpoints(std::set<nx::network::SocketAddress> endpoints, const QnUuid& id = QnUuid());
    void setForbiddenEndpoints(std::set<nx::network::SocketAddress> endpoints, const QnUuid& id);

    void activate();
    void deactivate();

    static bool isValidForConnect(const nx::network::SocketAddress& endpoint);
    static void validateEndpoints(std::set<nx::network::SocketAddress>* endpoints);

protected:
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

private:
    class InformationReader
    {
    public:
        InformationReader(const ModuleConnector* parent);
        ~InformationReader();

        void setHandler(
            std::function<void(std::optional<api::ModuleInformationWithAddresses>, QString)> handler);

        void start(const nx::network::SocketAddress& endpoint);
        nx::network::SocketAddress address() const { return m_socket->getForeignAddress(); }

    private:
        void readUntilError();

        const ModuleConnector* const m_parent;
        nx::network::http::AsyncHttpClientPtr m_httpClient;
        nx::network::SocketAddress m_endpoint;
        nx::Buffer m_buffer;
        std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;
        std::function<void(std::optional<api::ModuleInformationWithAddresses>, QString)> m_handler;
        nx::utils::InterruptionFlag m_destructionFlag;
    };

    class Module
    {
    public:
        Module(ModuleConnector* parent, const QnUuid& id);
        ~Module();

        void addEndpoints(std::set<nx::network::SocketAddress> endpoints);
        void ensureConnection();
        void remakeConnection();
        void setForbiddenEndpoints(std::set<nx::network::SocketAddress> endpoints);
        QString idForToStringFromPtr() const; //< Used by toString(const T*).

    private:
        enum Priority { kDefault, kDns, kLocalHost, kLocalIpV4, kLocalIpV6, kRemoteIp, kCloud };
        typedef std::map<Priority, std::set<nx::network::SocketAddress>> Endpoints;

        Priority hostPriority(const nx::network::HostAddress& host) const;
        std::optional<Endpoints::iterator> saveEndpoint(nx::network::SocketAddress endpoint);
        void connectToGroup(Endpoints::iterator endpointsGroup);
        void connectToEndpoint(const nx::network::SocketAddress& endpoint, Endpoints::iterator endpointsGroup);

        bool saveConnection(
            nx::network::SocketAddress endpoint,
            std::unique_ptr<InformationReader> reader,
            const api::ModuleInformationWithAddresses& information);

    private:
        ModuleConnector* const m_parent;
        const QnUuid m_id;
        Endpoints m_endpoints;
        std::set<nx::String> m_forbiddenEndpoints;
        nx::network::RetryTimer m_reconnectTimer;
        std::list<std::unique_ptr<InformationReader>> m_attemptingReaders;
        std::unique_ptr<InformationReader> m_connectedReader;
        nx::network::aio::Timer m_disconnectTimer;
    };

    Module* getModule(const QnUuid& id);

private:
    bool m_isPassiveMode = true;
    std::chrono::milliseconds m_disconnectTimeout;
    nx::network::RetryPolicy m_retryPolicy;

    ConnectedHandler m_connectedHandler;
    DisconnectedHandler m_disconnectedHandler;
    ConflictHandler m_conflictHandler;
    std::map<QnUuid, std::unique_ptr<Module>> m_modules;
};

} // namespace discovery
} // namespace vms
} // namespace nx
