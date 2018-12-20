#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QString>

class QnScopedTimer
{
public:
    QnScopedTimer(const char *description, qint64 detailMs = QnScopedTimer::kDefaultDetailMs);
    QnScopedTimer(const QString &description, qint64 detailMs = QnScopedTimer::kDefaultDetailMs);
    ~QnScopedTimer();

    qint64 currentElapsedTime() const;
    void logCurrentElapsedTime();

    static const int kDefaultDetailMs = 10;
    static const int kLogAll = -2;      /* Special value to log also constructors. */
private:
    void logMessage(const char *tag, qint64 time);

private:
    QString m_description;
    qint64 m_detailMs;
    QElapsedTimer m_timer;
};

#ifdef _DEBUG
#define QN_LOG_TIME(context) QnScopedTimer __scoped_timer(context)
#else
#define QN_LOG_TIME(context) qt_noop()
#endif
