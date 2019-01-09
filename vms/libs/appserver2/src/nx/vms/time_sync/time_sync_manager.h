#pragma once

#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <core/resource/resource_fwd.h>
#include <nx/network/abstract_socket.h>
#include <nx/vms/time_sync/abstract_time_sync_manager.h>
#include <common/common_module_aware.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/time.h>

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

class SystemClock: public AbstractSystemClock
{
public:
    virtual std::chrono::milliseconds millisSinceEpoch() override
    {
        return nx::utils::millisSinceEpoch();
    }
};

class SteadyClock: public AbstractSteadyClock
{
public:
    virtual std::chrono::milliseconds now() override
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
    }
};

struct QnRoute;

namespace nx {
namespace vms {
namespace time_sync {

class TimeSyncManager: public AbstractTimeSyncManager, public QnCommonModuleAware
{
    Q_OBJECT
public:
    static const std::chrono::milliseconds kMaxJitterForLocalClock;

    static const QString kTimeSyncUrlPath;

    /**
     * TimeSynchronizationManager::start MUST be called before using class instance.
     */
    TimeSyncManager(QnCommonModule* commonModule);
    virtual ~TimeSyncManager();

    virtual void stop();

    virtual void start();

    /** @return synchronized time (milliseconds from epoch, UTC). */
    virtual std::chrono::milliseconds getSyncTime(
        bool* outIsTimeTakenFromInternet = nullptr) const override;

    void setClock(
        const std::shared_ptr<AbstractSystemClock>& systemClock,
        const std::shared_ptr<AbstractSteadyClock>& steadyClock);

    std::chrono::milliseconds timeSyncInterval() const;

    QString idForToStringFromPtr() const;
protected:
    using AbstractStreamSocketPtr = std::unique_ptr<nx::network::AbstractStreamSocket>;

    bool loadTimeFromServer(const QnRoute& route);
    void loadTimeFromLocalClock();

    virtual void updateTime() = 0;
    virtual AbstractStreamSocketPtr connectToRemoteHost(const QnRoute& route, bool sslRequired);
    virtual bool setSyncTime(std::chrono::milliseconds value, std::chrono::milliseconds rtt);
    void setSyncTimeInternal(std::chrono::milliseconds value);

private:
    void doPeriodicTasks();

protected:
    std::shared_ptr<AbstractSystemClock> m_systemClock;
    std::shared_ptr<AbstractSteadyClock> m_steadyClock;
    std::atomic<bool> m_isTimeTakenFromInternet{false};
private:
    std::chrono::milliseconds m_synchronizedTime{0};
    std::chrono::milliseconds m_synchronizedOnClock{0};

    mutable QnMutex m_mutex;

    QThread* m_thread = nullptr;
    QTimer* m_timer = nullptr;
};

} // namespace time_sync
} // namespace vms
} // namespace nx
