#include "time_slider.h"


#include <QtGui/QPainter>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <ui/style/noptix_style.h>
#include <ui/graphics/items/standard/graphics_slider_p.h>

#include "tool_tip_item.h"


QnTimeSlider::QnTimeSlider(QGraphicsItem *parent):
    base_type(parent),
    m_windowStart(0),
    m_windowEnd(0),
    m_options(0),
    m_oldMinimum(0),
    m_oldMaximum(0)
{
    setProperty(Qn::SliderLength, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding, QSizePolicy::Slider);

    setWindowStart(minimum());
    setWindowEnd(maximum());
    setOptions(StickToMinimum | StickToMaximum);

    connect(this, SIGNAL(actionTriggered(int)), this, SLOT(at_actionTriggered(int)));

    sliderChange(SliderRangeChange);

    




    setMaximum(10000);

    setWindowStart(1000);
    setWindowEnd(5000);

    QnTimePeriodList l;
    
    l << QnTimePeriod(1000, 1000);
    l << QnTimePeriod(3000, 1000);
    setTimePeriods(SelectionLine, RecordingPeriod, l);
    l.clear();
    
    l << QnTimePeriod(1500, 300);
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

QnTimeSlider::Options QnTimeSlider::options() const {
    return m_options;
}

void QnTimeSlider::setOptions(Options options) {
    m_options = options;
}

qint64 QnTimeSlider::windowStart() const {
    return m_windowStart;
}

void QnTimeSlider::setWindowStart(qint64 windowStart) {
    windowStart = qBound(minimum(), windowStart, maximum());
    if(windowStart == m_windowStart)
        return;

    m_windowStart = windowStart;
}

qint64 QnTimeSlider::windowEnd() const {
    return m_windowEnd;
}

void QnTimeSlider::setWindowEnd(qint64 windowEnd) {
    windowEnd = qBound(minimum(), windowEnd, maximum());
    if(windowEnd == m_windowEnd)
        return;

    m_windowEnd = windowEnd;
}

QPointF QnTimeSlider::positionFromValue(qint64 logicalValue) const {
    Q_D(const GraphicsSlider);

    d->ensureMapper();

    return QPointF(
        d->pixelPosMin + GraphicsStyle::sliderPositionFromValue(m_windowStart, m_windowEnd, logicalValue, d->pixelPosMax - d->pixelPosMin, d->upsideDown), 
        0.0
    );
}

qint64 QnTimeSlider::valueFromPosition(const QPointF &position) const {
    Q_D(const GraphicsSlider);

    d->ensureMapper();

    return GraphicsStyle::sliderValueFromPosition(m_windowStart, m_windowEnd, position.x(), d->pixelPosMax - d->pixelPosMin, d->upsideDown);
}

void QnTimeSlider::updateToolTipVisibility() {
    qint64 pos = sliderPosition();

    toolTipItem()->setVisible(pos >= m_windowStart && pos <= m_windowEnd);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnTimeSlider::sliderChange(SliderChange change) {
    base_type::sliderChange(change);

    if(change == SliderRangeChange) {
        if((m_options & StickToMinimum) && m_oldMinimum == m_windowStart)
            setWindowStart(minimum());

        if((m_options & StickToMaximum) && m_oldMaximum == m_windowEnd)
            setWindowEnd(maximum());

        m_oldMinimum = minimum();
        m_oldMaximum = maximum();
    }
}

void QnTimeSlider::at_actionTriggered(int action) {
    if(action == SliderMove)
        updateToolTipVisibility();
}


// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnTimeSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    /* Initialize converter. It will be used a lot. */
    QRectF rect = this->rect();

    drawPeriodsBar(painter, m_timePeriods[SelectionLine][RecordingPeriod],  m_timePeriods[SelectionLine][MotionPeriod], rect.top() + rect.height() * 0.5,   rect.height() * 0.25);
    drawPeriodsBar(painter, m_timePeriods[LayoutLine][RecordingPeriod],     m_timePeriods[LayoutLine][MotionPeriod],    rect.top() + rect.height() * 0.75,  rect.height() * 0.25);

    qint64 pos = sliderPosition();
    if(pos >= m_windowStart && pos <= m_windowEnd) {
        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
        QnScopedPainterPenRollback penRollback(painter, QPen(QColor(255, 255, 255, 196), 0));
        
        qreal x = positionFromValue(pos).x();
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
}

void QnTimeSlider::drawPeriodsBar(QPainter *painter, QnTimePeriodList &recorded, QnTimePeriodList &motion, qreal top, qreal height) {
    qreal leftPos = positionFromValue(windowStart()).x();
    qreal rightPos = positionFromValue(windowEnd()).x();
    qreal centralPos = positionFromValue(sliderPosition()).x();

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

    qint64 minimumValue = this->windowStart();
    qint64 maximumValue = this->windowEnd();
    qreal centralPos = positionFromValue(this->sliderPosition()).x();

    QnTimePeriodList::const_iterator begin = periods.findNearestPeriod(minimumValue, false);
    QnTimePeriodList::const_iterator end = periods.findNearestPeriod(maximumValue, true);

    for (QnTimePeriodList::const_iterator pos = begin; pos < end; ++pos)
    {
        qreal leftPos = positionFromValue(qMax(pos->startTimeMs, minimumValue)).x();
        qreal rightPos = positionFromValue(pos->durationMs == -1 ? maximumValue : qMin(pos->startTimeMs + pos->durationMs, maximumValue)).x();
            
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
