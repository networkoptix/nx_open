#pragma once

#include <chrono>

#include <nx/vms/server/nvr/i_startable.h>

#include <api/model/api_ioport_data.h>

namespace nx::vms::server::nvr {

class IIoController: public IStartable
{
public:
    using StateChangeHandler = std::function<void(const QnIOStateDataList& state)>;
public:
    virtual ~IIoController() = default;

    virtual void start() = 0;

    virtual QnIOPortDataList portDesriptiors() const = 0;

    virtual QnIOStateDataList setOutputPortStates(
        const QnIOStateDataList& portStates,
        std::chrono::milliseconds autoResetTimeout) = 0;

    virtual QnIOStateDataList portStates() const = 0;

    virtual QnUuid registerStateChangeHandler(StateChangeHandler handler) = 0;

    virtual void unregisterStateChangeHandler(QnUuid handlerId) = 0;

};

} // namespace nx::vms::server::nvr
