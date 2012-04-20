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
#include <ui/style/globals.h>
#include <ui/graphics/items/standard/graphics_slider_p.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/processors/kinetic_cutting_processor.h>

#include "tool_tip_item.h"

namespace {
    QTime msecsToTime(qint64 msecs) {
        return QTime(0, 0, 0, 0).addMSecs(msecs); 
    }

    inline qreal adjust(qreal value, qreal target, qreal delta) {
        if(value < target) {
            if(target - value < delta) {
                return target;
            } else {
                return value + delta;
            }
        } else {
            if(value - target < delta) {
                return target;
            } else {
                return value - delta;
            }
        }
    }

    class TimeStepFactory: public QnTimeSlider {
    public:
        using QnTimeSlider::createAbsoluteSteps;
        using QnTimeSlider::createRelativeSteps;
    };
    Q_GLOBAL_STATIC_WITH_ARGS(const QVector<QnTimeStep>, absoluteTimeSteps, (TimeStepFactory::createAbsoluteSteps()));
    Q_GLOBAL_STATIC_WITH_ARGS(const QVector<QnTimeStep>, relativeTimeSteps, (TimeStepFactory::createRelativeSteps()));

    QPixmap renderText(const QString &text, const QPen &pen, const QFont &font, int fontPixelSize = -1) {
        QFont actualFont = font;
        if(fontPixelSize != -1)
            actualFont.setPixelSize(fontPixelSize);

        QFontMetrics metrics(actualFont);
        QSize textSize = metrics.size(Qt::TextSingleLine, text);

        QPixmap pixmap(textSize.width(), metrics.height());
        pixmap.fill(QColor(0, 0, 0, 0));

        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setPen(pen);
        painter.setFont(actualFont);
        painter.drawText(pixmap.rect(), Qt::AlignCenter | Qt::TextSingleLine, text);
        painter.end();

        return pixmap;
    }

    const qreal minTickmarkSpanPixels = 1.0;
    const qreal maxTickmarkSpanFraction = 0.75;
    const qreal minTickmarkSeparationPixels = 5.0;
    const qreal criticalTickmarkSeparationPixels = 2.0;
    const qreal minTextSeparationPixels = 30.0;
    const qreal criticalTextSeparationPixels = 20.0;
    
    const qreal tickmarkHeightRelativeAnimationSpeed = 1.0;
    const qreal tickmarkHeightAbsoluteAnimationSpeed = 0.1;
    const qreal tickmarkHeightStartingAnimationSpeed = 2.0;
    
    const qreal tickmarkOpacityRelativeAnimationSpeed = 1.0;
    const qreal tickmarkOpacityAbsoluteAnimationSpeed = 0.1;
    const qreal tickmarkOpacityStartingAnimationSpeed = 2.0;
    
    const qreal minTickmarkOpacity = 0.05;
    const qreal minTextOpacity = 0.5;
    
    const qreal tickmarkStepScale = 2.0 / 3.0;

    const qreal tickmarkTextScale = 0.75;
    const int minTextHeight = 9;
    const qreal maxTickmarkHeight = 1.0 / (1.0 + tickmarkTextScale);

    const qreal minHighlightSpanFraction = 0.5;

    const qreal msecsPerPixelChangeThreshold = 1.0e-4;

    const qreal tickmarkBarRelativeHeight = 0.5;

    const qreal lineCommentRelativeHeight = 0.75;

    
    const QColor tickmarkColor(255, 255, 255, 255);
    const QColor positionMarkerColor(255, 255, 255, 196);

    const QColor selectionColor = qnGlobals->selectionColor();
    const QColor selectionMarkerColor = selectionColor.lighter();

    const QColor pastBackgroundColor(255, 255, 255, 24);
    const QColor futureBackgroundColor(0, 0, 0, 0);

    const QColor pastRecordingColor(64, 255, 64, 128);
    const QColor futureRecordingColor(64, 255, 64, 64);

    const QColor pastMotionColor(255, 0, 0, 128);
    const QColor futureMotionColor(255, 0, 0, 64);


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
    m_animationUpdateMSecsPerPixel(1.0),
    m_msecsPerPixel(1.0),
    m_aggregationMSecs(0.0),
    m_minimalWindow(0)
{
    /* Set default property values. */
    setProperty(Qn::SliderLength, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding, QSizePolicy::Slider);

    setWindowStart(minimum());
    setWindowEnd(maximum());
    setOptions(StickToMinimum | StickToMaximum | UpdateToolTip | SelectionVisible | SelectionEditable);

    setToolTipFormat(tr("hh:mm:ss", "DEFAULT_TOOL_TIP_FORMAT"));
    
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QDateTime utcDateTime = currentDateTime.toUTC();
    currentDateTime.setTimeSpec(Qt::UTC);

    /* Prepare kinetic zoom processors. */
    KineticCuttingProcessor *processor = new KineticCuttingProcessor(QMetaType::QReal, this);
    processor->setHandler(this);
    processor->setMaxShiftInterval(0.4);
    processor->setFriction(degreesFor2x / 2);
    processor->setMaxSpeedMagnitude(degreesFor2x * 8);
    processor->setSpeedCuttingThreshold(degreesFor2x / 3);
    processor->setFlags(KineticProcessor::IGNORE_DELTA_TIME);

    /* Prepare animation timer listener. */
    startListening();

    /* Run handlers. */
    updateSteps();
    updateMinimalWindow();
    sliderChange(SliderRangeChange);
    itemChange(ItemSceneHasChanged, QVariant::fromValue<QGraphicsScene *>(scene()));
}

QnTimeSlider::~QnTimeSlider() {
    return;
}

QVector<QnTimeStep> QnTimeSlider::createAbsoluteSteps() {
    QVector<QnTimeStep> result;
    result <<
        createStandardSteps(false) <<
        QnTimeStep(QnTimeStep::Days,            1000ll * 60 * 60 * 24,              1,      31,     tr("dd MMM"),   tr("29 Mar"),       false) <<
        QnTimeStep(QnTimeStep::Months,          1000ll * 60 * 60 * 24 * 31,         1,      12,     tr("MMMM"),     tr("September"),    false) <<
        QnTimeStep(QnTimeStep::Years,           1000ll * 60 * 60 * 24 * 365,        1,      50000,  tr("yyyy"),     tr("2000"),         false);
    return enumerateSteps(result);
}

QVector<QnTimeStep> QnTimeSlider::createRelativeSteps() {
    QVector<QnTimeStep> result;
    result <<
        createStandardSteps(true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24,              1,      31,     tr("d"),        tr("29d"),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30,         1,      12,     tr("M"),        tr("11M"),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30 * 12,    1,      50000,  tr("y"),        tr("2000y"),        false);
    return enumerateSteps(result);
}

QVector<QnTimeStep> QnTimeSlider::createStandardSteps(bool isRelative) {
    QVector<QnTimeStep> result;
    result <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                10,     1000,   tr("ms"),       tr("10ms"),         isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                50,     1000,   tr("ms"),       tr("50ms"),         isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                100,    1000,   tr("ms"),       tr("100ms"),        isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                500,    1000,   tr("ms"),       tr("500ms"),        isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             1,      60,     tr("s"),        tr("59s"),          isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             5,      60,     tr("s"),        tr("59s"),          isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             10,     60,     tr("s"),        tr("59s"),          isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             30,     60,     tr("s"),        tr("59s"),          isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        1,      60,     tr("m"),        tr("59m"),          isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        5,      60,     tr("m"),        tr("59m"),          isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        10,     60,     tr("m"),        tr("59m"),          isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        30,     60,     tr("m"),        tr("59m"),          isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   1,      24,     tr("h"),        tr("23h"),          isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   3,      24,     tr("h"),        tr("23h"),          isRelative) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   12,     24,     tr("h"),        tr("23h"),          isRelative);
    return enumerateSteps(result);
}

QVector<QnTimeStep> QnTimeSlider::enumerateSteps(const QVector<QnTimeStep> &steps) {
    QVector<QnTimeStep> result = steps;
    for(int i = 0; i < steps.size(); i++)
        result[i].index = i;
    return result;
}

int QnTimeSlider::lineCount() const {
    return m_lineCount;
}

void QnTimeSlider::setLineCount(int lineCount) {
    if(m_lineCount == lineCount)
        return;

    m_lineCount = lineCount;

    m_timePeriods.resize(lineCount);
    m_lineComments.resize(lineCount);
    m_lineCommentPixmaps.resize(lineCount);

    updateLineCommentPixmaps();
}

void QnTimeSlider::setLineComment(int line, const QString &comment) {
    if(m_lineComments[line] == comment)
        return;

    m_lineComments[line] = comment;

    updateLineCommentPixmap(line);
}

QString QnTimeSlider::lineComment(int line) {
    return m_lineComments[line];
}

QnTimePeriodList QnTimeSlider::timePeriods(int line, Qn::TimePeriodType type) const {
    assert(type >= 0 && type < Qn::TimePeriodTypeCount);
    
    return m_timePeriods[line].normal[type];
}

void QnTimeSlider::setTimePeriods(int line, Qn::TimePeriodType type, const QnTimePeriodList &timePeriods) {
    assert(type >= 0 && type < Qn::TimePeriodTypeCount);

    m_timePeriods[line].normal[type] = timePeriods;
    
    updateAggregatedPeriods(line, type);
}

QnTimeSlider::Options QnTimeSlider::options() const {
    return m_options;
}

void QnTimeSlider::setOptions(Options options) {
    if(m_options == options)
        return;

    Options difference = m_options ^ options;
    m_options = options;

    if(difference & UseUTC)
        updateSteps();
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
    setWindow(windowStart, qMax(windowStart + m_minimalWindow, m_windowEnd));
}

qint64 QnTimeSlider::windowEnd() const {
    return m_windowEnd;
}

void QnTimeSlider::setWindowEnd(qint64 windowEnd) {
    setWindow(qMin(m_windowStart, windowEnd - m_minimalWindow), windowEnd);
}

void QnTimeSlider::setWindow(qint64 start, qint64 end) {
    start = qBound(minimum(), start, maximum());
    end = qMax(start, qBound(minimum(), end, maximum()));

    if(end - start < m_minimalWindow) {
        qint64 d0 = m_minimalWindow / 2;
        qint64 d1 = m_minimalWindow - d0;

        end -= d0;
        start -= d1;

        if(start < minimum()) {
            end += minimum() - start;
            start = minimum();
        }
        if(end > maximum()) {
            start += maximum() - end;
            end = maximum();
        }

        start = qBound(minimum(), start, maximum());
        end = qBound(minimum(), end, maximum());
    }

    if (m_windowStart != start || m_windowEnd != end) {
        m_windowStart = start;
        m_windowEnd = end;

        sliderChange(SliderMappingChange);
        emit windowChanged(m_windowStart, m_windowEnd);

        updateToolTipVisibility();
        updateMSecsPerPixel();
    }
}

qint64 QnTimeSlider::selectionStart() const {
    return m_selectionStart;
}

void QnTimeSlider::setSelectionStart(qint64 selectionStart) {
    setSelection(selectionStart, qMax(selectionStart, m_selectionEnd));
}

qint64 QnTimeSlider::selectionEnd() const {
    return m_selectionEnd;
}

void QnTimeSlider::setSelectionEnd(qint64 selectionEnd) {
    setSelection(qMin(m_selectionStart, selectionEnd), selectionEnd);
}

void QnTimeSlider::setSelection(qint64 start, qint64 end) {
    start = qBound(minimum(), start, maximum());
    end = qMax(start, qBound(minimum(), end, maximum()));

    if (m_selectionStart != start || m_selectionEnd != end) {
        m_selectionStart = start;
        m_selectionEnd = end;

        emit selectionChanged(m_windowStart, m_windowEnd);
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

void QnTimeSlider::finishAnimations() {
    animateStepValues(10 * 1000);
}

bool QnTimeSlider::scaleWindow(qreal factor, qint64 anchor) {
    qreal targetMSecsPerPixel = m_msecsPerPixel * factor;
    if(targetMSecsPerPixel < minMSecsPerPixel) {
        factor = minMSecsPerPixel / m_msecsPerPixel;
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

const QPixmap &QnTimeSlider::textPixmap(const QString &text, int height) {
    QPair<QString, int> key(text, height);

    QHash<QPair<QString, int>, QPixmap>::const_iterator itr = m_pixmapBySizedText.find(key);
    if(itr != m_pixmapBySizedText.end())
        return *itr;

    QPixmap result = renderText(
        text,
        QPen(palette().brush(QPalette::Text), 0), 
        font(), 
        height
    );

    return m_pixmapBySizedText[key] = result;
}

const QPixmap &QnTimeSlider::positionPixmap(qint64 position, int height, const QnTimeStep &step) {
    qint32 key = cacheKey(position, height, step);

    QHash<qint32, QPixmap>::const_iterator itr = m_pixmapByPositionKey.find(key);
    if(itr != m_pixmapByPositionKey.end())
        return *itr;

    return m_pixmapByPositionKey[key] = textPixmap(toString(position, step), height);
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

void QnTimeSlider::updateLineCommentPixmap(int line) {
    m_lineCommentPixmaps[line] = textPixmap(
        m_lineComments[line],
        qRound(lineHeight() * lineCommentRelativeHeight)
    );
}

void QnTimeSlider::updateLineCommentPixmaps() {
    for(int i = 0; i < m_lineCount; i++)
        updateLineCommentPixmap(i);
}

void QnTimeSlider::updateSteps() {
    m_steps = (m_options & UseUTC) ? *absoluteTimeSteps() : *relativeTimeSteps();

    m_nextTickmarkPos.resize(m_steps.size());
    m_tickmarkLines.resize(m_steps.size());
    m_stepData.resize(m_steps.size());
}

void QnTimeSlider::updateMSecsPerPixel() {
    qreal msecsPerPixel = (m_windowEnd - m_windowStart) / size().width();
    if(qFuzzyIsNull(msecsPerPixel))
        msecsPerPixel = 1.0; /* Technically, we should never get here, but we want to feel safe. */

    if(qFuzzyCompare(m_msecsPerPixel, msecsPerPixel))
        return;

    m_msecsPerPixel = msecsPerPixel;

    updateStepAnimationTargets();
    updateAggregationValue();
}

void QnTimeSlider::updateMinimalWindow() {
    qint64 minimalWindow = size().width() * minMSecsPerPixel;
    if(minimalWindow == m_minimalWindow)
        return;

    m_minimalWindow = minimalWindow;

    /* Re-bound. */
    setWindowStart(m_windowStart);
}

void QnTimeSlider::updateStepAnimationTargets() {
    bool updateNeeded = qAbs(m_msecsPerPixel - m_animationUpdateMSecsPerPixel) / qMin(m_msecsPerPixel, m_animationUpdateMSecsPerPixel) > msecsPerPixelChangeThreshold;
    if(!updateNeeded)
        return;

    int stepCount = m_steps.size();

    /* Find maximal index of the time step to use. */
    int maxStepIndex = stepCount - 1;
    qreal tickmarkSpanPixels = size().width() * maxTickmarkSpanFraction;
    for(; maxStepIndex >= 0; maxStepIndex--)
        if(m_steps[maxStepIndex].stepMSecs / m_msecsPerPixel <= tickmarkSpanPixels)
            break;
    maxStepIndex = qMax(maxStepIndex, 1); /* Display at least two tickmark levels. */

    /* Adjust target tickmark heights and opacities. */
    for(int i = 0; i < stepCount; i++) {
        TimeStepData &data = m_stepData[i];
        qreal separationPixels = m_steps[i].stepMSecs / m_msecsPerPixel;

        /* Target height & opacity. */
        qreal targetHeight;
        if (separationPixels < minTickmarkSeparationPixels) {
            targetHeight = 0.0;
        } else if(i >= maxStepIndex) {
            targetHeight = maxTickmarkHeight;
        } else  {
            targetHeight = maxTickmarkHeight * pow(tickmarkStepScale, maxStepIndex - i);
        }
        
        if(!qFuzzyCompare(data.targetHeight, targetHeight)) {
            data.lastTargetHeight = data.targetHeight;
            data.targetHeight = targetHeight;
            data.targetLineOpacity = minTickmarkOpacity + (1.0 - minTickmarkOpacity) * data.targetHeight / maxTickmarkHeight;
        }

        if(separationPixels < minTextSeparationPixels) {
            data.targetTextOpacity = 0.0;
        } else {
            data.targetTextOpacity = qMax(minTextOpacity, data.targetLineOpacity);
        }

        /* Adjust for max height & opacity. */
        qreal maxHeight = qMax(0.0, (separationPixels - criticalTickmarkSeparationPixels) / criticalTickmarkSeparationPixels);
        qreal maxLineOpacity = minTickmarkOpacity + (1.0 - minTickmarkOpacity) * maxHeight;
        qreal maxTextOpacity = qMin(maxLineOpacity, qMax(0.0, (separationPixels - criticalTextSeparationPixels) / criticalTextSeparationPixels));
        data.currentHeight      = qMin(data.currentHeight,      maxHeight);
        data.currentLineOpacity = qMin(data.currentLineOpacity, maxLineOpacity);
        data.currentTextOpacity = qMin(data.currentTextOpacity, maxTextOpacity);
    }

    m_animationUpdateMSecsPerPixel = m_msecsPerPixel;
}

void QnTimeSlider::animateStepValues(int deltaMSecs) {
    int stepCount = m_steps.size();
    qreal dt = deltaMSecs / 1000.0;

    const qreal maxTextHeight = size().height() * tickmarkBarRelativeHeight * (1.0 - maxTickmarkHeight);
    const qreal relativeTextHeightGap = (maxTextHeight - minTextHeight) / maxTickmarkHeight;

    for(int i = 0; i < stepCount; i++) {
        TimeStepData &data = m_stepData[i];

        qreal heightSpeed;
        qreal opacitySpeed;
        if(qFuzzyIsNull(data.targetHeight) || qFuzzyIsNull(data.lastTargetHeight)) {
            heightSpeed = tickmarkHeightStartingAnimationSpeed;
            opacitySpeed = tickmarkHeightStartingAnimationSpeed;
        } else {
            heightSpeed  = qMax(tickmarkHeightAbsoluteAnimationSpeed,  data.currentHeight      * tickmarkHeightRelativeAnimationSpeed);
            opacitySpeed = qMax(tickmarkOpacityAbsoluteAnimationSpeed, data.currentLineOpacity * tickmarkOpacityRelativeAnimationSpeed);
        }

        data.currentHeight      = adjust(data.currentHeight,        data.targetHeight,      heightSpeed * dt);
        data.currentLineOpacity = adjust(data.currentLineOpacity,   data.targetLineOpacity, opacitySpeed * dt);
        data.currentTextOpacity = adjust(data.currentTextOpacity,   data.targetTextOpacity, opacitySpeed * dt);

        data.currentTextHeight  = minTextHeight + qMax(0, qRound(data.currentHeight * relativeTextHeightGap));
    }
}

qreal QnTimeSlider::lineTop(int line) {
    QRectF rect = this->rect();
    return rect.top() + rect.height() * (tickmarkBarRelativeHeight + (1.0 - tickmarkBarRelativeHeight) * line / m_lineCount);
}

qreal QnTimeSlider::lineHeight() {
    return size().height() * (1.0 - tickmarkBarRelativeHeight) / m_lineCount;
}

void QnTimeSlider::updateAggregationValue() {
    /* Aggregate to half-pixels. */
    qreal aggregationMSecs = m_msecsPerPixel / 2.0;
    if(m_aggregationMSecs / 2.0 < aggregationMSecs && aggregationMSecs < m_aggregationMSecs * 2.0)
        return;

    m_aggregationMSecs = aggregationMSecs;
    
    for(int line = 0; line < m_lineCount; line++)
        for(int type = 0; type < Qn::TimePeriodTypeCount; type++)
            updateAggregatedPeriods(line, static_cast<Qn::TimePeriodType>(type));
}

void QnTimeSlider::updateAggregatedPeriods(int line, Qn::TimePeriodType type) {
    m_timePeriods[line].aggregated[type] = QnTimePeriod::aggregateTimePeriods(m_timePeriods[line].normal[type], m_aggregationMSecs);
}


// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnTimeSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QRectF rect = this->rect();
    qreal lineHeight = this->lineHeight();

    /* Draw time periods. */
    for(int line = 0; line < m_lineCount; line++) {
        drawPeriodsBar(
            painter, 
            m_timePeriods[line].aggregated[Qn::RecordingTimePeriod],  
            m_timePeriods[line].aggregated[Qn::MotionTimePeriod], 
            lineTop(line),   
            lineHeight
        );

    }

    /* Draw tickmark background. */
    drawSolidBackground(painter, rect.top(), rect.height() * tickmarkBarRelativeHeight);

    /* Draw selection. */
    drawSelection(painter);

    /* Draw line comments. */
    for(int line = 0; line < m_lineCount; line++) {
        QPixmap pixmap = m_lineCommentPixmaps[line];
        QRectF textRect(
            rect.left(),
            lineTop(line) + 0.5 * lineHeight * (1.0 - lineCommentRelativeHeight),
            pixmap.width(),
            pixmap.height()
        );

        painter->drawPixmap(textRect, pixmap, pixmap.rect());
    }

    /* Draw tickmarks. */
    drawTickmarks(painter, rect.top(), rect.height() * tickmarkBarRelativeHeight);

    /* Draw position marker. */
    drawMarker(painter, sliderPosition(), positionMarkerColor);
}

void QnTimeSlider::drawSelection(QPainter *painter) {
    if(!(m_options & SelectionVisible))
        return;

    if(m_selectionStart == m_selectionEnd) {
        drawMarker(painter, m_selectionStart, selectionMarkerColor);
        return;
    }

    qint64 selectionStart = qMax(m_selectionStart, m_windowStart);
    qint64 selectionEnd = qMin(m_selectionEnd, m_windowEnd);
    if(selectionStart < selectionEnd) {
        QRectF rect = this->rect();
        QRectF selectionRect(
            QPointF(positionFromValue(selectionStart).x(), rect.top()),
            QPointF(positionFromValue(selectionEnd).x(), rect.bottom())
        );

        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
        painter->fillRect(selectionRect, selectionColor);
    }

    drawMarker(painter, m_selectionStart, selectionMarkerColor);
    drawMarker(painter, m_selectionEnd, selectionMarkerColor);
}

void QnTimeSlider::drawMarker(QPainter *painter, qint64 pos, const QColor &color) {
    if(pos < m_windowStart || pos > m_windowEnd) 
        return;
    
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
    QnScopedPainterPenRollback penRollback(painter, QPen(color, 0));

    qreal x = positionFromValue(pos).x();
    QRectF rect = this->rect();

    painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
}

void QnTimeSlider::drawPeriodsBar(QPainter *painter, QnTimePeriodList &recorded, QnTimePeriodList &motion, qreal top, qreal height) {
    qint64 minimumValue = this->windowStart();
    qint64 maximumValue = this->windowEnd();
    qreal centralPos = positionFromValue(this->sliderPosition()).x();

    QnTimePeriodList periods[Qn::TimePeriodTypeCount] = {recorded, motion};
    QColor pastColor[Qn::TimePeriodTypeCount + 1] = {pastRecordingColor, pastMotionColor, pastBackgroundColor};
    QColor futureColor[Qn::TimePeriodTypeCount + 1] = {futureRecordingColor, futureMotionColor, futureBackgroundColor};

    QnTimePeriodList::const_iterator pos[Qn::TimePeriodTypeCount];
    QnTimePeriodList::const_iterator end[Qn::TimePeriodTypeCount];
    for(int i = 0; i < Qn::TimePeriodTypeCount; i++) {
         pos[i] = periods[i].findNearestPeriod(minimumValue, false);
         end[i] = periods[i].findNearestPeriod(maximumValue, true);
         if(end[i] != periods[i].end() && end[i]->containTime(maximumValue))
             end[i]++;
    }

    qint64 value = minimumValue;

    bool inside[Qn::TimePeriodTypeCount];
    for(int i = 0; i < Qn::TimePeriodTypeCount; i++)
        inside[i] = pos[i] == end[i] ? false : pos[i]->containTime(value);

    while(value != maximumValue) {
        qint64 nextValue[Qn::TimePeriodTypeCount] = {maximumValue, maximumValue};
        for(int i = 0; i < Qn::TimePeriodTypeCount; i++) {
            if(pos[i] == end[i]) 
                continue;
            
            if(!inside[i]) {
                nextValue[i] = pos[i]->startTimeMs;
                continue;
            }
            
            if(pos[i]->durationMs != -1)
                nextValue[i] = pos[i]->startTimeMs + pos[i]->durationMs;
        }

        qint64 bestValue = qMin(nextValue[0], nextValue[1]);
        
        int bestIndex;
        if(inside[Qn::MotionTimePeriod]) {
            bestIndex = Qn::MotionTimePeriod;
        } else if(inside[Qn::RecordingTimePeriod]) {
            bestIndex = Qn::RecordingTimePeriod;
        } else {
            bestIndex = Qn::TimePeriodTypeCount;
        }

        qreal leftPos = positionFromValue(value).x();
        qreal rightPos = positionFromValue(bestValue).x();
        if(rightPos <= centralPos) {
            painter->fillRect(QRectF(leftPos, top, rightPos - leftPos, height), pastColor[bestIndex]);
        } else if(leftPos >= centralPos) {
            painter->fillRect(QRectF(leftPos, top, rightPos - leftPos, height), futureColor[bestIndex]);
        } else {
            painter->fillRect(QRectF(leftPos, top, centralPos - leftPos, height), pastColor[bestIndex]);
            painter->fillRect(QRectF(centralPos, top, rightPos - centralPos, height), futureColor[bestIndex]);
        }
        
        for(int i = 0; i < Qn::TimePeriodTypeCount; i++) {
            if(bestValue != nextValue[i])
                continue;

            if(inside[i])
                pos[i]++;
            inside[i] = !inside[i];
        }

        value = bestValue;
    }
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
    if(end != periods.end() && end->containTime(maximumValue))
        end++;

    for (QnTimePeriodList::const_iterator pos = begin; pos < end; ++pos) {
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

void QnTimeSlider::drawSolidBackground(QPainter *painter, qreal top, qreal height) {
    qreal leftPos = positionFromValue(windowStart()).x();
    qreal rightPos = positionFromValue(windowEnd()).x();
    qreal centralPos = positionFromValue(sliderPosition()).x();

    if(!qFuzzyCompare(leftPos, centralPos))
        painter->fillRect(QRectF(leftPos, top, centralPos - leftPos, height), pastBackgroundColor);
    if(!qFuzzyCompare(rightPos, centralPos))
        painter->fillRect(QRectF(centralPos, top, rightPos - centralPos, height), futureBackgroundColor);
}

void QnTimeSlider::drawTickmarks(QPainter *painter, qreal top, qreal height) {
    int stepCount = m_steps.size();
    QRectF rect = this->rect();

#if 0
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
#endif

    /* Find minimal tickmark step index. */
    int minStepIndex = 0;
    for(; minStepIndex < stepCount; minStepIndex++)
        if(!qFuzzyIsNull(m_stepData[minStepIndex].currentHeight))
            break;

    /* Initialize next positions for tickmark steps. */
    for(int i = minStepIndex; i < stepCount; i++)
        m_nextTickmarkPos[i] = roundUp(m_windowStart, m_steps[i]);


    /* Draw tickmarks. */
    qreal bottom = top + height;
    
    for(int i = 0; i < m_tickmarkLines.size(); i++)
        m_tickmarkLines[i].clear();

    while(true) {
        qint64 pos = m_nextTickmarkPos[minStepIndex];
        if(pos > m_windowEnd)
            break;

        /* Find index of the step to use. */
        int index = minStepIndex;
        for(; index < stepCount; index++) {
            if(m_nextTickmarkPos[index] == pos) {
                m_nextTickmarkPos[index] = add(pos, m_steps[index]);
            } else {
                break;
            }
        }
        index--;

        qreal x = positionFromValue(pos).x();

        /* Draw label if needed. */
        qreal tickmarkHeight = height * m_stepData[index].currentHeight;
        if(!qFuzzyIsNull(m_stepData[index].currentTextOpacity)) {
            QPixmap pixmap = positionPixmap(pos, m_stepData[index].currentTextHeight, m_steps[index]);

            QRectF textRect(x - pixmap.width() / 2.0, top + tickmarkHeight, pixmap.width(), pixmap.height());
            if(textRect.left() < rect.left())
                textRect.moveLeft(rect.left());
            if(textRect.right() > rect.right())
                textRect.moveRight(rect.right());

            QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * m_stepData[index].currentTextOpacity);
            painter->drawPixmap(textRect, pixmap, pixmap.rect());
        }

        /* Calculate line ends. */
        m_tickmarkLines[index] << 
            QPointF(x, top) <<
            QPointF(x, top + tickmarkHeight);
    }

    /* Draw tickmarks. */
    {
        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
        for(int i = minStepIndex; i < stepCount; i++) {
            painter->setPen(toTransparent(tickmarkColor, m_stepData[i].currentLineOpacity));
            painter->drawLines(m_tickmarkLines[i]);
        }
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnTimeSlider::tick(int deltaMSecs) {
    animateStepValues(deltaMSecs);
}

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

        /* Re-bound. */
        setWindow(m_windowStart, m_windowEnd);
        setSelection(m_selectionStart, m_selectionEnd);
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
    if(change == ItemSceneHasChanged) {
        AnimationTimer *timer = InstrumentManager::animationTimerOf(scene());

        setTimer(timer);
        kineticProcessor()->setTimer(timer);
    }
    
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

void QnTimeSlider::resizeEvent(QGraphicsSceneResizeEvent *event) {
    base_type::resizeEvent(event);

    updateMSecsPerPixel();
    updateLineCommentPixmaps();
    updateMinimalWindow();
}

void QnTimeSlider::kineticMove(const QVariant &degrees) {
    qreal factor = std::pow(2.0, -degrees.toReal() / degreesFor2x);
    
    if(!scaleWindow(factor, m_zoomAnchor))
        kineticProcessor()->reset();
}

void QnTimeSlider::keyPressEvent(QKeyEvent *event) {
    switch(event->key()) {
    case Qt::Key_BracketLeft:
    case Qt::Key_BracketRight:
        if(!(m_options & SelectionEditable)) {
            base_type::keyPressEvent(event);
        } else {
            if(event->key() == Qt::Key_BracketLeft) {
                setSelectionStart(sliderPosition());
            } else {
                setSelectionEnd(sliderPosition());
            }
        }
    default:
        base_type::keyPressEvent(event);
        break;
    }
}