#include "time_slider.h"

#include <cassert>

#include <QtCore/QDateTime>
#include <QtGui/QPainter>
#include <QtGui/QGraphicsSceneWheelEvent>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <ui/style/noptix_style.h>
#include <ui/graphics/items/standard/graphics_slider_p.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/processors/kinetic_cutting_processor.h>

#include "tool_tip_item.h"

namespace {
    bool isDateLikeFormat(const QString &format) {
        bool inQuote = false;
        foreach(const QChar &c, format) {
            switch(c.unicode()) {
            case '\'':
                inQuote = !inQuote;
                break;
            case 'd':
            case 'M':
            case 'y':
                if(!inQuote)
                    return true;
                break;
            default:
                break;
            }
        }
        return false;
    }

    const qreal degreesFor2x = 180.0;

} // anonymous namespace



QnTimeSlider::QnTimeSlider(QGraphicsItem *parent):
    base_type(parent),
    m_windowStart(0),
    m_windowEnd(0),
    m_options(0),
    m_oldMinimum(0),
    m_oldMaximum(0),
    m_toolTipFormat(),
    m_dateLikeToolTipFormat(false)
{
    /* Set default property values. */
    setProperty(Qn::SliderLength, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding, QSizePolicy::Slider);

    setWindowStart(minimum());
    setWindowEnd(maximum());
    setOptions(StickToMinimum | StickToMaximum | UpdateToolTip);

    setToolTipFormat(tr("hh:mm:ss", "DEFAULT_TOOL_TIP_FORMAT"));
    
    /* Prepare kinetic zoom processors. */
    KineticCuttingProcessor *processor = new KineticCuttingProcessor(QMetaType::QReal, this);
    processor->setHandler(this);
    processor->setMaxShiftInterval(0.4);
    processor->setFriction(degreesFor2x / 2);
    processor->setMaxSpeedMagnitude(degreesFor2x * 8);
    processor->setSpeedCuttingThreshold(degreesFor2x / 3);
    processor->setFlags(KineticProcessor::IGNORE_DELTA_TIME);

    /* Run handlers. */
    sliderChange(SliderRangeChange);

    setMaximum(10000);

    setLineCount(2);

    QnTimePeriodList l;
    
    l << QnTimePeriod(1000, 1000);
    l << QnTimePeriod(3000, 1000);
    setTimePeriods(0, Qn::RecordingTimePeriod, l);
    l.clear();
    
    l << QnTimePeriod(1500, 300);
    setTimePeriods(0, Qn::MotionTimePeriod, l);

}

QnTimeSlider::~QnTimeSlider() {
    return;
}

int QnTimeSlider::lineCount() const {
    return m_timePeriods.size();
}

void QnTimeSlider::setLineCount(int lineCount) {
    m_timePeriods.resize(lineCount);
}

QnTimePeriodList QnTimeSlider::timePeriods(int line, Qn::TimePeriodType type) const {
    assert(type >= 0 && type < Qn::TimePeriodTypeCount);
    
    return m_timePeriods[line].forType[type];
}

void QnTimeSlider::setTimePeriods(int line, Qn::TimePeriodType type, const QnTimePeriodList &timePeriods) {
    assert(type >= 0 && type < Qn::TimePeriodTypeCount);

    m_timePeriods[line].forType[type] = timePeriods;
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
    setWindow(windowStart, qMax(windowStart, m_windowEnd));

    windowStart = qBound(minimum(), windowStart, maximum());
    if(windowStart == m_windowStart)
        return;

    m_windowStart = windowStart;

    emit windowChanged(m_windowStart, m_windowEnd);

    updateToolTipVisibility();
}

qint64 QnTimeSlider::windowEnd() const {
    return m_windowEnd;
}

void QnTimeSlider::setWindowEnd(qint64 windowEnd) {
    setWindow(qMin(m_windowStart, windowEnd), windowEnd);

    windowEnd = qBound(minimum(), windowEnd, maximum());
    if(windowEnd == m_windowEnd)
        return;

    m_windowEnd = windowEnd;

    emit windowChanged(m_windowStart, m_windowEnd);

    updateToolTipVisibility();
}

void QnTimeSlider::setWindow(qint64 start, qint64 end) {
    start = qBound(minimum(), start, maximum());
    end = qMax(start, qBound(minimum(), end, maximum()));

    if (m_windowStart != start || m_windowEnd != end) {
        m_windowStart = start;
        m_windowEnd = end;

        sliderChange(SliderMappingChange);
        emit windowChanged(m_windowStart, m_windowEnd);

        updateToolTipVisibility();
    }
}

const QString &QnTimeSlider::toolTipFormat() const {
    return m_toolTipFormat;
}

void QnTimeSlider::setToolTipFormat(const QString &format) {
    if(m_toolTipFormat == format)
        return;

    m_toolTipFormat = format;
    m_dateLikeToolTipFormat = isDateLikeFormat(format);

    updateToolTipText();
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

void QnTimeSlider::updateToolTipText() {
    if(!(m_options & UpdateToolTip))
        return;

    qint64 pos = sliderPosition();

    QString toolTip;
    if(m_dateLikeToolTipFormat) {
        toolTip = QDateTime::fromMSecsSinceEpoch(pos).toString(m_toolTipFormat);
    } else {
        toolTip = QTime(pos / (1000ll * 60ll * 60ll) % 24ll, pos / (1000ll * 60ll) % 60ll, pos / 1000ll % 60ll, pos % 1000ll).toString(m_toolTipFormat);
    }
    setToolTip(toolTip);
}

void QnTimeSlider::scaleWindow(qreal factor, qint64 anchor) {
    qint64 start = anchor + (m_windowStart - anchor) * factor;
    qint64 end = anchor + (m_windowEnd - anchor) * factor;

    setWindowStart(start);
    setWindowEnd(end);
}



// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnTimeSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    /* Initialize converter. It will be used a lot. */
    QRectF rect = this->rect();

    for(int i = 0; i < m_timePeriods.size(); i++) {
        drawPeriodsBar(
            painter, 
            m_timePeriods[i].forType[Qn::RecordingTimePeriod],  
            m_timePeriods[i].forType[Qn::MotionTimePeriod], 
            rect.top() + rect.height() * (0.5 + 0.5 * i / m_timePeriods.size()),   
            rect.height() * 0.5 / m_timePeriods.size()
        );
    }

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

void QnTimeSlider::drawScale(QPainter *painter, qreal top, qreal height) {
    
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnTimeSlider::sliderChange(SliderChange change) {
    base_type::sliderChange(change);

    switch(change) {
    case SliderRangeChange:
        if((m_options & StickToMinimum) && m_oldMinimum == m_windowStart)
            setWindowStart(minimum());

        if((m_options & StickToMaximum) && m_oldMaximum == m_windowEnd)
            setWindowEnd(maximum());

        m_oldMinimum = minimum();
        m_oldMaximum = maximum();
        break;
    case SliderValueChange:
        updateToolTipVisibility();
        updateToolTipText();
        break;
    default:
        break;
    }
}

QVariant QnTimeSlider::itemChange(GraphicsItemChange change, const QVariant &value) {
    if(change == ItemSceneHasChanged)
        kineticProcessor()->setTimer(InstrumentManager::animationTimerOf(scene()));
    
    return base_type::itemChange(change, value);
}

void QnTimeSlider::wheelEvent(QGraphicsSceneWheelEvent *event) {
    /* delta() returns the distance that the wheel is rotated 
     * in eighths (1/8s) of a degree. */
    qreal degrees = event->delta() / 8.0;

    m_zoomAnchor = valueFromPosition(event->pos());
    kineticProcessor()->shift(degrees);
    kineticProcessor()->start();

    event->accept();
}

void QnTimeSlider::kineticMove(const QVariant &degrees) {
    qreal factor = std::pow(2.0, -degrees.toReal() / degreesFor2x);
    
    scaleWindow(factor, m_zoomAnchor);
}

