#include "time_slider.h"

#include <QtGui/QPainter>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <ui/style/noptix_style.h>



QnTimeSlider::QnTimeSlider(QGraphicsItem *parent):
    base_type(parent)
{
    setProperty(Qn::SliderLength, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding, QSizePolicy::Slider);

    QnTimePeriodList l;
    
    l << QnTimePeriod(10, 10);
    l << QnTimePeriod(30, 10);
    setTimePeriods(SelectionLine, RecordingPeriod, l);
    l.clear();
    
    l << QnTimePeriod(15, 3);
    setTimePeriods(SelectionLine, MotionPeriod, l);

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
    /* Initialize converter. It will be used a lot. */
    m_converter = PositionValueConverter(this);

    QRectF rect = this->rect();

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);


    drawPeriodsBar(painter, m_timePeriods[SelectionLine][RecordingPeriod],  m_timePeriods[SelectionLine][MotionPeriod], rect.top() + rect.height() * 0.5,   rect.height() * 0.25);
    drawPeriodsBar(painter, m_timePeriods[LayoutLine][RecordingPeriod],     m_timePeriods[LayoutLine][MotionPeriod],    rect.top() + rect.height() * 0.75,  rect.height() * 0.25);
}

void QnTimeSlider::drawPeriodsBar(QPainter *painter, QnTimePeriodList &recorded, QnTimePeriodList &motion, qreal top, qreal height) {
    qreal leftPos = m_converter.positionFromValue(minimum());
    qreal rightPos = m_converter.positionFromValue(maximum());
    qreal centralPos = m_converter.positionFromValue(sliderPosition());

    if(!qFuzzyCompare(leftPos, centralPos))
        painter->fillRect(QRectF(leftPos, top, centralPos - leftPos, height), QColor(64, 64, 64));
    if(!qFuzzyCompare(rightPos, centralPos))
        painter->fillRect(QRectF(centralPos, top, rightPos - centralPos, height), QColor(0, 0, 0));

    drawPeriods(painter, recorded, top, height, QColor(24, 128, 24), QColor(12, 64, 12));
    drawPeriods(painter, motion, top, height, QColor(128, 0, 0), QColor(64, 0, 0));
}

void QnTimeSlider::drawPeriods(QPainter *painter, QnTimePeriodList &periods, qreal top, qreal height, const QColor &preColor, const QColor &pastColor) {
    if (periods.isEmpty())
        return;

    // TODO: rewrite this so that there is no drawing motion periods over recorded ones.

    qint64 minimumValue = this->minimum();
    qint64 maximumValue = this->maximum();
    qreal centralPos = m_converter.positionFromValue(this->sliderPosition());

    QnTimePeriodList::const_iterator begin = periods.findNearestPeriod(minimumValue, false);
    QnTimePeriodList::const_iterator end = periods.findNearestPeriod(maximumValue, true);

    for (QnTimePeriodList::const_iterator pos = begin; pos < end; ++pos)
    {
        qreal leftPos = m_converter.positionFromValue(qMax(pos->startTimeMs, minimumValue));
        qreal rightPos = m_converter.positionFromValue(pos->durationMs == -1 ? maximumValue : qMin(pos->startTimeMs + pos->durationMs, maximumValue));
            
        if(rightPos <= centralPos) {
            painter->fillRect(QRectF(leftPos, top, rightPos - leftPos, height), preColor);
        } else if(leftPos >= centralPos) {
            painter->fillRect(QRectF(leftPos, top, rightPos - leftPos, height), pastColor);
        } else {
            painter->fillRect(QRectF(leftPos, top, centralPos - leftPos, height), preColor);
            painter->fillRect(QRectF(centralPos, top, rightPos - centralPos, height), pastColor);
        }
    }
}
