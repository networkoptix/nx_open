#pragma once

#include <vector>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

class ILedPlatformAbstraction
{
public:
    virtual ~ILedPlatformAbstraction() = default;

    virtual std::vector<LedDescription> ledDescriptions() const = 0;

    virtual bool setLedState(const QString& ledId, LedState state) = 0;
};

} // namespace nx::vms::server::nvr::hanwha
