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

    virtual FanState state() const override;

    virtual QnUuid registerStateChangeHandler(StateChangeHandler handler) override;

    virtual void unregisterStateChangeHandler(QnUuid handlerId) override;

private:
    void updateState(FanState state);

public:
    mutable nx::utils::Mutex m_mutex;
    mutable nx::utils::Mutex m_handlerMutex;

    std::unique_ptr<IFanPlatformAbstraction> m_platformAbstraction;
    std::unique_ptr<FanStateFetcher> m_stateFetcher;

    std::map<QnUuid, StateChangeHandler> m_handlers;
    FanState m_lastState = FanState::undefined;
};

} // namespace nx::vms::server::nvr::hanwha
