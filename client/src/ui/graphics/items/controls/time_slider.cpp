#include "time_slider.h"
#include <utils/common/warnings.h>
#include <ui/style/noptix_style.h>

QnTimeSlider::QnTimeSlider(QGraphicsItem *parent):
    base_type(parent)
{
    setProperty(Qn::SliderLength, 0);
}

QnTimeSlider::~QnTimeSlider() {
    return;
}

QnTimePeriodList QnTimeSlider::timePeriods(DisplayLine line, PeriodType type) const {
    return m_timePeriods[line][type];
}

void QnTimeSlider::setTimePeriods(DisplayLine line, PeriodType type, const QnTimePeriodList &timePeriods) {
    m_timePeriods[line][type] = timePeriods;
}

void QnTimeSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    base_type::paint(painter, option, widget);
}

