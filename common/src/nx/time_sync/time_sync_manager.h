#pragma once

#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <core/resource/resource_fwd.h>
#include <nx/network/abstract_socket.h>
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
namespace time_sync {

class TimeSyncManager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
public:

    static const QString kTimeSyncUrlPath;

    /**
     * TimeSynchronizationManager::start MUST be called before using class instance.
     */
    TimeSyncManager(QnCommonModule* commonModule);
    virtual ~TimeSyncManager();

    virtual void stop();

    virtual void start();

    /** @return synchronized time (milliseconds from epoch, UTC). */
    std::chrono::milliseconds getSyncTime() const;

    /** @return True if current server has load time from internet at least once after start. */
    bool isTimeTakenFromInternet() const;

    void setClock(
        const std::shared_ptr<AbstractSystemClock>& systemClock,
        const std::shared_ptr<AbstractSteadyClock>& steadyClock);

    std::chrono::milliseconds timeSyncInterval() const;

signals:
    /** Emitted when synchronized time has been changed. */
    void timeChanged(qint64 syncTimeMs);

protected:
    using AbstractStreamSocketPtr = std::unique_ptr<nx::network::AbstractStreamSocket>;

    bool loadTimeFromServer(const QnRoute& route);
    void loadTimeFromLocalClock();

    virtual void updateTime() = 0;
    virtual AbstractStreamSocketPtr connectToRemoteHost(const QnRoute& route);
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
} // namespace nx
