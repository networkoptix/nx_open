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
    class TickmarkType {
        Q_DECLARE_TR_FUNCTIONS(TickmarkType);
    public:
        enum Type {
            Milliseconds,
            Days,
            Months,
            Years
        };

        TickmarkType(): type(Milliseconds), unitMSecs(0), stepMSecs(0), stepUnits(0), wrapUnits(0) {}

        TickmarkType(Type type, qint64 unitMSecs, int stepUnits, int wrapUnits, const QString &format, const QString &longestString):
            type(type),
            unitMSecs(unitMSecs),
            stepMSecs(unitMSecs * stepUnits),
            stepUnits(stepUnits),
            wrapUnits(wrapUnits),
            format(format),
            longestString(longestString)
        {}

        /** Type of the tickmark. */
        Type type;

        /** Size of the unit in which tickmark value is measured, in milliseconds. */
        qint64 unitMSecs;

        /** Tickmark step, in milliseconds */
        qint64 stepMSecs;

        /** Tickmark step, in units. */
        int stepUnits;

        /** Number of units for a wrap-around. */
        int wrapUnits;
        
        /** Format string for the tickmark value. */
        QString format;

        /** Longest possible string representation of the tickmark value. */
        QString longestString;

        static const QVector<TickmarkType> utc;
        static const QVector<TickmarkType> nonUtc;

    private:
        static QVector<TickmarkType> createStandardTypes() {
            QVector<TickmarkType> result;
            result <<
                TickmarkType(Milliseconds,  1ll,                                100,    1000,   tr("ms"),       tr("100ms")) <<
                TickmarkType(Milliseconds,  1000ll,                             1,      60,     tr("s"),        tr("59s")) <<
                TickmarkType(Milliseconds,  1000ll,                             5,      60,     tr("s"),        tr("59s")) <<
                TickmarkType(Milliseconds,  1000ll,                             10,     60,     tr("s"),        tr("59s")) <<
                TickmarkType(Milliseconds,  1000ll,                             30,     60,     tr("s"),        tr("59s")) <<
                TickmarkType(Milliseconds,  1000ll,                             1,      60,     tr("m"),        tr("59m")) <<
                TickmarkType(Milliseconds,  1000ll * 60,                        5,      60,     tr("m"),        tr("59m")) <<
                TickmarkType(Milliseconds,  1000ll * 60,                        10,     60,     tr("m"),        tr("59m")) <<
                TickmarkType(Milliseconds,  1000ll * 60,                        30,     60,     tr("m"),        tr("59m")) <<
                TickmarkType(Milliseconds,  1000ll * 60 * 60,                   1,      24,     tr("h"),        tr("23h")) <<
                TickmarkType(Milliseconds,  1000ll * 60 * 60,                   3,      24,     tr("h"),        tr("23h")) <<
                TickmarkType(Milliseconds,  1000ll * 60 * 60,                   12,     24,     tr("h"),        tr("23h"));
            return result;
        }

        static QVector<TickmarkType> createUtcTypes() {
            QVector<TickmarkType> result;
            result <<
                createStandardTypes() <<
                TickmarkType(Days,          1000ll * 60 * 60 * 24,              1,      31,     tr("dd MMM"),   tr("29 Mar")) <<
                TickmarkType(Months,        1000ll * 60 * 60 * 24 * 31,         1,      12,     tr("MMMM"),     tr("September")) <<
                TickmarkType(Years,         1000ll * 60 * 60 * 24 * 365,        1,      50000,  tr("yyyy"),     tr("2000"));
            return result;
        }

        static QVector<TickmarkType> createNonUtcTypes() {
            QVector<TickmarkType> result;
            result <<
                createStandardTypes() <<
                TickmarkType(Milliseconds,  1000ll * 60 * 60 * 24,              1,      31,     tr("d"),        tr("29d")) <<
                TickmarkType(Milliseconds,  1000ll * 60 * 60 * 24 * 30,         1,      12,     tr("M"),        tr("11M")) <<
                TickmarkType(Milliseconds,  1000ll * 60 * 60 * 24 * 30 * 12,    1,      50000,  tr("y"),        tr("2000y"));
            return result;
        }

    };

    const QVector<TickmarkType> TickmarkType::utc = TickmarkType::createUtcTypes();
    const QVector<TickmarkType> TickmarkType::nonUtc = TickmarkType::createNonUtcTypes();

    qint64 timeToMSecs(const QTime &time) {
        return QTime(0, 0, 0, 0).msecsTo(time);
    }

    QTime msecsToTime(qint64 msecs) {
        int msecsPart = msecs % 1000;
        msecs /= 1000;
        int secs = msecs % 60;
        msecs /= 60;
        int mins = msecs % 60;
        msecs /= 60;
        int hours = msecs % 24;

        return QTime(hours, mins, secs, msecsPart);
    }

    qint64 roundUp(qint64 value, qint64 step) {
        value = value + step - 1;
        return value - value % step;
    }

    qint64 roundUp(qint64 msecs, const TickmarkType &tickmarkType, bool useUtc) {
        if(!useUtc) 
            return roundUp(msecs, tickmarkType.stepMSecs);
        
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs);
        switch(tickmarkType.type) {
        case TickmarkType::Milliseconds:
            dateTime.setTime(msecsToTime(roundUp(timeToMSecs(dateTime.time()), tickmarkType.stepMSecs)));
            break;
        case TickmarkType::Days:
            if(dateTime.time() != QTime(0, 0, 0, 0)) {
                dateTime.setTime(QTime(0, 0, 0, 0));
                dateTime.addDays(1);
            }
            break;
        case TickmarkType::Months:
            if(dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1) {
                dateTime.setTime(QTime(0, 0, 0, 0));
                dateTime.setDate(QDate(dateTime.date().year(), dateTime.date().month(), 1));
                dateTime.addMonths(1);
            }
            break;
        case TickmarkType::Years:
            if(dateTime.time() != QTime(0, 0, 0, 0) || dateTime.date().day() != 1 || dateTime.date().month() != 1) {
                dateTime.setTime(QTime(0, 0, 0, 0));
                dateTime.setDate(QDate(dateTime.date().year(), 1, 1));
                dateTime.addYears(1);
            }
            break;
        default:
            qnWarning("Invalid tickmark type '%1'.", static_cast<int>(tickmarkType.type));
            break;
        }

        return dateTime.toMSecsSinceEpoch();
    }

    qint64 absoluteNumber(qint64 msecs, const TickmarkType &tickmarkType, bool useUtc) {
        if(!useUtc)
            return msecs / tickmarkType.stepMSecs;

        QDateTime baseDateTime = QDateTime::fromMSecsSinceEpoch(0);
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(msecs);
        switch(tickmarkType.type) {
        case TickmarkType::Milliseconds:
            return baseDateTime.msecsTo(dateTime) / tickmarkType.stepMSecs;
        case TickmarkType::Days:
            return baseDateTime.daysTo(dateTime);
        case TickmarkType::Months:
            return baseDateTime.date().year() * 12 + baseDateTime.date().month();
        case TickmarkType::Years:
            return baseDateTime.date().year();
        default:
            qnWarning("Invalid tickmark type '%1'.", static_cast<int>(tickmarkType.type));
            return 0;
        }
    }

    const int minTickmarkSpanPixels = 5;
    const qreal minHighlightSpanFraction = 0.5;

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

    qint64 msecsPerPixel = (m_windowEnd - m_windowStart) / size().width();
    if(msecsPerPixel == 0)
        msecsPerPixel = 1; /* Technically, we should never get here, but we want to feel safe. */

    bool useUtc = m_options & UseUTC;

    const QVector<TickmarkType> &types = useUtc ? TickmarkType::utc : TickmarkType::nonUtc;

    /* Find minimal index of the interval type to use. */
    int minTickmarkIndex = 0;
    for(; minTickmarkIndex < types.size(); minTickmarkIndex++)
        if(types[minTickmarkIndex].stepMSecs / msecsPerPixel >= minTickmarkSpanPixels)
            break;
    minTickmarkIndex = qMin(minTickmarkIndex, types.size() - 1);
    const TickmarkType &minTickmarkType = types[minTickmarkIndex];

    /* Find maximal index of the interval type to use. */
    int highlightIndex = minTickmarkIndex;
    int highlightSpanPixels = size().width() * minHighlightSpanFraction;
    for(; highlightIndex < types.size(); highlightIndex++)
        if(types[highlightIndex].stepMSecs / msecsPerPixel >= highlightSpanPixels && types[highlightIndex].stepUnits == 1)
            break;
    highlightIndex = qMin(highlightIndex, types.size() - 1); // TODO: remove this line.
    const TickmarkType &highlightType = types[highlightIndex];


    /* Draw highlight. */
    {
        painter->setPen(Qt::NoPen);

        qint64 pos = roundUp(m_windowStart, highlightType, useUtc);
        qreal x0 = positionFromValue(m_windowStart).x();
        qint64 number = absoluteNumber(pos, highlightType, useUtc);
        while(true) {
            qreal x1 = positionFromValue(pos).x();
            painter->setBrush(number % 2 ? QColor(0, 0, 0) : QColor(32, 32, 32));
            painter->drawRect(QRectF(x0, top, x1 - x0, height));

            if(pos >= m_windowEnd)
                break;

            pos += highlightType.stepMSecs;
            number++;
            x0 = x1;
        }
    }

    /* Round position to the closest tickmark that will be displayed. */
    //qint64 pos = roundUp(m_windowStart, minTickmarkType, useUtc);

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

