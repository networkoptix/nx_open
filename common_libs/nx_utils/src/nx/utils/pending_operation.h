#pragma once

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

    PendingOperation(const Callback& callback, int interval, QObject* parent = nullptr);

    void requestOperation();

    Flags flags() const;
    void setFlags(Flags flags);

private:
    Callback m_callback;
    Flags m_flags = FireImmediately;
    QTimer* m_timer = nullptr;
    bool m_requested = false;
};

} // namespace utils
} // namespace nx
