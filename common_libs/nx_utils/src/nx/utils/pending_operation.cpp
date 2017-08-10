#include "pending_operation.h"

#include <QtCore/QTimer>

namespace nx {
namespace utils {

PendingOperation::PendingOperation(const Callback& callback, int interval, QObject* parent):
    QObject(parent),
    m_callback(callback),
    m_timer(new QTimer(this))
{
    m_timer->setInterval(interval);
    connect(m_timer, &QTimer::timeout, this,
        [this]()
        {
            if (!m_requested)
            {
                m_timer->stop();
                return;
            }

            m_requested = false;
            m_callback();
        });
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

} // namespace utils
} // namespace nx
