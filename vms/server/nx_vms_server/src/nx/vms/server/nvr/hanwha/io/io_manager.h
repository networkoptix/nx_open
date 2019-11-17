#pragma once

#include <map>
#include <set>

#include <nx/utils/thread/mutex.h>

#include <nx/vms/server/nvr/i_io_manager.h>

namespace nx::network::aio { class Timer; }

namespace nx::vms::server::nvr::hanwha {

class IoStateFetcher;
class IoCommandExecutor;
class IIoPlatformAbstraction;

class IoManager: public IIoManager
{
public:
    IoManager(std::unique_ptr<IIoPlatformAbstraction> platformAbstraction);

    virtual ~IoManager() override;

    virtual void start() override;

    virtual QnIOPortDataList portDesriptiors() const override;

    virtual bool setOutputPortState(
        const QString& portId,
        IoPortState state,
        std::chrono::milliseconds autoResetTimeout) override;

    virtual QnIOStateDataList portStates() const override;

    virtual QnUuid registerStateChangeHandler(IoStateChangeHandler handler) override;

    virtual void unregisterStateChangeHandler(QnUuid handlerId) override;

private:
    void handleInputPortStates(const std::set<QnIOStateData>& inputPortStates);
    void handleOutputPortStates(const QnIOStateDataList& outputPortStates);

    IoPortState calculateOutputPortState(const QString& portId) const;

private:
    mutable nx::utils::Mutex m_mutex;
    mutable nx::utils::Mutex m_handlerMutex;

    std::unique_ptr<IIoPlatformAbstraction> m_platformAbstraction;
    std::unique_ptr<IoStateFetcher> m_stateFetcher;
    std::unique_ptr<IoCommandExecutor> m_commandExecutor;
    std::unique_ptr<nx::network::aio::Timer> m_timer;

    std::map<QnUuid, IoStateChangeHandler> m_handlers;

    struct IoPortContext
    {
        int enabledCounter = 0;
        int forciblyEnabledCounter = 0;
        IoPortState portState = IoPortState::undefined;
    };

    mutable std::map</*portId*/ QString, IoPortContext> m_outputPortStates;
    std::set<QnIOStateData> m_lastInputPortStates;
};

} // namespace nx::vms::server::nvr::hanwha
