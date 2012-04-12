#include "time_slider.h"
#include <utils/common/warnings.h>
#include <ui/style/noptix_style.h>

namespace {

}


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


// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnTimeSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    
}

void QnTimeSlider::drawPeriodsBar(QPainter *painter, QnTimePeriodList &recorded, QnTimePeriodList &motion) {
    
}

void QnTimeSlider::drawPeriods(QPainter *painter, QnTimePeriodList &periods, const QColor &preColor, const QColor &pastColor) {
    if (periods.isEmpty())
        return;

    /*
    bool upsideDown = 

    const qint64 pos = this->sliderPosition();


    QRectF contentsRect = this->contentsRect();
    qreal x = contentsRect.left() + m_handleRect.width() / 2;

    qreal msInPixel = getMsInPixel();

    QnTimePeriodList::const_iterator beginItr = timePeriods.findNearestPeriod(pos, false);
    QnTimePeriodList::const_iterator endItr = timePeriods.findNearestPeriod(pos+m_parent->sliderRange(), false);

    int center = m_handleRect.center().x();
    QColor light = color.lighter();
    QColor dark = color;

    //foreach(const QnTimePeriod &period, timePeriods)
    for (QnTimePeriodList::const_iterator itr = beginItr; itr <= endItr; ++itr)
    {
        const QnTimePeriod& period = *itr;
        qreal left = x + static_cast<qreal>(period.startTimeMs - pos) / msInPixel;
        left = qMax(left, contentsRect.left());

        qreal right;
        if (period.durationMs == -1) {
            right = contentsRect.right();
        } else {
            right = x + static_cast<qreal>((period.startTimeMs + period.durationMs) - pos) / msInPixel;
            right = qMin(right, contentsRect.right());
        }

        //if (left > right)
        //    continue;

        if(right < center) {
            painter->fillRect(left, contentsRect.top(), qMax(1.0, right - left), contentsRect.height(), light);
        } else if(left > center) {
            painter->fillRect(left, contentsRect.top(), qMax(1.0, right - left), contentsRect.height(), dark);
        } else {
            painter->fillRect(left, contentsRect.top(), center - left, contentsRect.height(), light);
            painter->fillRect(center, contentsRect.top(), qMax(1.0, right - center), contentsRect.height(), dark);
        }
    }*/
}


