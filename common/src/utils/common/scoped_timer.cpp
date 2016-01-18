#include "scoped_timer.h"

QnScopedTimer::QnScopedTimer(const QString &description)
    : m_description(description)
{
    logMessage(">", -1);
    m_timer.start();
}

QnScopedTimer::~QnScopedTimer()
{
    logMessage("<", m_timer.elapsed());
}

qint64 QnScopedTimer::currentElapsedTime() const
{
    return m_timer.elapsed();
}

void QnScopedTimer::logCurrentElapsedTime()
{
    logMessage("-", m_timer.elapsed());
}

void QnScopedTimer::logMessage(const char *tag, qint64 time)
{
    if (time >= 0)
        qDebug() << "[" << tag << m_description << "]:" << time << "ms";
    else
        qDebug() << "[" << tag << m_description << "]";
}
