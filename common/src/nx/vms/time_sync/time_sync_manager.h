#include <common/common_module_aware.h>
#include <nx/utils/timer_manager.h>
#include <core/resource/resource_fwd.h>

class AbstractSystemClock
{
public:
    virtual ~AbstractSystemClock() = default;

    virtual std::chrono::milliseconds millisSinceEpoch() = 0;
};

class AbstractSteadyClock
{
public:
    virtual ~AbstractSteadyClock() = default;

    /**
    * @return Time from some unspecified epoch.
    */
    virtual std::chrono::milliseconds now() = 0;
};

namespace nx {
namespace vvms {
namespace time_sync {

class TimeSynchManager:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT
public:

    /**
     * TimeSynchronizationManager::start MUST be called before using class instance.
     */
    TimeSynchManager(
		QnCommonModule* commonModule,
		Qn::PeerType peerType,
        nx::utils::StandaloneTimerManager* const timerManager,
        const std::shared_ptr<AbstractSystemClock>& systemClock = nullptr,
        const std::shared_ptr<AbstractSteadyClock>& steadyClock = nullptr);
    virtual ~TimeSynchManager();

    /** Implementation of QnStoppable::pleaseStop. */
    virtual void pleaseStop();

    void start();

    /** Returns synchronized time (millis from epoch, UTC). */
    qint64 getSyncTime() const;
    
signals:
    /** Emitted when synchronized time has been changed. */
    void timeChanged(qint64 syncTime);
private:
    void doPeriodicTasks();
    QnMediaServerResourcePtr getPrimaryTimeServer();
    void updateTimeFromInternet();
    void loadTimeFromServer();
    QSharedPointer<nx::network::AbstractStreamSocket> connectToRemoteHost(const QnRoute& route);
private:
    qint64 m_synchronizedTime;
};

} // namespace time_sync
} // namespace vms
} // namespace nx
