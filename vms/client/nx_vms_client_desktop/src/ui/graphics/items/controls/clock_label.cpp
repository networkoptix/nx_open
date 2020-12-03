#include "clock_label.h"

#include <chrono>

#include <QtCore/QDateTime>

#include <translation/datetime_formatter.h>

namespace
{

using namespace std::chrono;
static constexpr auto kClockUpdatePeriod = 1s;

} // namespace

QnClockLabel::QnClockLabel(QGraphicsItem* parent):
    base_type(parent)
{
    QFont font;
    font.setPixelSize(18);
    font.setWeight(QFont::DemiBold);
    setFont(font);

    m_timerId = startTimer(kClockUpdatePeriod);
}

QnClockLabel::~QnClockLabel()
{
    killTimer(m_timerId);
}

void QnClockLabel::timerEvent(QTimerEvent*)
{
    setText(datetime::toString(QDateTime::currentMSecsSinceEpoch(), datetime::Format::hh_mm_ss));
}
