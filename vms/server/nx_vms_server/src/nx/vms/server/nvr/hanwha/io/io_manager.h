#pragma once

#include <map>
#include <set>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>

#include <nx/vms/server/nvr/i_io_manager.h>

namespace nx::vms::server::nvr::hanwha {

class IoStateFetcher;
class IIoPlatformAbstraction;

class IoManager: public IIoManager
{
public:
    IoManager(std::unique_ptr<IIoPlatformAbstraction> platformAbstraction);

    virtual ~IoManager() override;

    virtual void start() override;

    virtual void stop() override;

    virtual QnIOPortDataList portDesriptiors() const override;

    virtual void setPortCircuitTypes(
        const std::map<QString, Qn::IODefaultState>& circuitTypeByPort) override;

    virtual bool setOutputPortState(
        const QString& portId,
        IoPortState state,
        std::chrono::milliseconds autoResetTimeout) override;

    virtual QnIOStateDataList portStates() const override;

    virtual HandlerId registerStateChangeHandler(IoStateChangeHandler handler) override;

    virtual void unregisterStateChangeHandler(HandlerId handlerId) override;

private:
    void initialize();

    void updatePortStates(const std::set<QnIOStateData>& portStates);
    void sendInitialStateIfNeeded();

    bool setOutputPortStateInternal(const QString& portId, IoPortState portState);

private:
    mutable nx::utils::Mutex m_mutex;
    mutable nx::utils::Mutex m_handlerMutex;

    std::unique_ptr<IIoPlatformAbstraction> m_platformAbstraction;
    std::unique_ptr<IoStateFetcher> m_stateFetcher;
    std::unique_ptr<nx::utils::TimerManager> m_timerManager;

    struct HandlerContext
    {
        bool intialStateHasBeenReported = false;
        IoStateChangeHandler handler;
    };

    std::map<HandlerId, HandlerContext> m_handlerContexts;

    struct IoPortContext
    {
        nx::utils::TimerId timerId = 0;
        IoPortState currentState = IoPortState::undefined;
        Qn::IODefaultState circuitType = Qn::IODefaultState::IO_OpenCircuit;
        Qn::IOPortType portType = Qn::IOPortType::PT_Unknown;
    };

    mutable std::map</*portId*/ QString, IoPortContext> m_portContexts;
    std::set<QnIOStateData> m_lastPortStates;

    HandlerId m_maxHandlerId = 0;

    bool m_isStopped = false;
};

} // namespace nx::vms::server::nvr::hanwha
