#pragma once

#include <map>
#include <set>

#include <nx/vms/server/nvr/i_io_controller.h>

#include <nx/utils/thread/mutex.h>

namespace nx::vms::server::nvr::hanwha {

class IoStateFetcher;

class IoController: public IIoController
{
public:
    IoController();

    virtual ~IoController() override;

    virtual void start() override;

    virtual QnIOPortDataList portDesriptiors() const override;

    virtual QnIOStateDataList setOutputPortStates(
        const QnIOStateDataList& portStates,
        std::chrono::milliseconds autoResetTimeout) override;

    virtual QnIOStateDataList portStates() const override;

    virtual QnUuid registerStateChangeHandler(StateChangeHandler handler) override;

    virtual void unregisterStateChangeHandler(QnUuid handlerId) override;

private:
    void handleState(const std::set<QnIOStateData>& state);

private:
    mutable QnMutex m_mutex;
    mutable QnMutex m_handlerMutex;

    std::map<QnUuid, StateChangeHandler> m_handlers;
    std::unique_ptr<IoStateFetcher> m_stateFetcher;
    std::set<QnIOStateData> m_lastIoState;
};

} // namespace nx::vms::server::nvr::hanwha
