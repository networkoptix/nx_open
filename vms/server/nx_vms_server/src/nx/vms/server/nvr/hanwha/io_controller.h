#pragma once

#include <map>

#include <nx/vms/server/nvr/i_io_controller.h>

#include <nx/utils/thread/mutex.h>

namespace nx::vms::server::nvr::hanwha {

class IoController: public IIoController
{
public:
    virtual QnIOPortDataList portDesriptiors() const override;

    virtual QnIOStateDataList setOutputPortStates(
        const QnIOStateDataList& portStates,
        std::chrono::milliseconds autoResetTimeout) override;

    virtual QnIOStateDataList portStates() const override;

    virtual QnUuid registerStateChangeHandler(StateChangeHandler handler) override;

    virtual void unregisterStateChangeHandler(QnUuid handlerId) override;

private:
    mutable QnMutex m_handlerMutex;
    std::map<QnUuid, StateChangeHandler> m_handlers;
};

} // namespace nx::vms::server::nvr::hanwha
