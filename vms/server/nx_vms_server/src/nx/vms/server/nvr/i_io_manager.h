#pragma once

#include <chrono>

#include <api/model/api_ioport_data.h>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr {

class IIoManager
{
public:
    virtual ~IIoManager() = default;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual QnIOPortDataList portDesriptiors() const = 0;

    virtual bool setOutputPortState(
        const QString& portId,
        IoPortState state,
        std::chrono::milliseconds autoResetTimeout) = 0;

    virtual QnIOStateDataList portStates() const = 0;

    virtual HandlerId registerStateChangeHandler(IoStateChangeHandler handler) = 0;

    virtual void unregisterStateChangeHandler(HandlerId handlerId) = 0;

};

} // namespace nx::vms::server::nvr
