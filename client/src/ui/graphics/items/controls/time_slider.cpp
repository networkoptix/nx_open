#include "time_slider.h"

#include <cassert>

#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>

#include <QtGui/QPainter>
#include <QtGui/QGraphicsSceneWheelEvent>

#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <ui/common/color_transformations.h>
#include <ui/common/scene_utility.h>
#include <ui/style/noptix_style.h>
#include <ui/style/globals.h>
#include <ui/graphics/items/standard/graphics_slider_p.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/processors/kinetic_cutting_processor.h>
#include "camera/thumbnails_loader.h"

#include "tool_tip_item.h"
#include "time_slider_pixmap_cache.h"

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

    qreal speed(qreal progress, qreal center, qreal starting, qreal relative) {
        /* Speed of tickmark height animation depends on tickmark height.
         * 
         * The goal is for animation of different tickmark groups to start and
         * end at the same moment, even if the height changes in these groups are 
         * different. */
        if(progress > center) {
            return progress * relative;
        } else {
            return (relative - starting / center) * progress + starting;
        }
    }

    class TimeStepFactory: public QnTimeSlider {
    public:
        using QnTimeSlider::createAbsoluteSteps;
        using QnTimeSlider::createRelativeSteps;
    };
    Q_GLOBAL_STATIC_WITH_ARGS(QVector<QnTimeStep>, absoluteTimeSteps, (TimeStepFactory::createAbsoluteSteps()));
    Q_GLOBAL_STATIC_WITH_ARGS(QVector<QnTimeStep>, relativeTimeSteps, (TimeStepFactory::createRelativeSteps()));

    /* Note that most numbers below are given relative to time slider size. */



    /* Tickmark bar. */

    /** Minimal distance between tickmarks from the same group for this group to be used as toplevel one. */
    const qreal topLevelTickmarkStep = 0.75;

    /** Minimal distance between tickmarks from the same group for this group to be visible. 
     * Note that because of the fact that tickmarks do not disappear instantly, in some cases
     * step may become smaller that this value. */
    const qreal minTickmarkLineStepPixels = 5.0;

    /** Critical distance between tickmarks from the same group. 
     * Tickmarks that are closer to each other will never be displayed. */
    const qreal criticalTickmarkLineStepPixels = 2.0;

    /** Minimal distance between tickmarks from the same group for text labels to be visible for this group. */
    const qreal minTickmarkTextStepPixels = 30.0;

    /** Critical distance between tickmarks from the same group.
     * Text labels will never be displayed for tickmarks that are closer to each other. */
    const qreal criticalTickmarkTextStepPixels = 20.0;
    
    /** Minimal opacity for a tickmark that is visible. */
    const qreal minTickmarkLineOpacity = 0.05;

    /** Minimal opacity for a tickmark text label that is visible. */
    const qreal minTickmarkTextOpacity = 0.5;
    
    /** Ratio between the sizes of consequent tickmark groups. */
    const qreal tickmarkStepScale = 2.0 / 3.0;

    /** Minimal height of a tickmark text. */
    const int minTickmarkTextHeightPixels = 9;
    
    /** Maximal tickmark height, relative to the height of the tickmark bar. */
    const qreal maxTickmarkHeight = 1.0;

    /** Minimal tickmark height, relative to the height of the tickmark bar. */
    const qreal minTickmarkHeight = 0.1;

    /** Height of a tickmark text, relative to tickmark height. */
    const qreal tickmarkTextHeight = 1.0 / 3.0;

    /** Height of a tickmark line, relative to tickmark height. */
    const qreal tickmarkLineHeight = 2.0 / 3.0;

    /* Tickmark height animation parameters. */
    const qreal tickmarkHeightRelativeAnimationSpeed = 1.0;
    const qreal tickmarkHeightCentralAnimationValue = minTickmarkHeight;
    const qreal tickmarkHeightStartingAnimationSpeed = 0.5;

    /* Tickmark opacity animation parameters. */
    const qreal tickmarkOpacityRelativeAnimationSpeed = 1.0;
    const qreal tickmarkOpacityCentralAnimationValue = 0.5;
    const qreal tickmarkOpacityStartingAnimationSpeed = 1.0;



    /* Date bar. */
    const qreal dateTextTopMargin = 0.0;
    const qreal dateTextBottomMargin = 0.1;

    const qreal minDateSpanFraction = 0.15;
    const qreal minDateSpanPixels = 120;


    
    /* Lines bar. */

    const qreal lineCommentTopMargin = 0.0;
    const qreal lineCommentBottomMargin = 0.05;



    /* Global */

    /** Minimal relative change of msecs-per-pixel value of a time slider for animation parameters to be recalculated.
     * This value was introduced so that the parameters are not recalculated constantly when changes are small. */
    const qreal msecsPerPixelChangeThreshold = 1.0e-4;
    
    /** Lower limit on time slider scale. */
    const qreal minMSecsPerPixel = 2.0;

    const QRectF dateBarPosition = QRectF(0.0, 0.0, 1.0, 0.2);

    const QRectF tickmarkBarPosition = QRectF(0.0, 0.2, 1.0, 0.5);

    const QRectF lineBarPosition = QRectF(0.0, 0.7, 1.0, 0.3);

    /** Maximal number of lines in a time slider. */
    const int maxLines = 16;

    const qreal degreesFor2x = 180.0;

    const qreal zoomSideSnapDistance = 0.075;



    /* Colors. */
    
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

    const QColor separatorColor(255, 255, 255, 64);

    const QColor dateOverlayColorA(255, 255, 255, 48);
    const QColor dateOverlayColorB = withAlpha(selectionColor, 48);


    bool checkLine(int line) {
        if(line < 0 || line >= maxLines) {
            qnWarning("Invalid line number '%1'.", line);
            return false;
        } else {
            return true;
        }
    }

    bool checkLinePeriod(int line, Qn::TimePeriodType type) {
        if(!checkLine(line))
            return false;

        if(type < 0 || type >= Qn::TimePeriodTypeCount) {
            qnWarning("Invalid time period type '%1'.", static_cast<int>(type));
            return false;
        } else {
            return true;
        }
    }

    QRectF positionRect(const QRectF &sourceRect, const QRectF &position) {
        QRectF result = SceneUtility::cwiseMul(position, sourceRect.size());
        result.moveTopLeft(result.topLeft() + sourceRect.topLeft());
        return result;
    }


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
    m_minimalWindow(0),
    m_selectionValid(false),
    m_pixmapCache(QnTimeSliderPixmapCache::instance()),
    m_unzooming(false)
{
    /* Set default property values. */
    setProperty(Qn::SliderLength, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding, QSizePolicy::Slider);

    setWindowStart(minimum());
    setWindowEnd(maximum());
    setOptions(StickToMinimum | StickToMaximum | UpdateToolTip | SelectionEditable);

    setToolTipFormat(tr("hh:mm:ss", "DEFAULT_TOOL_TIP_FORMAT"));

    /* Set default vector sizes. */
    m_lineCommentPixmaps.resize(maxLines);
    m_lineComments.resize(maxLines);
    m_lineTimePeriods.resize(maxLines);

    /* Prepare kinetic zoom processors. */
    KineticCuttingProcessor *processor = new KineticCuttingProcessor(QMetaType::QReal, this);
    processor->setHandler(this);
    processor->setMaxShiftInterval(0.4);
    processor->setFriction(degreesFor2x / 2);
    processor->setMaxSpeedMagnitude(degreesFor2x * 8);
    processor->setSpeedCuttingThreshold(degreesFor2x / 3);
    processor->setFlags(KineticProcessor::IGNORE_DELTA_TIME);
    registerAnimation(processor);

    /* Prepare animation timer listener. */
    startListening();
    registerAnimation(this);

    /* Run handlers. */
    updateSteps();
    updateMinimalWindow();
    sliderChange(SliderRangeChange);
}

QnTimeSlider::~QnTimeSlider() {
    return;
}

QVector<QnTimeStep> QnTimeSlider::createAbsoluteSteps() {
    QVector<QnTimeStep> result;
    result <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                10,     1000,   tr("ms"),       tr("10ms"),         QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                50,     1000,   tr("ms"),       tr("50ms"),         QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                100,    1000,   tr("ms"),       tr("100ms"),        QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                500,    1000,   tr("ms"),       tr("500ms"),        QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             1,      60,     tr("s"),        tr("59s"),          QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             5,      60,     tr("s"),        tr("59s"),          QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             10,     60,     tr("s"),        tr("59s"),          QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             30,     60,     tr("s"),        tr("59s"),          QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        1,      60,     tr("m"),        tr("59m"),          tr("dd MMMM yyyy hh:mm ap"), false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        5,      60,     tr("m"),        tr("59m"),          QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        10,     60,     tr("m"),        tr("59m"),          QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        30,     60,     tr("m"),        tr("59m"),          QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   1,      24,     tr("h"),        tr("23h"),          tr("dd MMMM yyyy h ap"), false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   3,      24,     tr("h"),        tr("23h"),          QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   12,     24,     tr("h"),        tr("23h"),          QString(),          false) <<
        QnTimeStep(QnTimeStep::Days,            1000ll * 60 * 60 * 24,              1,      31,     tr("dd MMM"),   tr("29 Mar"),       tr("dd MMMM yyyy"), false) <<
        QnTimeStep(QnTimeStep::Months,          1000ll * 60 * 60 * 24 * 31,         1,      12,     tr("MMMM"),     tr("September"),    tr("MMMM yyyy"),    false) <<
        QnTimeStep(QnTimeStep::Years,           1000ll * 60 * 60 * 24 * 365,        1,      50000,  tr("yyyy"),     tr("2000"),         tr("yyyy"),         false);
    return enumerateSteps(result);
}

QVector<QnTimeStep> QnTimeSlider::createRelativeSteps() {
    QVector<QnTimeStep> result;
    result <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                10,     1000,   tr("ms"),       tr("10ms"),         QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                50,     1000,   tr("ms"),       tr("50ms"),         QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                100,    1000,   tr("ms"),       tr("100ms"),        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                500,    1000,   tr("ms"),       tr("500ms"),        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             1,      60,     tr("s"),        tr("59s"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             5,      60,     tr("s"),        tr("59s"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             10,     60,     tr("s"),        tr("59s"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             30,     60,     tr("s"),        tr("59s"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        1,      60,     tr("m"),        tr("59m"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        5,      60,     tr("m"),        tr("59m"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        10,     60,     tr("m"),        tr("59m"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        30,     60,     tr("m"),        tr("59m"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   1,      24,     tr("h"),        tr("23h"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   3,      24,     tr("h"),        tr("23h"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   12,     24,     tr("h"),        tr("23h"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24,              1,      31,     tr("d"),        tr("29d"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30,         1,      12,     tr("M"),        tr("11M"),          QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30 * 12,    1,      50000,  tr("y"),        tr("2000y"),        QString(),          true);
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

    if(lineCount < 0 || lineCount > maxLines) {
        qnWarning("Invalid line count '%1'.", lineCount);
        return;
    }

    m_lineCount = lineCount;

    updateLineCommentPixmaps();
}

void QnTimeSlider::setLineComment(int line, const QString &comment) {
    if(!checkLine(line))
        return;

    if(m_lineComments[line] == comment)
        return;

    m_lineComments[line] = comment;

    updateLineCommentPixmap(line);
}

QString QnTimeSlider::lineComment(int line) {
    if(!checkLine(line))
        return QString();

    return m_lineComments[line];
}

QnTimePeriodList QnTimeSlider::timePeriods(int line, Qn::TimePeriodType type) const {
    if(!checkLinePeriod(line, type))
        return QnTimePeriodList();
    
    return m_lineTimePeriods[line].normal[type];
}

void QnTimeSlider::setTimePeriods(int line, Qn::TimePeriodType type, const QnTimePeriodList &timePeriods) {
    if(!checkLinePeriod(line, type))
        return;

    m_lineTimePeriods[line].normal[type] = timePeriods;
    
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

bool QnTimeSlider::isSelectionValid() const {
    return m_selectionValid;
}

void QnTimeSlider::setSelectionValid(bool valid) {
    m_selectionValid = valid;
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

QnThumbnailsLoader *QnTimeSlider::thumbnailsLoader() const {
    return m_thumbnailsLoader.data();
}

void QnTimeSlider::setThumbnailsLoader(QnThumbnailsLoader *loader) {
    m_thumbnailsLoader = loader;
}

QPointF QnTimeSlider::positionFromValue(qint64 logicalValue, bool bound) const {
    Q_D(const GraphicsSlider);

    d->ensureMapper();

    return QPointF(
        d->pixelPosMin + GraphicsStyle::sliderPositionFromValue(m_windowStart, m_windowEnd, logicalValue, d->pixelPosMax - d->pixelPosMin, d->upsideDown, bound), 
        0.0
    );
}

qint64 QnTimeSlider::valueFromPosition(const QPointF &position, bool bound) const {
    Q_D(const GraphicsSlider);

    d->ensureMapper();

    return GraphicsStyle::sliderValueFromPosition(m_windowStart, m_windowEnd, position.x(), d->pixelPosMax - d->pixelPosMin, d->upsideDown, bound);
}

void QnTimeSlider::finishAnimations() {
    animateStepValues(10 * 1000);

    if(m_unzooming) {
        setWindow(minimum(), maximum());
        m_unzooming = false;
    }

    kineticProcessor()->reset();
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
    QRectF lineBarRect = positionRect(rect(), lineBarPosition);

    m_lineCommentPixmaps[line] = m_pixmapCache->textPixmap(
        m_lineComments[line],
        qRound(lineBarRect.height() / m_lineCount * (1.0 - lineCommentTopMargin - lineCommentBottomMargin)),
        palette().color(QPalette::WindowText)
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
    qreal tickmarkSpanPixels = size().width() * topLevelTickmarkStep;
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
        if (separationPixels < minTickmarkLineStepPixels) {
            targetHeight = 0.0;
        } else if(i >= maxStepIndex) {
            targetHeight = maxTickmarkHeight;
        } else  {
            targetHeight = minTickmarkHeight + (maxTickmarkHeight - minTickmarkHeight) * pow(tickmarkStepScale, maxStepIndex - i);
        }
        
        if(!qFuzzyCompare(data.targetHeight, targetHeight)) {
            data.targetHeight = targetHeight;

            if(qFuzzyIsNull(targetHeight)) {
                data.targetLineOpacity = 0.0;
            } else {
                data.targetLineOpacity = minTickmarkLineOpacity + (1.0 - minTickmarkLineOpacity) * (data.targetHeight - minTickmarkHeight) / (maxTickmarkHeight - minTickmarkHeight);
            }
        }

        if(separationPixels < minTickmarkTextStepPixels) {
            data.targetTextOpacity = 0.0;
        } else {
            data.targetTextOpacity = qMax(minTickmarkTextOpacity, data.targetLineOpacity);
        }

        /* Adjust for max height & opacity. */
        qreal maxHeight = qMax(0.0, (separationPixels - criticalTickmarkLineStepPixels) / criticalTickmarkLineStepPixels);
        qreal maxLineOpacity = minTickmarkLineOpacity + (1.0 - minTickmarkLineOpacity) * maxHeight;
        qreal maxTextOpacity = qMin(maxLineOpacity, qMax(0.0, (separationPixels - criticalTickmarkTextStepPixels) / criticalTickmarkTextStepPixels));
        data.currentHeight      = qMin(data.currentHeight,      maxHeight);
        data.currentLineOpacity = qMin(data.currentLineOpacity, maxLineOpacity);
        data.currentTextOpacity = qMin(data.currentTextOpacity, maxTextOpacity);
    }

    m_animationUpdateMSecsPerPixel = m_msecsPerPixel;
}

void QnTimeSlider::animateStepValues(int deltaMSecs) {
    int stepCount = m_steps.size();
    qreal dt = deltaMSecs / 1000.0;

    QRectF tickmarkBarRect = positionRect(rect(), tickmarkBarPosition);
    const qreal maxTickmarkTextHeightPixels = tickmarkBarRect.height() * maxTickmarkHeight * tickmarkTextHeight;

    /* Range of valid text height values. */
    const qreal textHeightRangePixels = qMax(0.0, maxTickmarkTextHeightPixels - minTickmarkTextHeightPixels);

    for(int i = 0; i < stepCount; i++) {
        TimeStepData &data = m_stepData[i];

        qreal heightSpeed  = speed(data.currentHeight,      0.5, tickmarkHeightStartingAnimationSpeed,  tickmarkHeightRelativeAnimationSpeed);
        qreal opacitySpeed = speed(data.currentLineOpacity, 0.5, tickmarkOpacityStartingAnimationSpeed, tickmarkOpacityRelativeAnimationSpeed);

        data.currentHeight      = adjust(data.currentHeight,        data.targetHeight,      heightSpeed * dt);
        data.currentLineOpacity = adjust(data.currentLineOpacity,   data.targetLineOpacity, opacitySpeed * dt);
        data.currentTextOpacity = adjust(data.currentTextOpacity,   data.targetTextOpacity, opacitySpeed * dt);

        data.currentTextHeight  = minTickmarkTextHeightPixels + qRound(data.currentHeight * textHeightRangePixels);
        data.currentLineHeight  = qMin(data.currentHeight * tickmarkBarRect.height() * tickmarkLineHeight, tickmarkBarRect.height() - data.currentTextHeight - 1.0);
    }
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
    m_lineTimePeriods[line].aggregated[type] = QnTimePeriod::aggregateTimePeriods(m_lineTimePeriods[line].normal[type], m_aggregationMSecs);
}


// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnTimeSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
    sendPendingMouseMoves(widget);

    QRectF rect = this->rect();
    QRectF dateBarRect = positionRect(rect, dateBarPosition);
    QRectF lineBarRect = positionRect(rect, lineBarPosition);
    QRectF tickmarkBarRect = positionRect(rect, tickmarkBarPosition);
    qreal lineHeight = lineBarRect.height() / m_lineCount;

    /* Draw border. */
    {
        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
        QnScopedPainterPenRollback penRollback(painter, QPen(separatorColor, 0));
        QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
        painter->drawRect(rect);
    }

    /* Draw background. */
    if(m_lineCount == 0) {
        drawSolidBackground(painter, rect);
    } else {
        drawSolidBackground(painter, tickmarkBarRect);
        drawSolidBackground(painter, dateBarRect);

        /* Draw lines background (that is, time periods). */
        for(int line = 0; line < m_lineCount; line++) {
            QRectF lineRect(lineBarRect.left(), lineBarRect.top() + line * lineHeight, lineBarRect.width(), lineHeight);

            drawPeriodsBar(
                painter, 
                m_lineTimePeriods[line].aggregated[Qn::RecordingTimePeriod],  
                m_lineTimePeriods[line].aggregated[Qn::MotionTimePeriod], 
                lineRect
            );

            drawSeparator(painter, lineRect);
        }
    }

    /* Draw selection. */
    drawSelection(painter);

    /* Draw line comments. */
    for(int line = 0; line < m_lineCount; line++) {
        const QPixmap *pixmap = m_lineCommentPixmaps[line];
        QRectF textRect(
            lineBarRect.left(),
            lineBarRect.top() + lineHeight * line + lineHeight * lineCommentTopMargin,
            pixmap->width(),
            pixmap->height()
        );

        painter->drawPixmap(textRect, *pixmap, pixmap->rect());
    }

    /* Draw tickmarks. */
    drawTickmarks(painter, tickmarkBarRect);
    drawSeparator(painter, tickmarkBarRect);

    /* Draw highlights. */
    drawDates(painter, dateBarRect);

    /* Draw position marker. */
    drawMarker(painter, sliderPosition(), positionMarkerColor);

    /* Draw thumbnails. */
    drawThumbnails(painter, QRectF(rect.left(), rect.top() - thumbnailsHeight(), rect.width(), thumbnailsHeight()));
}

int QnTimeSlider::thumbnailsHeight() const
{
    return rect().height();
}

void QnTimeSlider::drawSeparator(QPainter *painter, const QRectF &rect) {
    if(qFuzzyCompare(rect.top(), this->rect().top()))
        return; /* Don't draw separator at the top of the widget. */

    QnScopedPainterPenRollback penRollback(painter, QPen(separatorColor, 0));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
    painter->drawLine(rect.topLeft(), rect.topRight());
}

void QnTimeSlider::drawSelection(QPainter *painter) {
    if(!m_selectionValid)
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

void QnTimeSlider::drawPeriodsBar(QPainter *painter, QnTimePeriodList &recorded, QnTimePeriodList &motion, const QRectF &rect) {
    qint64 minimumValue = this->windowStart();
    qint64 maximumValue = this->windowEnd();
    qreal centralPos = positionFromValue(this->sliderPosition()).x();

    /* The code here may look complicated, but it takes care of not rendering
     * different motion periods several times over the same location. 
     * It makes transparent time slider look better. */

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
            painter->fillRect(QRectF(leftPos, rect.top(), rightPos - leftPos, rect.height()), pastColor[bestIndex]);
        } else if(leftPos >= centralPos) {
            painter->fillRect(QRectF(leftPos, rect.top(), rightPos - leftPos, rect.height()), futureColor[bestIndex]);
        } else {
            painter->fillRect(QRectF(leftPos, rect.top(), centralPos - leftPos, rect.height()), pastColor[bestIndex]);
            painter->fillRect(QRectF(centralPos, rect.top(), rightPos - centralPos, rect.height()), futureColor[bestIndex]);
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

void QnTimeSlider::drawSolidBackground(QPainter *painter, const QRectF &rect) {
    qreal leftPos = positionFromValue(windowStart()).x();
    qreal rightPos = positionFromValue(windowEnd()).x();
    qreal centralPos = positionFromValue(sliderPosition()).x();

    if(!qFuzzyCompare(leftPos, centralPos))
        painter->fillRect(QRectF(leftPos, rect.top(), centralPos - leftPos, rect.height()), pastBackgroundColor);
    if(!qFuzzyCompare(rightPos, centralPos))
        painter->fillRect(QRectF(centralPos, rect.top(), rightPos - centralPos, rect.height()), futureBackgroundColor);
}

void QnTimeSlider::drawTickmarks(QPainter *painter, const QRectF &rect) {
    int stepCount = m_steps.size();

    /* Find minimal tickmark step index. */
    int minStepIndex = 0;
    for(; minStepIndex < stepCount; minStepIndex++)
        if(!qFuzzyIsNull(m_stepData[minStepIndex].currentHeight))
            break;

    /* Initialize next positions for tickmark steps. */
    for(int i = minStepIndex; i < stepCount; i++)
        m_nextTickmarkPos[i] = roundUp(m_windowStart, m_steps[i]);


    /* Draw tickmarks. */
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
        qreal lineHeight = m_stepData[index].currentLineHeight;
        if(!qFuzzyIsNull(m_stepData[index].currentTextOpacity)) {
            const QPixmap *pixmap = m_pixmapCache->positionShortPixmap(pos, m_stepData[index].currentTextHeight, m_steps[index]);

            QRectF textRect(x - pixmap->width() / 2.0, rect.top() + lineHeight, pixmap->width(), pixmap->height());
            if(textRect.left() < rect.left())
                textRect.moveLeft(rect.left());
            if(textRect.right() > rect.right())
                textRect.moveRight(rect.right());

            QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * m_stepData[index].currentTextOpacity);
            painter->drawPixmap(textRect, *pixmap, pixmap->rect());
        }

        /* Calculate line ends. */
        m_tickmarkLines[index] << 
            QPointF(x, rect.top() + 1.0 /* To prevent antialiased lines being drawn outside provided rect. */) <<
            QPointF(x, rect.top() + lineHeight);
    }

    /* Draw tickmarks. */
    {
        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
        for(int i = minStepIndex; i < stepCount; i++) {
            painter->setPen(toTransparent(tickmarkColor, m_stepData[i].currentLineOpacity));
            painter->drawLines(m_tickmarkLines[i]);
        }
    }
}

void QnTimeSlider::drawDates(QPainter *painter, const QRectF &rect) {
    int stepCount = m_steps.size();

    /* Find index of the highlight time step. */
    int highlightIndex = 0;
    qreal highlightSpanPixels = qMax(size().width() * minDateSpanFraction, minDateSpanPixels);
    for(; highlightIndex < stepCount; highlightIndex++)
        if(!m_steps[highlightIndex].longFormat.isEmpty() && m_steps[highlightIndex].stepMSecs / m_msecsPerPixel >= highlightSpanPixels)
            break;
    highlightIndex = qMin(highlightIndex, stepCount - 1); // TODO: remove this line.
    const QnTimeStep &highlightStep = m_steps[highlightIndex];

    /* Do some precalculations. */
    const qreal textHeight = rect.height() * (1.0 - dateTextTopMargin - dateTextBottomMargin);
    const qreal textTopMargin = rect.height() * dateTextTopMargin;

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);

    /* Draw highlight. */
    qint64 pos1 = roundUp(m_windowStart, highlightStep);
    qint64 pos0 = sub(pos1, highlightStep);
    qreal x0 = positionFromValue(m_windowStart).x();
    qint64 number = absoluteNumber(pos1, highlightStep);
    while(true) {
        qreal x1 = positionFromValue(pos1).x();

        painter->setPen(Qt::NoPen);
        painter->setBrush(number % 2 ? dateOverlayColorA : dateOverlayColorB);
        painter->drawRect(QRectF(x0, rect.top(), x1 - x0, rect.height()));

        const QPixmap *pixmap = m_pixmapCache->positionLongPixmap(pos0, textHeight, highlightStep);

        qreal x = (x0 + x1) / 2.0;
        QRectF textRect(x - pixmap->width() / 2.0, rect.top() + textTopMargin, pixmap->width(), pixmap->height());
        QRectF pixmapRect(pixmap->rect());
        if(textRect.left() < rect.left()) {
            textRect.moveRight(x1);
            pixmapRect.setLeft(pixmapRect.left() + rect.left() - textRect.left());
            textRect.setLeft(rect.left());
        }
        if(textRect.right() > rect.right()) {
            textRect.moveLeft(x0);
            pixmapRect.setRight(pixmapRect.right() + rect.right() - textRect.right());
            textRect.setRight(rect.right());
        }

        painter->drawPixmap(textRect, *pixmap, pixmapRect);

        if(pos1 >= m_windowEnd)
            break;

        painter->setPen(QPen(separatorColor, 0));
        painter->drawLine(QPointF(x1, rect.top()), QPointF(x1, rect.bottom()));

        pos0 = pos1;
        pos1 = add(pos1, highlightStep);
        number++;
        x0 = x1;
    }
}

void QnTimeSlider::drawThumbnails(QPainter *painter, const QRectF &rect) {
    QnThumbnailsLoader *loader = m_thumbnailsLoader.data();
    if (!loader)
        return;
    if (loader->step() == 0)
        return;

    qint64 currentTime = valueFromPosition(rect.topLeft());
    qint64 endTime = valueFromPosition(rect.topRight());

    currentTime = qFloor(currentTime, loader->step());

    for (; currentTime < endTime; currentTime += loader->step()) {
        loader->lockPixmaps();
        const QPixmap *pixmap = loader->getPixmapByTime(currentTime);
        if (pixmap) {
            QPointF pos = positionFromValue(currentTime, false);
            painter->drawRect(pos.x(), rect.top(), pixmap->size().width(), pixmap->size().height());
            painter->drawPixmap(QPointF(pos.x(), rect.top()), *pixmap);
        }
        loader->unlockPixmaps();
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnTimeSlider::tick(int deltaMSecs) {
    if(!m_unzooming) {
        animateStepValues(deltaMSecs);
    } else {
        qreal sliderRange = maximum() - minimum();
        qreal windowRange = m_windowEnd - m_windowStart;

        if(!qFuzzyCompare(sliderRange, windowRange)) {
            qreal progress = windowRange / sliderRange;
            qreal speed = qMin(4.0 - 3.0 * progress, progress * 16.0);
            qreal delta = speed * deltaMSecs / 1000.0 * sliderRange;

            qreal startFraction = (m_windowStart - minimum()) / (sliderRange - windowRange);
            qreal endFraction  = (maximum() - m_windowEnd) / (sliderRange - windowRange);

            setWindow(
                m_windowStart - static_cast<qint64>(delta * startFraction),
                m_windowEnd + static_cast<qint64>(delta * endFraction)
            );
        }

        if(minimum() == m_windowStart && maximum() == m_windowEnd)
            m_unzooming = false;

        animateStepValues(8.0 * deltaMSecs);
    }
}

void QnTimeSlider::sliderChange(SliderChange change) {
    base_type::sliderChange(change);

    switch(change) {
    case SliderRangeChange: {
        qint64 windowStart = m_windowStart;
        if((m_options & StickToMinimum) && windowStart == m_oldMinimum)
            windowStart = minimum();

        qint64 windowEnd = m_windowEnd;
        if((m_options & StickToMaximum) && windowEnd == m_oldMaximum)
            windowEnd = maximum();

        /* Stick zoom anchor. */
        if(m_zoomAnchor == m_oldMinimum)
            m_zoomAnchor = minimum();
        if(m_zoomAnchor == m_oldMaximum)
            m_zoomAnchor = maximum();

        m_oldMinimum = minimum();
        m_oldMaximum = maximum();

        /* Re-bound. */
        setWindow(windowStart, windowEnd);
        setSelection(m_selectionStart, m_selectionEnd);
        break;
    }
    case SliderValueChange:
        updateToolTipVisibility();
        updateToolTipText();
        break;
    default:
        break;
    }
}

void QnTimeSlider::wheelEvent(QGraphicsSceneWheelEvent *event) {
    if(m_unzooming) {
        event->accept();
        return; /* Do nothing if animated unzoom is in progress. */
    }

    /* delta() returns the distance that the wheel is rotated 
     * in eighths (1/8s) of a degree. */
    qreal degrees = event->delta() / 8.0;

    m_zoomAnchor = valueFromPosition(event->pos());
    
    /* Snap zoom anchor to window sides. */
    qreal windowRange = m_windowEnd - m_windowStart;
    if((m_zoomAnchor - m_windowStart) / windowRange < zoomSideSnapDistance) {
        m_zoomAnchor = m_windowStart;
    } else if((m_windowEnd - m_zoomAnchor) / windowRange < zoomSideSnapDistance) {
        m_zoomAnchor = m_windowEnd;
    }

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
            if(!isSelectionValid()) {
                setSelection(sliderPosition(), sliderPosition());
                setSelectionValid(true);
            } else {
                if(event->key() == Qt::Key_BracketLeft) {
                    setSelectionStart(sliderPosition());
                } else {
                    setSelectionEnd(sliderPosition());
                }
            }
        }
        break;
    default:
        base_type::keyPressEvent(event);
        break;
    }
}

void QnTimeSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    base_type::mouseDoubleClickEvent(event);

    if(event->button() == Qt::LeftButton) {
        kineticProcessor()->reset(); /* Stop kinetic zoom. */
        m_unzooming = true;
    }
}

