#pragma once

#include <memory>

#include <nx/utils/thread/mutex.h>

#include <nx/vms/server/nvr/i_fan_manager.h>

namespace nx::vms::server::nvr::hanwha {

class FanStateFetcher;
class IFanPlatformAbstraction;

class FanManager: public IFanManager
{
public:
    FanManager(std::unique_ptr<IFanPlatformAbstraction> platformAbstraction);

    virtual ~FanManager() override;

    virtual void start() override;

    virtual void stop() override;

    virtual FanState state() const override;

    virtual HandlerId registerStateChangeHandler(StateChangeHandler handler) override;

    virtual void unregisterStateChangeHandler(HandlerId handlerId) override;

private:
    void updateState(FanState state);

public:
    mutable nx::utils::Mutex m_mutex;
    mutable nx::utils::Mutex m_handlerMutex;

    std::unique_ptr<IFanPlatformAbstraction> m_platformAbstraction;
    std::unique_ptr<FanStateFetcher> m_stateFetcher;

    std::map<HandlerId, StateChangeHandler> m_handlers;
    FanState m_lastState = FanState::undefined;

    HandlerId m_maxHandlerId = 0;

    bool m_isStopped = false;
};

} // namespace nx::vms::server::nvr::hanwha
