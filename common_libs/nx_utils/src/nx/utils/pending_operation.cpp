#include "pending_operation.h"

#include <QtCore/QTimer>

namespace nx {
namespace utils {

PendingOperation::PendingOperation(QObject* parent):
    QObject(parent),
    m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this,
        [this]()
        {
            if (!m_requested)
            {
                m_timer->stop();
                return;
            }

            m_requested = false;
            if (m_callback)
                m_callback();
        });
}

PendingOperation::PendingOperation(const Callback& callback, int intervalMs, QObject* parent):
    PendingOperation(parent)
{
    setCallback(callback);
    m_timer->setInterval(intervalMs);
}

void PendingOperation::requestOperation()
{
    if (m_timer->isActive())
    {
        m_requested = true;

        if (m_flags.testFlag(FireOnlyWhenIdle))
            m_timer->start();

        return;
    }

    if (m_flags.testFlag(FireImmediately))
    {
        m_requested = false;
        if (m_callback)
            m_callback();
    }
    else
    {
        m_requested = true;
    }

    m_timer->start();
}

PendingOperation::Flags PendingOperation::flags() const
{
    return m_flags;
}

void PendingOperation::setFlags(Flags flags)
{
    m_flags = flags;
}

int PendingOperation::intervalMs() const
{
    return m_timer->interval();
}

void PendingOperation::setIntervalMs(int value)
{
    m_timer->setInterval(value);
}

void PendingOperation::setCallback(const Callback& callback)
{
    m_callback = callback;
}

} // namespace utils
} // namespace nx
