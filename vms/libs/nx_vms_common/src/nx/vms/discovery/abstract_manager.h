// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/network/socket_common.h>
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

NX_VMS_COMMON_API QString toString(const nx::vms::discovery::ModuleEndpoint& module);

/**
 * Abstract interface for module discovery manager.
 * Provides common interface for Manager and test mocks.
 */
class NX_VMS_COMMON_API AbstractManager: public QObject
{
    Q_OBJECT

public:
    explicit AbstractManager(QObject* parent = nullptr): QObject(parent) {}
    virtual ~AbstractManager() override = default;

    virtual std::list<ModuleEndpoint> getAll() const = 0;

    /**
     * Connects to discovery signals and notifies about existing modules.
     * This is a template method to support member function pointers with different class types.
     */
    template<typename Ptr, typename Found, typename Changed, typename Lost>
    void onSignals(Ptr ptr, Found foundSlot, Changed changedSlot, Lost lostSlot) const
    {
        connect(this, &AbstractManager::found, ptr, foundSlot);
        connect(this, &AbstractManager::changed, ptr, changedSlot);
        connect(this, &AbstractManager::lost, ptr, lostSlot);

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
};

} // namespace nx::vms::discovery
