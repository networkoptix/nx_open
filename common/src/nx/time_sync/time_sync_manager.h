#pragma once

#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <core/resource/resource_fwd.h>
#include <nx/network/abstract_socket.h>
#include <common/common_module_aware.h>
#include <nx/utils/thread/mutex.h>

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

struct QnRoute;

namespace nx {
namespace time_sync {

class TimeSyncManager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
public:

    /**
     * TimeSynchronizationManager::start MUST be called before using class instance.
     */
    TimeSyncManager(
        QnCommonModule* commonModule,
        const std::shared_ptr<AbstractSystemClock>& systemClock = nullptr,
        const std::shared_ptr<AbstractSteadyClock>& steadyClock = nullptr);
    virtual ~TimeSyncManager();

    virtual void stop();

    virtual void start();

    /** @return synchronized time (milliseconds from epoch, UTC). */
    std::chrono::milliseconds getSyncTime() const;

signals:
    /** Emitted when synchronized time has been changed. */
    void timeChanged(qint64 syncTimeMs);
protected:
    using AbstractStreamSocketPtr = std::unique_ptr<nx::network::AbstractStreamSocket>;

    void loadTimeFromServer(const QnRoute& route);
    void loadTimeFromLocalClock();

    virtual void updateTime() = 0;
    virtual AbstractStreamSocketPtr connectToRemoteHost(const QnRoute& route);
    void setSyncTime(std::chrono::milliseconds value);
private:
    using AbstractStreamSocketPtr = std::unique_ptr<nx::network::AbstractStreamSocket>;

    void doPeriodicTasks();
protected:
    std::shared_ptr<AbstractSystemClock> m_systemClock;
    std::shared_ptr<AbstractSteadyClock> m_steadyClock;
private:
    std::chrono::milliseconds m_synchronizedTime{0};
    std::chrono::milliseconds m_synchronizedOnClock{0};

    mutable QnMutex m_mutex;

    QThread* m_thread = nullptr;
	QTimer* m_timer = nullptr;
};

} // namespace time_sync
} // namespace nx
