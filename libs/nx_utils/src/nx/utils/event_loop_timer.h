#pragma once

#include <nx/utils/move_only_func.h>

#include <chrono>
#include <mutex>

#include <QTimer>
#include <QtCore/QObject>

namespace nx::utils {

/**
 * QTimer based timer. Allows longer timeouts than QTimer. Uses QTimer internally and needs
 * to be created in a thread with an active event loop or moved to such thread before starting.
 * Note: Singleshot.
 * Note: Thread-safe.
 */
class EventLoopTimer: public QObject
{
public:
    /**
     * @param checkPeriod Inner state update period. In most cases, default value should be used.
     *     Should be more than 1 ms.
     */
    EventLoopTimer(std::chrono::milliseconds checkPeriod = std::chrono::minutes(10));

    /**
     * @brief Cancels previously started timer if any.
     */
    void start(std::chrono::milliseconds timeout, MoveOnlyFunc<void()> handler);

private:
    QTimer m_timer;
    MoveOnlyFunc<void()> m_handler;
    std::chrono::milliseconds m_leftTimeout{std::chrono::milliseconds::zero()};
    std::chrono::milliseconds m_checkPeriod{std::chrono::milliseconds::zero()};
    std::chrono::steady_clock::time_point m_startPoint;
    std::mutex m_mutex;

    void onTimer();
};

} // namespace nx::utils
