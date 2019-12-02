#pragma once

#include <memory>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr {

class INetworkBlockManager;
class IIoManager;
class IBuzzerManager;
class IFanManager;
class ILedManager;

class IManagerFactory
{
public:
    virtual ~IManagerFactory() = default;

    virtual std::unique_ptr<INetworkBlockManager> createNetworkBlockManager() const = 0;

    virtual std::unique_ptr<IIoManager> createIoManager() const = 0;

    virtual std::unique_ptr<IBuzzerManager> createBuzzerManager() const = 0;

    virtual std::unique_ptr<IFanManager> createFanManager() const = 0;

    virtual std::unique_ptr<ILedManager> createLedManager() const = 0;
};

} // namespace nx::vms::server::nvr
