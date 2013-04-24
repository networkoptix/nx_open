#include "clock_data_provider.h"

#include <QDateTime>

#include <utils/settings.h>

QnClockDataProvider::QnClockDataProvider(QObject *parent) :
    QObject(parent),
    m_format(qnSettings->isClock24Hour() ? Hour24 : Hour12),
    m_showWeekDay(qnSettings->isClockWeekdayOn()),
    m_showDateAndMonth(qnSettings->isClockDateOn()),
    m_showSeconds(qnSettings->isClockSecondsOn()),
    m_timer(new QTimer(this))
{
    connect(qnSettings->notifier(QnSettings::CLOCK_24HOUR),     SIGNAL(valueChanged(int)), this, SLOT(updateFormatString()));
    connect(qnSettings->notifier(QnSettings::CLOCK_WEEKDAY),    SIGNAL(valueChanged(int)), this, SLOT(updateFormatString()));
    connect(qnSettings->notifier(QnSettings::CLOCK_DATE),       SIGNAL(valueChanged(int)), this, SLOT(updateFormatString()));
    connect(qnSettings->notifier(QnSettings::CLOCK_SECONDS),    SIGNAL(valueChanged(int)), this, SLOT(updateFormatString()));

    updateFormatString();

    connect(m_timer, SIGNAL(timeout()), this, SLOT(at_timer_timeout()));
    m_timer->start(1000);
}

QnClockDataProvider::~QnClockDataProvider() {

}

void QnClockDataProvider::at_timer_timeout() {
    emit timeChanged(QDateTime::currentDateTime().toString(m_formatString));
}

void QnClockDataProvider::updateFormatString() {
    m_formatString = QString();
    if (m_showWeekDay)
        m_formatString += QLatin1String("ddd ");
    if (m_showDateAndMonth)
        m_formatString += QLatin1String("MMM d ");
    m_formatString += QLatin1String("hh:mm");
    if (m_showSeconds)
        m_formatString += QLatin1String(":ss");
    if (m_format == Hour12)
        m_formatString += QLatin1String(" AP");
}
