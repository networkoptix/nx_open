#pragma once

#include <QtCore/QThread>

#include <nx/utils/timer_manager.h>
#include <core/resource/resource_fwd.h>
#include <network/router.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/time/mean_time_fetcher.h>
#include <common/common_module_aware.h>

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
namespace mediaserver {

class ReverseConnectionManager;

class TimeSyncManager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
public:

    /**
     * TimeSynchronizationManager::start MUST be called before using class instance.
     */
    TimeSyncManager(
        QnCommonModule* commonModule,
        ReverseConnectionManager* reverseConnectionManager,
        const std::shared_ptr<AbstractSystemClock>& systemClock = nullptr,
        const std::shared_ptr<AbstractSteadyClock>& steadyClock = nullptr);
    virtual ~TimeSyncManager();

    virtual void stop();

    virtual void start();

    /** Returns synchronized time (millis from epoch, UTC). */
    qint64 getSyncTime() const;

    std::chrono::milliseconds getTime() const;
signals:
    /** Emitted when synchronized time has been changed. */
    void timeChanged(qint64 syncTime);
private:
    using AbstractStreamSocketPtr = std::unique_ptr<nx::network::AbstractStreamSocket>;

    void loadTimeFromInternet();
    void loadTimeFromLocalClock();
    void loadTimeFromServer(const QnRoute& route);

    void doPeriodicTasks();
    QnMediaServerResourcePtr getPrimaryTimeServer();
    AbstractStreamSocketPtr connectToRemoteHost(const QnRoute& route);
    void initializeTimeFetcher();

    void setTime(std::chrono::milliseconds value);
private:
    std::unique_ptr<AbstractAccurateTimeFetcher> m_internetTimeSynchronizer;
    std::atomic<bool> m_internetSyncInProgress{false};

    std::chrono::milliseconds m_synchronizedTime{0};
    std::chrono::milliseconds m_synchronizedOnClock{0};

    std::shared_ptr<AbstractSystemClock> m_systemClock;
    std::shared_ptr<AbstractSteadyClock> m_steadyClock;
    mutable QnMutex m_mutex;

    QThread* m_thread = nullptr;
	QTimer* m_timer = nullptr;
    ReverseConnectionManager* m_reverseConnectionManager = nullptr;
};

} // namespace mediaserver
} // namespace nx
