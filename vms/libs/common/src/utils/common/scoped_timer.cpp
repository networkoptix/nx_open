#include "scoped_timer.h"

#include <QtCore/QDebug>

QnScopedTimer::QnScopedTimer(const QString &description, qint64 detailMs)
    : m_description(description)
    , m_detailMs(detailMs)
{
    logMessage(">", -1);
    m_timer.start();
}

QnScopedTimer::QnScopedTimer(const char *description, qint64 detailMs)
    : m_description(QString::fromUtf8(description))
    , m_detailMs(detailMs)
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
    if (time < m_detailMs)
        return;

    if (time >= 0)
        qDebug() << "[" << tag << m_description << "]:" << time << "ms";
    else
        qDebug() << "[" << tag << m_description << "]";
}
