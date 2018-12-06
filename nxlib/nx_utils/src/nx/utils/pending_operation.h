#pragma once

#include <chrono>
#include <functional>

#include <QtCore/QObject>

class QTimer;

namespace nx {
namespace utils {

/**
 * Class QnPendingOperation executes the given callback
 * when requestOperation() is called but
 * not more often than the given interval.
 */
class NX_UTILS_API PendingOperation: public QObject
{
public:
    using Callback = std::function<void()>;

    enum Flag
    {
        NoFlags = 0,
        /** Invoke the callback immediately after the first operation request. */
        FireImmediately = 0x01,
        /** Do not invoke the callback until requests are stopped. */
        FireOnlyWhenIdle = 0x02
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    PendingOperation(QObject* parent = nullptr);
    PendingOperation(const Callback& callback, int intervalMs, QObject* parent = nullptr);
    PendingOperation(const Callback& callback, std::chrono::milliseconds interval,
        QObject* parent = nullptr);

    void requestOperation();
    void fire(); //< Fire immediately, no matter if it was previously requested or not.

    Flags flags() const;
    void setFlags(Flags flags);

    int intervalMs() const;
    void setIntervalMs(int value);

    std::chrono::milliseconds interval() const;
    void setInterval(std::chrono::milliseconds value);

    void setCallback(const Callback& callback);

private:
    Callback m_callback;
    Flags m_flags = FireImmediately;
    QTimer* const m_timer = nullptr;
    bool m_requested = false;
};

} // namespace utils
} // namespace nx
