#pragma once

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

class IFanPlatformAbstraction
{
public:
    virtual ~IFanPlatformAbstraction() = default;

    virtual FanState state() const = 0;

    virtual void interrupt() = 0;
};

} // namespace nx::vms::server::nvr::hanwha
