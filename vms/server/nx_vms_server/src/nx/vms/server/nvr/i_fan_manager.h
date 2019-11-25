#pragma once

#include <functional>

#include <nx/utils/uuid.h>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr {

class IFanManager
{
public:
    using StateChangeHandler = std::function<void(FanState state)>;

public:
    virtual ~IFanManager() = default;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual FanState state() const = 0;

    virtual QnUuid registerStateChangeHandler(StateChangeHandler handler) = 0;

    virtual void unregisterStateChangeHandler(QnUuid handlerId) = 0;
};

} // namespace nx::vms::server::nvr
