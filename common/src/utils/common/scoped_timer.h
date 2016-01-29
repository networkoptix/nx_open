#pragma once

#include <QtCore/QElapsedTimer>

class QnScopedTimer
{
public:
    QnScopedTimer(const QString &description);
    ~QnScopedTimer();

    qint64 currentElapsedTime() const;
    void logCurrentElapsedTime();

private:
    void logMessage(const char *tag, qint64 time);

private:
    QString m_description;
    QElapsedTimer m_timer;
};
