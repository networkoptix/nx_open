#include "generic_notifier.h"

#include <QtCore/QTimerEvent>
#include <QtCore/QDateTime>

namespace {
    enum {
        CheckPeriodMSecs = 1000,
        RestartPeriodMSecs = 1000 * 60 * 60,
        PrecisionMSecs = 100
    };

    bool isChanged(qint64 last, qint64 current, qint64 expectedDelta) {
        return qAbs((current - last) - expectedDelta) > PrecisionMSecs;
    }

} // anonymous namespace

QnGenericNotifier::QnGenericNotifier(QObject *parent): 
    base_type(parent),
    m_lastUtcTime( 0 ),
    m_lastLocalTime( 0 ),
    m_lastElapsedTime( 0 ),
    m_lastTimeZoneOffset( std::numeric_limits<qint64>::max() )
{
    //default values ensure that first call updateTimes(true) will emit all signals

    m_checkTimer.start(CheckPeriodMSecs, this);
    m_restartTimer.start(RestartPeriodMSecs, this);
    m_elapsedTimer.start();
}

QnGenericNotifier::~QnGenericNotifier() {
    return;
}

void QnGenericNotifier::updateTimes(bool notify) {
    qint64 elapsedTime = m_elapsedTimer.elapsed();
    
    QDateTime localDateTime = QDateTime::currentDateTime();
    QDateTime utcDateTime = localDateTime.toUTC();
    localDateTime.setTimeSpec(Qt::UTC);

    qint64 localTime = localDateTime.toMSecsSinceEpoch();
    qint64 utcTime = utcDateTime.toMSecsSinceEpoch();
    qint64 timeZoneOffset = localTime - utcTime;

    if(notify) {
        qint64 delta = elapsedTime - m_lastElapsedTime;
        if(isChanged(m_lastLocalTime, localTime, delta) || isChanged(m_lastUtcTime, utcTime, delta))
            this->notify(TimeValue);

        if(m_lastTimeZoneOffset != timeZoneOffset)
            this->notify(TimeZoneValue);
    }

    m_lastElapsedTime = elapsedTime;
    m_lastLocalTime = localTime;
    m_lastUtcTime = utcTime;
    m_lastTimeZoneOffset = timeZoneOffset;
}

void QnGenericNotifier::timerEvent(QTimerEvent *event) {
    if(event->timerId() == m_checkTimer.timerId()) {
        updateTimes(true);
    } else if(event->timerId() == m_restartTimer.timerId()) {
        m_elapsedTimer.restart();
        updateTimes(false);
    } else {
        base_type::timerEvent(event);
    }
}


