#pragma once

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

class INetworkBlockPlatformAbstraction
{
public:
    virtual ~INetworkBlockPlatformAbstraction() = default;

    virtual int portCount() const = 0;

    virtual NetworkPortState portState(int portNumber) const = 0;

    virtual bool setPoeEnabled(int portNumber, bool isPoeEnabled) = 0;
};

} // namespace nx::vms::server::nvr::hanwha
