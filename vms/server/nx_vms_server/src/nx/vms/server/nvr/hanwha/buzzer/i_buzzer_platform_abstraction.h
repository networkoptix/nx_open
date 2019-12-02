#pragma once

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

class IBuzzerPlatformAbstraction
{
public:
    virtual ~IBuzzerPlatformAbstraction() = default;

    virtual bool setState(BuzzerState state) = 0;
};

} // namespace nx::vms::server::nvr::hanwha
