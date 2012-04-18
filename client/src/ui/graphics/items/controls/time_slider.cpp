#include "time_slider.h"

#include <cassert>

#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>

#include <QtGui/QPainter>
#include <QtGui/QGraphicsSceneWheelEvent>

#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <ui/style/noptix_style.h>
#include <ui/graphics/items/standard/graphics_slider_p.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/processors/kinetic_cutting_processor.h>

#include "tool_tip_item.h"

namespace {
    class TimeStep {
        Q_DECLARE_TR_FUNCTIONS(TimeStep);
    public:
        enum Type {
            Milliseconds,
            Days,
            Months,
            Years
        };

        TimeStep(): type(Milliseconds), unitMSecs(0), stepMSecs(0), stepUnits(0), wrapUnits(0) {}

        TimeStep(Type type, qint64 unitMSecs, int stepUnits, int wrapUnits, const QString &format, const QString &longestString, bool isRelative = true):
            type(type),
            unitMSecs(unitMSecs),
            stepMSecs(unitMSecs * stepUnits),
            stepUnits(stepUnits),
            wrapUnits(wrapUnits),
            format(format),
            longestString(longestString)
        {}

        /** Type of the time step. */
        Type type;

        /** Size of the unit in which step value is measured, in milliseconds. */
        qint64 unitMSecs;

        /** Time step, in milliseconds */
        qint64 stepMSecs;

        /** Time step, in units. */
        int stepUnits;

        /** Number of units for a wrap-around. */
        int wrapUnits;
        
        /** Format string for the step value. */
        QString format;

        /** Longest possible string representation of the step value. */
        QString longestString;

        /** Whether this time step is to be used for relative times (e.g. time intervals), 
         * or for absolute times (i.e. obtained via <tt>QDateTime::toMSecsSinceEpoch</tt>). */
        bool isRelative;

    public:
        static QVector<TimeStep> createAbsoluteSteps() {
            QVector<TimeStep> result;
            result <<
                createStandardSteps(false) <<
                TimeStep(Days,          1000ll * 60 * 60 * 24,              1,      31,     tr("dd MMM"),   tr("29 Mar"),       false) <<
                TimeStep(Months,        1000ll * 60 * 60 * 24 * 31,         1,      12,     tr("MMMM"),     tr("September"),    false) <<
                TimeStep(Years,         1000ll * 60 * 60 * 24 * 365,        1,      50000,  tr("yyyy"),     tr("2000"),         false);
            return result;
        }

        static QVector<TimeStep> createRelativeSteps() {
            QVector<TimeStep> result;
            result <<
                createStandardSteps(true) <<
                TimeStep(Milliseconds,  1000ll * 60 * 60 * 24,              1,      31,     tr("d"),        tr("29d"),          false) <<
                TimeStep(Milliseconds,  1000ll * 60 * 60 * 24 * 30,         1,      12,     tr("M"),        tr("11M"),          false) <<
                TimeStep(Milliseconds,  1000ll * 60 * 60 * 24 * 30 * 12,    1,      50000,  tr("y"),        tr("2000y"),        false);
            return result;
        }



    private:
        static QVector<TimeStep> createStandardSteps(bool isRelative) {
            QVector<TimeStep> result;
            result <<
                TimeStep(Milliseconds,  1ll,                                100,    1000,   tr("ms"),       tr("100ms"),        isRelative) <<
                TimeStep(Milliseconds,  1000ll,                             1,      60,     tr("s"),        tr("59s"),          isRelative) <<
                TimeStep(Milliseconds,  1000ll,                             5,      60,     tr("s"),        tr("59s"),          isRelative) <<
                TimeStep(Milliseconds,  1000ll,                             10,     60,     tr("s"),        tr("59s"),          isRelative) <<
                TimeStep(Milliseconds,  1000ll,                             30,     60,     tr("s"),        tr("59s"),          isRelative) <<
                TimeStep(Milliseconds,  1000ll,                             1,      60,     tr("m"),        tr("59m"),          isRelative) <<
                TimeStep(Milliseconds,  1000ll * 60,                        5,      60,     tr("m"),        tr("59m"),          isRelative) <<
                TimeStep(Milliseconds,  1000ll * 60,                        10,     60,     tr("m"),        tr("59m"),          isRelative) <<
                TimeStep(Milliseconds,  1000ll * 60,                        30,     60,     tr("m"),        tr("59m"),          isRelative) <<
                TimeStep(Milliseconds,  1000ll * 60 * 60,                   1,      24,     tr("h"),        tr("23h"),          isRelative) <<
                TimeStep(Milliseconds,  1000ll * 60 * 60,                   3,      24,     tr("h"),        tr("23h"),          isRelative) <<
                TimeStep(Milliseconds,  1000ll * 60 * 60,                   12,     24,     tr("h"),        tr("23h"),          isRelative);
            return result;
        }

    };

    const QVector<TimeStep> absoluteTimeSteps = TimeStep::createAbsoluteSteps();
    const QVector<TimeStep> relativeTimeSteps = TimeStep::createRelativeSteps();

    qint64 timeToMSecs(const QTime &time) {
        return QTime(0, 0, 0, 0).msecsTo(time);
    }

    QTime msecsToTime(qint64 msecs) {
        return QTime(0, 0, 0, 0).addMSecs(msecs); 
    }

    template<class T>
    T roundUp(T value, T step) {
        value = value + step - 1;
        return value - value % step;
    }

    qint64 roundUp(qint64 msecs, const TimeStep &step) {
        if(step.isRelative) 
            return roundUp(msecs, step.stepMSecs);
        
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs);
        switch(step.type) {
        case TimeStep::Milliseconds:
            dateTime.setTime(msecsToTime(roundUp(timeToMSecs(dateTime.time()), step.stepMSecs)));
            break;
        case TimeStep::Days:
            if(dateTime.time() != QTime(0, 0, 0, 0) || (dateTime.date().day() != 1 && dateTime.date().day() % step.stepUnits != 0)) {
                dateTime.setTime(QTime(0, 0, 0, 0));

                int oldDay = dateTime.date().day();
                int newDay = qMin(roundUp(oldDay + 1, step.stepUnits), dateTime.date().daysInMonth() + 1);
                dateTime = dateTime.addDays(newDay - oldDay);
            }
            break;
        case TimeStep::Months:
            if(dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1 || ((dateTime.date().month() - 1) % step.stepUnits != 0)) {
                dateTime.setTime(QTime(0, 0, 0, 0));
                dateTime.setDate(QDate(dateTime.date().year(), dateTime.date().month(), 1));
                
                int oldMonth = dateTime.date().month();
                /* We should have added 1 to month() here we don't want to end
                 * up with the same month number, but months are numbered from 1,
                 * so the addition is not needed. */
                int newMonth = roundUp(oldMonth, step.stepUnits) + 1;
                dateTime = dateTime.addMonths(newMonth - oldMonth);
            }
            break;
        case TimeStep::Years:
            if(dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1 || dateTime.date().month() != 1 || dateTime.date().year() % step.stepUnits != 0) {
                dateTime.setTime(QTime(0, 0, 0, 0));
                dateTime.setDate(QDate(dateTime.date().year(), 1, 1));
                
                int oldYear = dateTime.date().year();
                int newYear = roundUp(oldYear + 1, step.stepUnits);
                dateTime = dateTime.addYears(newYear - oldYear);
            }
            break;
        default:
            qnWarning("Invalid time step type '%1'.", static_cast<int>(step.type));
            break;
        }

        return dateTime.toMSecsSinceEpoch();
    }

    qint64 add(qint64 msecs, const TimeStep &step) {
        if(step.isRelative)
            return msecs + step.stepMSecs;

        switch(step.type) {
        case TimeStep::Milliseconds:
        case TimeStep::Days:
            return msecs + step.stepMSecs;
        case TimeStep::Months:
            return QDateTime::fromMSecsSinceEpoch(msecs).addMonths(step.stepUnits).toMSecsSinceEpoch();
        case TimeStep::Years:
            return QDateTime::fromMSecsSinceEpoch(msecs).addYears(step.stepUnits).toMSecsSinceEpoch();
        default:
            qnWarning("Invalid time step type '%1'.", static_cast<int>(step.type));
            return msecs;
        }
    }

    const QDateTime baseDateTime = QDateTime::fromMSecsSinceEpoch(0);

    qint64 absoluteNumber(qint64 msecs, const TimeStep &step) {
        if(!step.isRelative)
            return msecs / step.stepMSecs;

        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs);
        switch(step.type) {
        case TimeStep::Milliseconds:
        case TimeStep::Days:
            return baseDateTime.msecsTo(dateTime) / step.stepMSecs;
        case TimeStep::Months: {
            int year, month;
            dateTime.date().getDate(&year, &month, NULL);

            return (year * 12 + month) / step.stepUnits;
        }
        case TimeStep::Years:
            return dateTime.date().year() / step.stepUnits;
        default:
            qnWarning("Invalid time step type '%1'.", static_cast<int>(step.type));
            return 0;
        }
    }

    const qreal minTickmarkSpanPixels = 5;
    const qreal minHighlightSpanFraction = 0.5;

    /** Lower zoom limit. */
    const qreal minMSecsPerPixel = 2.0;

    const qreal degreesFor2x = 180.0;

} // anonymous namespace



QnTimeSlider::QnTimeSlider(QGraphicsItem *parent):
    base_type(parent),
    m_windowStart(0),
    m_windowEnd(0),
    m_options(0),
    m_oldMinimum(0),
    m_oldMaximum(0),
    m_toolTipFormat()
{
    /* Set default property values. */
    setProperty(Qn::SliderLength, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding, QSizePolicy::Slider);

    setWindowStart(minimum());
    setWindowEnd(maximum());
    setOptions(StickToMinimum | StickToMaximum | UpdateToolTip);

    setToolTipFormat(tr("hh:mm:ss", "DEFAULT_TOOL_TIP_FORMAT"));
    
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QDateTime utcDateTime = currentDateTime.toUTC();
    currentDateTime.setTimeSpec(Qt::UTC);
    m_utcOffsetMSec = utcDateTime.msecsTo(currentDateTime);

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











    // testing code
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

void QnTimeSlider::setOption(Option option, bool value) {
    if(value) {
        setOptions(m_options | option);
    } else {
        setOptions(m_options & ~option);
    }
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
    if(m_options & UseUTC) {
        toolTip = QDateTime::fromMSecsSinceEpoch(pos).toString(m_toolTipFormat);
    } else {
        toolTip = msecsToTime(pos).toString(m_toolTipFormat);
    }
    setToolTip(toolTip);
}

bool QnTimeSlider::scaleWindow(qreal factor, qint64 anchor) {
    qreal msecsPerPixel = (m_windowEnd - m_windowStart) / size().width();
    qreal targetMSecsPerPixel = msecsPerPixel * factor;
    if(targetMSecsPerPixel < minMSecsPerPixel) {
        factor = minMSecsPerPixel / msecsPerPixel;
        if(qFuzzyCompare(factor, 1.0))
            return false; /* We've reached the min scale. */
    }

    qint64 start = anchor + (m_windowStart - anchor) * factor;
    qint64 end = anchor + (m_windowEnd - anchor) * factor;

    if(end > maximum()) {
        start = maximum() - (end - start);
        end = maximum();
    }
    if(start < minimum()) {
        end = minimum() + (end - start);
        start = minimum();
    }

    setWindow(start, end);

    /* If after two adjustments desired window end still lies outside the 
     * slider range, then we've reached the max scale. */
    return end <= maximum();
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

    drawTickmarks(painter, rect.top(), rect.height() * 0.5);

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

void QnTimeSlider::drawTickmarks(QPainter *painter, qreal top, qreal height) {
    qDebug() << m_windowEnd << m_windowStart;

    qreal msecsPerPixel = (m_windowEnd - m_windowStart) / size().width();
    if(qFuzzyIsNull(msecsPerPixel))
        msecsPerPixel = 1.0; /* Technically, we should never get here, but we want to feel safe. */

    const QVector<TimeStep> &steps = m_options & UseUTC ? absoluteTimeSteps : relativeTimeSteps;

    /* Find minimal index of the time step to use. */
    int minTickmarkIndex = 0;
    for(; minTickmarkIndex < steps.size(); minTickmarkIndex++)
        if(steps[minTickmarkIndex].stepMSecs / msecsPerPixel >= minTickmarkSpanPixels)
            break;
    minTickmarkIndex = qMin(minTickmarkIndex, steps.size() - 1);
    const TimeStep &minTickmarkStep = steps[minTickmarkIndex];

    /* Find index of the highlight time step. */
    int highlightIndex = minTickmarkIndex;
    qreal highlightSpanPixels = size().width() * minHighlightSpanFraction;
    for(; highlightIndex < steps.size(); highlightIndex++)
        if(steps[highlightIndex].stepUnits == 1 && steps[highlightIndex].stepMSecs / msecsPerPixel >= highlightSpanPixels)
            break;
    highlightIndex = qMin(highlightIndex, steps.size() - 1); // TODO: remove this line.
    const TimeStep &highlightStep = steps[highlightIndex];


    /* Draw highlight. */
    {
        painter->setPen(Qt::NoPen);

        qint64 pos = roundUp(m_windowStart, highlightStep);
        qreal x0 = positionFromValue(m_windowStart).x();
        qint64 number = absoluteNumber(pos, highlightStep);
        while(true) {
            qreal x1 = positionFromValue(pos).x();
            painter->setBrush(number % 2 ? QColor(0, 0, 0) : QColor(32, 32, 32));
            painter->drawRect(QRectF(x0, top, x1 - x0, height));

            if(pos >= m_windowEnd)
                break;

            pos = add(pos, highlightStep);
            number++;
            x0 = x1;
        }
    }

    /* Draw tickmarks. */
    {
        painter->setPen(QColor(255, 255, 255));

        qreal bottom = top + height;

        qint64 pos = roundUp(m_windowStart, minTickmarkStep);

        while(true) {
            qreal x = positionFromValue(pos).x();
            painter->drawLine(QPointF(x, top), QPointF(x, bottom));
            
            pos = add(pos, minTickmarkStep);
            if(pos >= m_windowEnd)
                break;
        }
    }
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
    
    if(!scaleWindow(factor, m_zoomAnchor))
        kineticProcessor()->reset();
}

