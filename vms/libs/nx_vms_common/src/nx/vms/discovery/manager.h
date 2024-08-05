// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/network/retry_timer.h>
#include <nx/network/socket_common.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/module_information.h>

namespace nx::vms::discovery {

struct NX_VMS_COMMON_API ModuleEndpoint: api::ModuleInformationWithAddresses
{
    nx::network::SocketAddress endpoint;

    ModuleEndpoint(
        api::ModuleInformationWithAddresses old = {},
        nx::network::SocketAddress endpoint = {});

    bool operator==(const ModuleEndpoint& rhs) const = default;
};

/**
 * Discovers VMS modules, notifies their module information and availability changes.
 *
 * - Searches server for endpoints by multicasts, resource pool and interface methods.
 * - Tries to maintain connection to each module and access up-to-date module information.
 * - Notifies about availability or module information changes (in server mode).
 */
class NX_VMS_COMMON_API Manager:
    public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    struct ServerModeInfo
    {
        nx::Uuid peerId;
        nx::Uuid auditId;
        bool multicastDiscoveryAllowed = true;
    };

    /**
     * Create client-mode manager, which only discovers media servers.
     */
    Manager(QObject* parent = nullptr);

    /**
     * Create server-mode manager, which discovers other servers and also broadcasts info about
     * itself if allowed.
     */
    Manager(ServerModeInfo serverModeInfo, QObject* parent = nullptr);

    virtual ~Manager() override;

    /**
     * Server-mode control, defining if multicasts can be sent.
     */
    void setMulticastDiscoveryAllowed(bool value);

    /** Sets moduleInformation to multicast, starts multicast process if not running yet. */
    void setMulticastInformation(const api::ModuleInformationWithAddresses& information);

    void setReconnectPolicy(nx::network::RetryPolicy value);
    void setUpdateInterfacesInterval(std::chrono::milliseconds value);
    void setMulticastInterval(std::chrono::milliseconds value);

    /**
     * Start Servers search and listen for the urls changes for all servers in the provided Resource
     * Pool.
     */
    void start(QnResourcePool* resourcePool);

    /**
     * Start checking provided urls only.
     */
    void startModuleConnectorOnly();

    void stop();

    std::list<ModuleEndpoint> getAll() const; //< All accessible modules.
    std::optional<nx::network::SocketAddress> getEndpoint(const nx::Uuid& id) const; //< Reachable endpoint.
    std::optional<ModuleEndpoint> getModule(const nx::Uuid& id) const;
    void forgetModule(const nx::Uuid& id);

    /**
     * Try to find module on the specified endpoint.
     *
     * @param endpoint Socket address to search for module information on.
     * @param expectedId If the specified address is added to module's address list and the module
     *     is already found on a different address, then this address will not be used until the
     *     address in use is disconnected. If expectedId is not specified, the endpoint will be
     *     pinged just once.
     */
    void checkEndpoint(nx::network::SocketAddress endpoint, nx::Uuid expectedId = nx::Uuid());
    void checkEndpoint(const nx::utils::Url &url, nx::Uuid expectedId = nx::Uuid());

    template<typename Ptr, typename Found, typename Changed, typename Lost>
    void onSignals(Ptr ptr, Found foundSlot, Changed changedSlot, Lost lostSlot)
    {
        connect(this, &Manager::found, ptr, foundSlot);
        connect(this, &Manager::changed, ptr, changedSlot);
        connect(this, &Manager::lost, ptr, lostSlot);

        for (const auto& module: getAll())
            (ptr->*foundSlot)(module);
    }

signals:
    /** New reachable module is found. */
    void found(nx::vms::discovery::ModuleEndpoint module);

    /** Module information or active endpoint of reachable module is changed. */
    void changed(nx::vms::discovery::ModuleEndpoint module);

    /** Module connection is lost and could not be restored. */
    void lost(nx::Uuid information);

    /** Found the module with the same UUID as we are. */
    void conflict(
        std::chrono::microseconds timestamp,
        const nx::vms::discovery::ModuleEndpoint& module);

private:
    void updateEndpoints(const QnMediaServerResourcePtr& server);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::discovery
