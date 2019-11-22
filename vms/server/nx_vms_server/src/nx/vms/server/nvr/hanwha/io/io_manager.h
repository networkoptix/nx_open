#pragma once

#include <map>
#include <set>

#include <nx/utils/thread/mutex.h>

#include <nx/vms/server/nvr/i_io_manager.h>

namespace nx::utils { class TimerManager; }

namespace nx::vms::server::nvr::hanwha {

class IoStateFetcher;
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
    void updatePortStates(const std::set<QnIOStateData>& portStates);

    bool setOutputPortStateInternal(const QString& portId);
    IoPortState calculateOutputPortState(const QString& portId) const;

private:
    mutable nx::utils::Mutex m_mutex;
    mutable nx::utils::Mutex m_handlerMutex;

    std::unique_ptr<IIoPlatformAbstraction> m_platformAbstraction;
    std::unique_ptr<IoStateFetcher> m_stateFetcher;
    std::unique_ptr<nx::utils::TimerManager> m_timerManager;

    std::map<QnUuid, IoStateChangeHandler> m_handlers;

    mutable std::map</*portId*/ QString, /*enabledCounter*/int> m_outputPortCounters;
    std::set<QnIOStateData> m_lastPortStates;
};

} // namespace nx::vms::server::nvr::hanwha
