#include "pending_operation.h"

QnPendingOperation::QnPendingOperation(const Callback &callback, int interval, QObject *parent)
    : QObject(parent)
    , m_callback(callback)
    , m_timer(new QTimer(this))
    , m_requested(false)
{
    m_timer->setInterval(interval);
    connect(m_timer, &QTimer::timeout, this, [this](){
        if (!m_requested) {
            m_timer->stop();
            return;
        }

        m_requested = false;
        m_callback();
    });
}

void QnPendingOperation::requestOperation() {
    if (m_timer->isActive()) {
        m_requested = true;
        return;
    }

    m_timer->start();
    m_requested = false;
    m_callback();
}

