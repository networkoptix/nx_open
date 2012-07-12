#include "time_slider.h"

#include <cassert>
#include <limits>

#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/qmath.h>

#include <QtGui/QPainter>
#include <QtGui/QGraphicsSceneWheelEvent>

#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

#include <camera/thumbnails_loader.h>

#include <ui/common/color_transformations.h>
#include <ui/common/geometry.h>
#include <ui/style/noptix_style.h>
#include <ui/style/globals.h>
#include <ui/graphics/items/standard/graphics_slider_p.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/processors/kinetic_cutting_processor.h>
#include <ui/processors/drag_processor.h>

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
    const qreal minTickmarkTextStepPixels = 40.0;

    /** Critical distance between tickmarks from the same group.
     * Text labels will never be displayed for tickmarks that are closer to each other. */
    const qreal criticalTickmarkTextStepPixels = 25.0;
    
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



    /* Dates bar. */

    const qreal dateTextTopMargin = 0.0;
    const qreal dateTextBottomMargin = 0.1;

    const qreal minDateSpanFraction = 0.15;
    const qreal minDateSpanPixels = 160;


    
    /* Lines bar. */

    const qreal lineCommentTopMargin = -0.20;
    const qreal lineCommentBottomMargin = 0.05;


    /* Thumbnails bar. */

    const qreal thumbnailHeightForDrawing = 10.0;



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

    const qreal hoverEffectDistance = 5.0;

    const int startDragDistance = 5;



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

    bool checkLinePeriod(int line, Qn::TimePeriodRole type) {
        if(!checkLine(line))
            return false;

        if(type < 0 || type >= Qn::TimePeriodRoleCount) {
            qnWarning("Invalid time period type '%1'.", static_cast<int>(type));
            return false;
        } else {
            return true;
        }
    }

    QRectF positionRect(const QRectF &sourceRect, const QRectF &position) {
        QRectF result = QnGeometry::cwiseMul(position, sourceRect.size());
        result.moveTopLeft(result.topLeft() + sourceRect.topLeft());
        return result;
    }

    void drawCroppedPixmap(QPainter *painter, const QRectF &target, const QRectF &cropTarget, const QPixmap &pixmap, const QRectF &source, QRectF *drawnTarget = NULL) {
        MarginsF targetMargins(
            qMax(0.0, cropTarget.left() - target.left()),
            qMax(0.0, cropTarget.top() - target.top()),
            qMax(0.0, target.right() - cropTarget.right()),
            qMax(0.0, target.bottom() - cropTarget.bottom())
        );

        QRectF erodedTarget;

        if(targetMargins.isNull()) {
            erodedTarget = target;
        } else {
            erodedTarget = QnGeometry::eroded(target, targetMargins);
        } 

        if(!erodedTarget.isValid()) {
            erodedTarget = QRectF();
        } else {
            MarginsF sourceMargins = QnGeometry::cwiseMul(QnGeometry::cwiseDiv(targetMargins, target.size()), source.size());
            QRectF erodedSource = QnGeometry::eroded(source, sourceMargins);

            painter->drawPixmap(erodedTarget, pixmap, erodedSource);
        }

        if(drawnTarget)
            *drawnTarget = erodedTarget;
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
    m_unzooming(false),
    m_dragIsClick(false),
    m_selecting(false),
    m_dragMarker(NoMarker),
	m_lineCount(0),
    m_totalLineStretch(0.0),
    m_rulerHeight(0.0),
    m_prefferedHeight(0.0),
    m_lastThumbnailsUpdateTime(0),
    m_lastHoverThumbnail(-1)
{
    m_noThumbnailsPixmap = m_pixmapCache->textPixmap(tr("NO THUMBNAILS\nAVAILABLE"), 16, QColor(255, 255, 255, 255));

    /* Prepare thumbnail update timer. */
    m_thumbnailsUpdateTimer = new QTimer(this);
    connect(m_thumbnailsUpdateTimer, SIGNAL(timeout()), this, SLOT(updateThumbnailsStepSizeTimer()));

    /* Set default vector sizes. */
    m_lineData.resize(maxLines);

    /* Prepare kinetic zoom kineticProcessor. */
    KineticCuttingProcessor *kineticProcessor = new KineticCuttingProcessor(QMetaType::QReal, this);
    kineticProcessor->setHandler(this);
    kineticProcessor->setMaxShiftInterval(0.4);
    kineticProcessor->setFriction(degreesFor2x / 2);
    kineticProcessor->setMaxSpeedMagnitude(degreesFor2x * 8);
    kineticProcessor->setSpeedCuttingThreshold(degreesFor2x / 3);
    kineticProcessor->setFlags(KineticProcessor::IGNORE_DELTA_TIME);
    registerAnimation(kineticProcessor);

    /* Prepare zoom kineticProcessor. */
    DragProcessor *dragProcessor = new DragProcessor(this);
    dragProcessor->setHandler(this);
    dragProcessor->setFlags(DragProcessor::DONT_COMPRESS);
    dragProcessor->setStartDragDistance(startDragDistance);

    /* Prepare animation timer listener. */
    startListening();
    registerAnimation(this);

    /* Set default property values. */
    setAcceptHoverEvents(true);
    setProperty(Qn::SliderLength, 0);
    setProperty(Qn::NoHandScrollOver, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred, QSizePolicy::Slider);

    setWindowStart(minimum());
    setWindowEnd(maximum());
    setOptions(StickToMinimum | StickToMaximum | UpdateToolTip | SelectionEditable | AdjustWindowToPosition | SnapZoomToSides | UnzoomOnDoubleClick);

    setToolTipFormat(tr("hh:mm:ss", "DEFAULT_TOOL_TIP_FORMAT"));
    setRulerHeight(60.0);

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
        QnTimeStep(QnTimeStep::Years,           1000ll * 60 * 60 * 24 * 365,        1,      50000,  tr("yyyy"),     tr("2000"),         tr("yyyy"),         false) <<
        QnTimeStep(QnTimeStep::Years,           1000ll * 60 * 60 * 24 * 365,        5,      50000,  tr("yyyy"),     tr("2000"),         QString(),         false) <<
        QnTimeStep(QnTimeStep::Years,           1000ll * 60 * 60 * 24 * 365,        10,     50000,  tr("yyyy"),     tr("2000"),         tr("yyyy"),         false);
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
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30 * 12,    1,      50000,  tr("y"),        tr("2000y"),        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30 * 12,    5,      50000,  tr("y"),        tr("2000y"),        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30 * 12,    10,     50000,  tr("y"),        tr("2000y"),        QString(),          true);
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
}

void QnTimeSlider::setLineVisible(int line, bool visible) {
    if(!checkLine(line))
        return;

    if(m_lineData[line].visible == visible)
        return;

    m_lineData[line].visible = visible;

    updateTotalLineStretch();
}

bool QnTimeSlider::isLineVisible(int line) const {
    if(!checkLine(line))
        return false;

    return m_lineData[line].visible;
}

void QnTimeSlider::setLineStretch(int line, qreal stretch) {
    if(!checkLine(line))
        return;
    
    if(qFuzzyCompare(m_lineData[line].stretch, stretch))
        return;

    m_lineData[line].stretch = stretch;

    updateTotalLineStretch();
}

qreal QnTimeSlider::lineStretch(int line) const {
    if(!checkLine(line))
        return 0.0;

    return m_lineData[line].stretch;
}

void QnTimeSlider::setLineComment(int line, const QString &comment) {
    if(!checkLine(line))
        return;

    if(m_lineData[line].comment == comment)
        return;

    m_lineData[line].comment = comment;

    updateLineCommentPixmap(line);
}

QString QnTimeSlider::lineComment(int line) {
    if(!checkLine(line))
        return QString();

    return m_lineData[line].comment;
}

QnTimePeriodList QnTimeSlider::timePeriods(int line, Qn::TimePeriodRole type) const {
    if(!checkLinePeriod(line, type))
        return QnTimePeriodList();
    
    return m_lineData[line].normalPeriods[type];
}

void QnTimeSlider::setTimePeriods(int line, Qn::TimePeriodRole type, const QnTimePeriodList &timePeriods) {
    if(!checkLinePeriod(line, type))
        return;

    m_lineData[line].normalPeriods[type] = timePeriods;
    
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
        qint64 delta = (m_minimalWindow - (end - start) + 1) / 2;
        start -= delta;
        end += delta;

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
        updateThumbnailsPeriod();
    }
}

bool QnTimeSlider::windowContains(qint64 position) {
    return m_windowStart <= position && position <= m_windowEnd;
}

void QnTimeSlider::ensureWindowContains(qint64 position) {
    qint64 d = 0;

    if(position < m_windowStart) {
        d = position - m_windowStart;
    } else if(m_windowEnd < position) {
        d = position - m_windowEnd;
    } else {
        return;
    }

    setWindow(m_windowStart + d, m_windowEnd + d);
}

void QnTimeSlider::setSliderPosition(qint64 position, bool keepInWindow) {
    if(!keepInWindow) {
        return setSliderPosition(position);
    } else {
        bool inWindow = windowContains(sliderPosition());
        setSliderPosition(position);
        if(inWindow)
            ensureWindowContains(sliderPosition());
    }
}

void QnTimeSlider::setValue(qint64 value, bool keepInWindow) {
    if(!keepInWindow) {
        return setValue(value);
    } else {
        bool inWindow = windowContains(this->value());
        setValue(value);
        if(inWindow)
            ensureWindowContains(this->value());
    }
}

qint64 QnTimeSlider::getValue() const {
    return value();
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
    if(m_thumbnailsLoader.data() == loader)
        return;

    clearThumbnails();
    m_oldThumbnailData.clear();
    m_thumbnailsUpdateTimer->stop();

    if(m_thumbnailsLoader)
        disconnect(m_thumbnailsLoader.data(), NULL, this, NULL);

    m_thumbnailsLoader = loader;

    if(m_thumbnailsLoader) {
        connect(m_thumbnailsLoader.data(), SIGNAL(sourceSizeChanged()), this, SLOT(updateThumbnailsStepSizeForced()));
        connect(m_thumbnailsLoader.data(), SIGNAL(thumbnailsInvalidated()), this, SLOT(clearThumbnails()));
        connect(m_thumbnailsLoader.data(), SIGNAL(thumbnailLoaded(const QnThumbnail &)), this, SLOT(addThumbnail(const QnThumbnail &)));
    }

    updateThumbnailsPeriod();
    updateThumbnailsStepSize(true);

    if(m_thumbnailsLoader)
        foreach(const QnThumbnail &thumbnail, loader->thumbnails())
            addThumbnail(thumbnail);
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

qreal QnTimeSlider::quickPositionFromValue(qint64 logicalValue, bool bound) const {
    if(bound) {
        return m_boundMapper(logicalValue);
    } else {
        return m_unboundMapper(logicalValue);
    }
}

void QnTimeSlider::finishAnimations() {
    animateStepValues(10 * 1000);
    animateThumbnails(10 * 1000);

    if(m_unzooming) {
        setWindow(minimum(), maximum());
        m_unzooming = false;
    }

    kineticProcessor()->reset();
}

void QnTimeSlider::animatedUnzoom() {
    kineticProcessor()->reset(); /* Stop kinetic zoom. */
    m_unzooming = true;
}

bool QnTimeSlider::isAnimatingWindow() const {
    return m_unzooming || kineticProcessor()->isRunning();
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

QnTimeSlider::Marker QnTimeSlider::markerFromPosition(const QPointF &pos, qreal maxDistance) const {
    if(m_selectionValid) {
        QPointF selectionStart = positionFromValue(m_selectionStart);
        if(qAbs(selectionStart.x() - pos.x()) < maxDistance)
            return SelectionStartMarker;
        
        QPointF selectionEnd = positionFromValue(m_selectionEnd);
        if(qAbs(selectionEnd.x() - pos.x()) < maxDistance)
            return SelectionEndMarker;
    }

    return NoMarker;
}

QPointF QnTimeSlider::positionFromMarker(Marker marker) const {
    switch(marker) {
    case SelectionStartMarker:
        if(!m_selectionValid) {
            return rect().topLeft();
        } else {
            return positionFromValue(m_selectionStart);
        }
    case SelectionEndMarker:
        if(!m_selectionValid) {
            return rect().topLeft();
        } else {
            return positionFromValue(m_selectionEnd);
        }
    default:
        return rect().topLeft();
    }
}

void QnTimeSlider::setMarkerSliderPosition(Marker marker, qint64 position) {
    switch(marker) {
    case SelectionStartMarker:
        setSelectionStart(position);
        break;
    case SelectionEndMarker:
        setSelectionEnd(position);
        break;
    default:
        break;
    }
}

qreal QnTimeSlider::effectiveLineStretch(int line) const {
    return m_lineData[line].visible ? m_lineData[line].stretch : 0.0;
}

QRectF QnTimeSlider::thumbnailsRect() const {
    QRectF rect = this->rect();
    return QRectF(rect.left(), rect.top(), rect.width(), thumbnailsHeight());
}

QRectF QnTimeSlider::rulerRect() const {
    QRectF rect = this->rect();
    return QRectF(rect.left(), rect.top() + thumbnailsHeight(), rect.width(), rulerHeight());
}

qreal QnTimeSlider::thumbnailsHeight() const {
    return size().height() - m_rulerHeight;
}

qreal QnTimeSlider::rulerHeight() const {
    return m_rulerHeight;
}

void QnTimeSlider::setRulerHeight(qreal rulerHeight) {
    if(qFuzzyCompare(m_rulerHeight, rulerHeight))
        return;

    m_prefferedHeight = m_prefferedHeight + (rulerHeight - m_rulerHeight);
    m_rulerHeight = rulerHeight;

    updateGeometry();
    updateThumbnailsStepSize(false);
}

void QnTimeSlider::addThumbnail(const QnThumbnail &thumbnail) {
    if(m_thumbnailsUpdateTimer->isActive())
        return;

    if(m_thumbnailData.contains(thumbnail.time())) 
        return; /* There is no real point in overwriting existing thumbnails. Besides, it would result in flicker. */
 
    m_thumbnailData[thumbnail.time()] = ThumbnailData(thumbnail);
}

void QnTimeSlider::clearThumbnails() {
    m_thumbnailData.clear();
}

void QnTimeSlider::freezeThumbnails() {
    m_oldThumbnailData = m_thumbnailData.values();
    clearThumbnails();

    qreal center = m_thumbnailsPaintRect.center().x();
    qreal height = m_thumbnailsPaintRect.height();

    for(int i = 0; i < m_oldThumbnailData.size(); i++) {
        ThumbnailData &data = m_oldThumbnailData[i];
        data.hiding = true;

        qreal pos = quickPositionFromValue(data.thumbnail.time(), false);
        qreal width = data.thumbnail.size().width() * height / data.thumbnail.size().height();
        data.pos = (pos - center) / width;
    }
}



// -------------------------------------------------------------------------- //
// Updating
// -------------------------------------------------------------------------- //
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
    QRectF lineBarRect = positionRect(rulerRect(), lineBarPosition);

    int height = qFloor(lineBarRect.height() * m_lineData[line].stretch / m_totalLineStretch * (1.0 - lineCommentTopMargin - lineCommentBottomMargin));
    if(height <= 0)
        return;

    m_lineData[line].commentPixmap = m_pixmapCache->textPixmap(
        m_lineData[line].comment,
        height,
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

    updateThumbnailsStepSize(false);
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

    QRectF tickmarkBarRect = positionRect(rulerRect(), tickmarkBarPosition);
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

void QnTimeSlider::animateThumbnails(int deltaMSecs) {
    qreal dt = deltaMSecs / 1000.0;

    bool oldShown = false;
    for(QList<ThumbnailData>::iterator pos = m_oldThumbnailData.begin(); pos != m_oldThumbnailData.end(); pos++)
        oldShown |= animateThumbnail(dt, *pos);
    
    if(!oldShown) {
        m_oldThumbnailData.clear();

        for(QMap<qint64, ThumbnailData>::iterator pos = m_thumbnailData.begin(); pos != m_thumbnailData.end(); pos++)
            animateThumbnail(dt, *pos);
    }
}

bool QnTimeSlider::animateThumbnail(qreal dt, ThumbnailData &data) {
    if(data.selecting && !data.hiding) {
        data.selection = qMin(1.0, data.selection + dt * 4.0);
    } else {
        data.selection = qMax(0.0, data.selection - dt * 4.0);
    }

    if(data.hiding) {
        data.opacity = qMax(0.0, data.opacity - dt * 4.0);

        return !qFuzzyIsNull(data.opacity);
    } else {
        data.opacity = qMin(1.0, data.opacity + dt * 4.0);

        return true;
    }
}

void QnTimeSlider::updateAggregationValue() {
    /* Aggregate to half-pixels. */
    qreal aggregationMSecs = m_msecsPerPixel / 2.0;
    if(m_aggregationMSecs / 2.0 < aggregationMSecs && aggregationMSecs < m_aggregationMSecs * 2.0)
        return;

    m_aggregationMSecs = aggregationMSecs;
    
    for(int line = 0; line < m_lineCount; line++)
        for(int type = 0; type < Qn::TimePeriodRoleCount; type++)
            updateAggregatedPeriods(line, static_cast<Qn::TimePeriodRole>(type));
}

void QnTimeSlider::updateAggregatedPeriods(int line, Qn::TimePeriodRole type) {
    m_lineData[line].aggregatedPeriods[type] = QnTimePeriod::aggregateTimePeriods(m_lineData[line].normalPeriods[type], m_aggregationMSecs);
}

void QnTimeSlider::updateTotalLineStretch() {
    qreal totalLineStretch = 0.0;
    for(int line = 0; line < m_lineCount; line++)
        totalLineStretch += effectiveLineStretch(line);
    
    if(qFuzzyCompare(m_totalLineStretch, totalLineStretch))
        return;
    m_totalLineStretch = totalLineStretch;

    updateLineCommentPixmaps();
}

void QnTimeSlider::updateThumbnailsPeriod() {
    if (!thumbnailsLoader())
        return;

    if(m_thumbnailsUpdateTimer->isActive() || !m_oldThumbnailData.isEmpty())
        return;

    thumbnailsLoader()->setTimePeriod(m_windowStart, m_windowEnd);
}

void QnTimeSlider::updateThumbnailsStepSizeLater() {
    m_thumbnailsUpdateTimer->start(250); /* Re-start it if it's active. */
}

void QnTimeSlider::updateThumbnailsStepSizeForced() {
    m_thumbnailsUpdateTimer->stop();
    updateThumbnailsStepSize(true, true);
}

void QnTimeSlider::updateThumbnailsStepSizeTimer() {
    m_thumbnailsUpdateTimer->stop();
    updateThumbnailsStepSize(true);
}

void QnTimeSlider::updateThumbnailsStepSize(bool instant, bool forced) {
    if (!thumbnailsLoader())
        return; /* Nothing to update. */

    if (m_thumbnailsUpdateTimer->isActive()) {
        updateThumbnailsStepSizeLater(); /* Re-start the timer. */
        return;
    }
    
    /* Calculate new bounding size. */
    int boundingHeigth = qRound(thumbnailsHeight());
    QSize boundingSize = QSize(boundingHeigth * 256, boundingHeigth);
    bool boundingSizeChanged = thumbnailsLoader()->boundingSize() != boundingSize;

    /* Calculate actual thumbnail size. */
    QSize size = thumbnailsLoader()->thumbnailSize();
    if(size.isEmpty()) {
        size = QSize(boundingHeigth * 4 / 3, boundingHeigth); /* For uninitialized loader, assume 4:3 aspect ratio. */
    } else if(size.height() != boundingHeigth) { // TODO: evil hack, should work on signals.
        size = QSize(boundingHeigth * size.width() / size.height() , boundingHeigth);
    }
    if(size.isEmpty())
        return;

    /* Calculate new time step. */
    qint64 timeStep = m_msecsPerPixel * size.width();
    bool timeStepChanged = qAbs(timeStep / m_msecsPerPixel - thumbnailsLoader()->timeStep() / m_msecsPerPixel) >= 1;

    /* Nothing changed? Leave. */
    if(!timeStepChanged && !boundingSizeChanged)
        return;

    /* Ok, thumbnails have to be re-generated. So we first freeze our old thumbnails. */
    if(m_oldThumbnailData.isEmpty())
        freezeThumbnails();

    /* If animation is running, we want to wait until it's finished. 
     * We also don't want to update thumbnails too often. */
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if((!instant || isAnimatingWindow() || currentTime - m_lastThumbnailsUpdateTime < 1000) && !forced) {
        updateThumbnailsStepSizeLater();
    } else {
        m_lastThumbnailsUpdateTime = currentTime;
        thumbnailsLoader()->setBoundingSize(boundingSize);
        thumbnailsLoader()->setTimeStep(timeStep);
        updateThumbnailsPeriod();
    }
}

void QnTimeSlider::setThumbnailSelecting(qint64 time, bool selecting) {
    if(time < 0)
        return;

    QMap<qint64, ThumbnailData>::iterator pos = m_thumbnailData.find(time);
    if(pos == m_thumbnailData.end())
        return;

    qint64 actualTime = pos->thumbnail.actualTime();

    QMap<qint64, ThumbnailData>::iterator ipos;
    for(ipos = pos; ipos->thumbnail.actualTime() == actualTime; ipos--) {
        ipos->selecting = selecting;
        if(ipos == m_thumbnailData.begin())
            break;
    }
    for(ipos = pos + 1; ipos != m_thumbnailData.end() && ipos->thumbnail.actualTime() == actualTime; ipos++)
        ipos->selecting = selecting;
}


// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnTimeSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
    sendPendingMouseMoves(widget);

    m_unboundMapper = QnLinearFunction(m_windowStart, positionFromValue(m_windowStart).x(), m_windowEnd, positionFromValue(m_windowEnd).x());
    m_boundMapper = QnBoundedLinearFunction(m_unboundMapper, rect().left(), rect().right());

    QRectF thumbnailsRect = this->thumbnailsRect();
    QRectF rulerRect = this->rulerRect();
    QRectF dateBarRect = positionRect(rulerRect, dateBarPosition);
    QRectF lineBarRect = positionRect(rulerRect, lineBarPosition);
    QRectF tickmarkBarRect = positionRect(rulerRect, tickmarkBarPosition);
    
    qreal lineTop, lineUnit = qFuzzyIsNull(m_totalLineStretch) ? 0.0 : lineBarRect.height() / m_totalLineStretch;

    /* Draw background. */
    if(qFuzzyIsNull(m_totalLineStretch)) {
        drawSolidBackground(painter, this->rect());
    } else {
        drawSolidBackground(painter, thumbnailsRect);
        drawSolidBackground(painter, tickmarkBarRect);
        drawSolidBackground(painter, dateBarRect);

        /* Draw lines background (that is, time periods). */
        lineTop = lineBarRect.top();
        for(int line = 0; line < m_lineCount; line++) {
            qreal lineHeight = lineUnit * effectiveLineStretch(line);
            if(qFuzzyIsNull(lineHeight))
                continue;

            QRectF lineRect(lineBarRect.left(), lineTop, lineBarRect.width(), lineHeight);

            drawPeriodsBar(
                painter, 
                m_lineData[line].aggregatedPeriods[Qn::RecordingRole],  
                m_lineData[line].aggregatedPeriods[Qn::MotionRole], 
                lineRect
            );

            drawSeparator(painter, lineRect);
            
            lineTop += lineHeight;
        }
    }

    /* Draw thumbnails. */
    drawThumbnails(painter, QnGeometry::eroded(thumbnailsRect, MarginsF(1.0, 1.0, 1.0, 1.0))); /* Rect is eroded so that thumbnails don't get painted under borders. */

    /* Draw separators. */
    if(!thumbnailsRect.isEmpty())
        drawSeparator(painter, thumbnailsRect);
    drawSeparator(painter, tickmarkBarRect);
    drawSeparator(painter, dateBarRect);

    /* Draw border. */
    {
        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
        QnScopedPainterPenRollback penRollback(painter, QPen(separatorColor, 0));
        QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
        painter->drawRect(rect());
    }

    /* Draw selection. */
    drawSelection(painter);

    /* Draw line comments. */
    lineTop = lineBarRect.top();
    for(int line = 0; line < m_lineCount; line++) {
        qreal lineHeight = lineUnit * effectiveLineStretch(line);
        if(qFuzzyIsNull(lineHeight))
            continue;

        QPixmap pixmap = m_lineData[line].commentPixmap;
        QRectF textRect(
            lineBarRect.left(),
            lineTop + lineHeight * lineCommentTopMargin,
            pixmap.width(),
            pixmap.height()
        );

        painter->drawPixmap(textRect, pixmap, pixmap.rect());

        lineTop += lineHeight;
    }

    /* Draw tickmarks. */
    drawTickmarks(painter, tickmarkBarRect);

    /* Draw dates. */
    drawDates(painter, dateBarRect);

    /* Draw position marker. */
    drawMarker(painter, sliderPosition(), positionMarkerColor);
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
            QPointF(quickPositionFromValue(selectionStart), rect.top()),
            QPointF(quickPositionFromValue(selectionEnd), rect.bottom())
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

    qreal x = quickPositionFromValue(pos);
    QRectF rect = this->rect();

    painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
}

void QnTimeSlider::drawPeriodsBar(QPainter *painter, QnTimePeriodList &recorded, QnTimePeriodList &motion, const QRectF &rect) {
    qint64 minimumValue = this->windowStart();
    qint64 maximumValue = this->windowEnd();
    qreal centralPos = quickPositionFromValue(this->sliderPosition());

    /* The code here may look complicated, but it takes care of not rendering
     * different motion periods several times over the same location. 
     * It makes transparent time slider look better. */

    QnTimePeriodList periods[Qn::TimePeriodRoleCount] = {recorded, motion};
    QColor pastColor[Qn::TimePeriodRoleCount + 1] = {pastRecordingColor, pastMotionColor, pastBackgroundColor};
    QColor futureColor[Qn::TimePeriodRoleCount + 1] = {futureRecordingColor, futureMotionColor, futureBackgroundColor};

    QnTimePeriodList::const_iterator pos[Qn::TimePeriodRoleCount];
    QnTimePeriodList::const_iterator end[Qn::TimePeriodRoleCount];
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++) {
         pos[i] = periods[i].findNearestPeriod(minimumValue, false);
         end[i] = periods[i].findNearestPeriod(maximumValue, true);
         if(end[i] != periods[i].end() && end[i]->containTime(maximumValue))
             end[i]++;
    }

    qint64 value = minimumValue;

    bool inside[Qn::TimePeriodRoleCount];
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++)
        inside[i] = pos[i] == end[i] ? false : pos[i]->containTime(value);

    while(value != maximumValue) {
        qint64 nextValue[Qn::TimePeriodRoleCount] = {maximumValue, maximumValue};
        for(int i = 0; i < Qn::TimePeriodRoleCount; i++) {
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
        if(inside[Qn::MotionRole]) {
            bestIndex = Qn::MotionRole;
        } else if(inside[Qn::RecordingRole]) {
            bestIndex = Qn::RecordingRole;
        } else {
            bestIndex = Qn::TimePeriodRoleCount;
        }

        qreal leftPos = quickPositionFromValue(value);
        qreal rightPos = quickPositionFromValue(bestValue);
        if(rightPos <= centralPos) {
            painter->fillRect(QRectF(leftPos, rect.top(), rightPos - leftPos, rect.height()), pastColor[bestIndex]);
        } else if(leftPos >= centralPos) {
            painter->fillRect(QRectF(leftPos, rect.top(), rightPos - leftPos, rect.height()), futureColor[bestIndex]);
        } else {
            painter->fillRect(QRectF(leftPos, rect.top(), centralPos - leftPos, rect.height()), pastColor[bestIndex]);
            painter->fillRect(QRectF(centralPos, rect.top(), rightPos - centralPos, rect.height()), futureColor[bestIndex]);
        }
        
        for(int i = 0; i < Qn::TimePeriodRoleCount; i++) {
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
    qreal leftPos = quickPositionFromValue(windowStart());
    qreal rightPos = quickPositionFromValue(windowEnd());
    qreal centralPos = quickPositionFromValue(sliderPosition());

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

    /* Find initial and maximal positions. */
    QPointF overlap(criticalTickmarkTextStepPixels / 2.0, 0.0);
    qint64 startPos = valueFromPosition(positionFromValue(m_windowStart) - overlap, false);
    qint64 endPos = valueFromPosition(positionFromValue(m_windowEnd) + overlap, false);

    /* Initialize next positions for tickmark steps. */
    for(int i = minStepIndex; i < stepCount; i++)
        m_nextTickmarkPos[i] = roundUp(startPos, m_steps[i]);

    /* Draw tickmarks. */
    for(int i = 0; i < m_tickmarkLines.size(); i++)
        m_tickmarkLines[i].clear();

    while(true) {
        qint64 pos = m_nextTickmarkPos[minStepIndex];
        if(pos > endPos)
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

        qreal x = quickPositionFromValue(pos, false);

        /* Draw label if needed. */
        qreal lineHeight = m_stepData[index].currentLineHeight;
        if(!qFuzzyIsNull(m_stepData[index].currentTextOpacity)) {
            QPixmap pixmap = m_pixmapCache->positionShortPixmap(pos, m_stepData[index].currentTextHeight, m_steps[index]);
            QRectF textRect(x - pixmap.width() / 2.0, rect.top() + lineHeight, pixmap.width(), pixmap.height());

            QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * m_stepData[index].currentTextOpacity);
            drawCroppedPixmap(painter, textRect, rect, pixmap, pixmap.rect());
        }

        /* Calculate line ends. */
        if(pos >= m_windowStart && pos <= m_windowEnd) {
            m_tickmarkLines[index] << 
                QPointF(x, rect.top() + 1.0 /* To prevent antialiased lines being drawn outside provided rect. */) <<
                QPointF(x, rect.top() + lineHeight);
        }
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
    qreal x0 = quickPositionFromValue(m_windowStart);
    qint64 number = absoluteNumber(pos1, highlightStep);
    while(true) {
        qreal x1 = quickPositionFromValue(pos1);

        painter->setPen(Qt::NoPen);
        painter->setBrush(number % 2 ? dateOverlayColorA : dateOverlayColorB);
        painter->drawRect(QRectF(x0, rect.top(), x1 - x0, rect.height()));

        QPixmap pixmap = m_pixmapCache->positionLongPixmap(pos0, textHeight, highlightStep);

        QRectF textRect((x0 + x1) / 2.0 - pixmap.width() / 2.0, rect.top() + textTopMargin, pixmap.width(), pixmap.height());
        if(textRect.left() < rect.left())
            textRect.moveRight(x1);
        if(textRect.right() > rect.right())
            textRect.moveLeft(x0);

        drawCroppedPixmap(painter, textRect, rect, pixmap, pixmap.rect());

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
    if (rect.height() < thumbnailHeightForDrawing)
        return;

    m_thumbnailsPaintRect = rect;

    if (!thumbnailsLoader()) {
        QSizeF labelSizeBound = rect.size();
        labelSizeBound.setHeight(m_noThumbnailsPixmap.height());

        QRectF labelRect = QnGeometry::aligned(QnGeometry::expanded(QnGeometry::aspectRatio(m_noThumbnailsPixmap.size()), labelSizeBound, Qt::KeepAspectRatio), rect, Qt::AlignCenter);
        drawCroppedPixmap(painter, labelRect, rect, m_noThumbnailsPixmap, m_noThumbnailsPixmap.rect());
        return;
    }

    qint64 step = thumbnailsLoader()->timeStep();
    if (thumbnailsLoader()->timeStep() <= 0)
        return;

    qreal aspectRatio = QnGeometry::aspectRatio(thumbnailsLoader()->thumbnailSize());
    qreal thumbnailWidth = rect.height() * aspectRatio;

    if(!m_oldThumbnailData.isEmpty() || m_thumbnailsUpdateTimer->isActive()) {
        QRectF boundingRect = rect; 
        for (int i = 0; i < m_oldThumbnailData.size(); i++) {
            const ThumbnailData &data = m_oldThumbnailData[i];
            if(data.thumbnail.isEmpty())
                continue;

            qreal x = rect.width() / 2 + data.pos * thumbnailWidth;;
            QSizeF targetSize(data.thumbnail.aspectRatio() * rect.height(), rect.height());
            QRectF targetRect(x - targetSize.width() / 2, rect.top(), targetSize.width(), targetSize.height());

            drawThumbnail(painter, data, targetRect, boundingRect);

            boundingRect.setLeft(qMax(boundingRect.left(), targetRect.right()));
        }
    } else {
        QnScopedPainterPenRollback penRollback(painter);
        QnScopedPainterBrushRollback brushRollback(painter);

        qint64 startTime = qFloor(m_windowStart, step);
        qint64 endTime = qCeil(m_windowEnd, step);

        QRectF boundingRect = rect; 
        for (qint64 time = startTime; time <= endTime; time += step) {
            QMap<qint64, ThumbnailData>::iterator pos = m_thumbnailData.find(time);
            if(pos == m_thumbnailData.end())
                continue;

            ThumbnailData &data = *pos;
            if(data.thumbnail.isEmpty())
                continue;

            qreal x = quickPositionFromValue(time, false);
            QSizeF targetSize(data.thumbnail.aspectRatio() * rect.height(), rect.height());
            QRectF targetRect(x - targetSize.width() / 2, rect.top(), targetSize.width(), targetSize.height());

            drawThumbnail(painter, data, targetRect, boundingRect);

            boundingRect.setLeft(qMax(boundingRect.left(), targetRect.right()));
        }
    }
}

void QnTimeSlider::drawThumbnail(QPainter *painter, const ThumbnailData &data, const QRectF &targetRect, const QRectF &boundingRect) {
    const QPixmap &pixmap = data.thumbnail.pixmap();

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * data.opacity);

    QRectF rect;
    drawCroppedPixmap(painter, targetRect, boundingRect, pixmap, pixmap.rect(), &rect);

    if(!rect.isEmpty()) {
        qreal a = data.selection;
        qreal width = 1.0 + a * 2.0;
        QColor color = linearCombine(1.0 - a, QColor(255, 255, 255, 32), a, selectionMarkerColor);
        rect = QnGeometry::eroded(rect, width / 2.0);

        painter->setPen(QPen(color, width));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect);

        qreal x = quickPositionFromValue(data.thumbnail.actualTime());
        if(x >= rect.left() && x <= rect.right()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(withAlpha(color, (color.alpha() + 255) / 2));
            painter->drawEllipse(QPointF(x, rect.bottom()), width * 2, width * 2);
        }
    }
    painter->setOpacity(opacity);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
QSizeF QnTimeSlider::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    switch(which) {
    case Qt::MinimumSize: {
        QSizeF result = base_type::sizeHint(which, constraint);
        result.setHeight(m_rulerHeight);
        return result;
    }
    case Qt::PreferredSize: {
        QSizeF result = base_type::sizeHint(which, constraint);
        result.setHeight(m_prefferedHeight);
        return result;
    }
    default:
        return base_type::sizeHint(which, constraint);
    }
}

void QnTimeSlider::tick(int deltaMSecs) {
    if((m_options & AdjustWindowToPosition) && !m_unzooming && !isSliderDown()) {
        /* Apply window position corrections if no animation or user interaction is in progress. */
        qint64 position = this->sliderPosition();
        if(position >= m_windowStart && position <= m_windowEnd) {
            qint64 windowRange = m_windowEnd - m_windowStart;

            qreal speed;
            if(position < m_windowStart + windowRange / 3) {
                speed = -1.0 + static_cast<qreal>(position - m_windowStart) / (windowRange / 3);
            } else if(position < m_windowEnd - windowRange / 3) {
                speed = 0.0;
            } else {
                speed = 1.0 - static_cast<qreal>(m_windowEnd - position) / (windowRange / 3);
            }
            speed = speed * qMax(0.5 * windowRange, 32 * 1000.0);

            qint64 delta = speed * deltaMSecs / 1000.0;
            if(m_windowEnd + delta > maximum())
                delta = maximum() - m_windowEnd;
            if(m_windowStart + delta < minimum())
                delta = minimum() - m_windowStart;

            setWindow(m_windowStart + delta, m_windowEnd + delta);
        }
    }

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

    animateThumbnails(deltaMSecs);
}

void QnTimeSlider::sliderChange(SliderChange change) {
    base_type::sliderChange(change);

    switch(change) {
    case SliderRangeChange: {
        qint64 windowStart = m_windowStart;
        qint64 windowEnd = m_windowEnd;

        if((m_options & StickToMaximum) && windowEnd == m_oldMaximum) {
            windowStart += maximum() - windowEnd;
            windowEnd = maximum();
        }

        if((m_options & StickToMinimum) && windowStart == m_oldMinimum) {
            windowEnd += minimum() - windowStart;
            windowStart = minimum();
        }

        /* Stick zoom anchor. */
        if(m_zoomAnchor == m_oldMinimum)
            m_zoomAnchor = minimum();
        if(m_zoomAnchor == m_oldMaximum)
            m_zoomAnchor = maximum();

        m_oldMinimum = minimum();
        m_oldMaximum = maximum();

        /* Re-bound window. */
        setWindow(windowStart, windowEnd);

        /* Re-bound selection. Invalidate if it's outside the new range. */
        if((m_selectionStart >= minimum() && m_selectionStart <= maximum()) || (m_selectionEnd >= minimum() && m_selectionEnd <= maximum())) {
            setSelection(m_selectionStart, m_selectionEnd);
        } else {
            setSelectionValid(false);
        }
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
    if(m_options & SnapZoomToSides) {
        qreal windowRange = m_windowEnd - m_windowStart;
        if((m_zoomAnchor - m_windowStart) / windowRange < zoomSideSnapDistance) {
            m_zoomAnchor = m_windowStart;
        } else if((m_windowEnd - m_zoomAnchor) / windowRange < zoomSideSnapDistance) {
            m_zoomAnchor = m_windowEnd;
        }
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
    updateThumbnailsStepSize(false);
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

    if((m_options & UnzoomOnDoubleClick) && event->button() == Qt::LeftButton)
        animatedUnzoom();
}

void QnTimeSlider::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    base_type::hoverEnterEvent(event);

    unsetCursor();
}

void QnTimeSlider::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    base_type::hoverLeaveEvent(event);

    unsetCursor();

    setThumbnailSelecting(m_lastHoverThumbnail, false);
}

void QnTimeSlider::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    base_type::hoverMoveEvent(event);

    switch(markerFromPosition(event->pos(), hoverEffectDistance)) {
    case SelectionStartMarker:
    case SelectionEndMarker:
        setCursor(Qt::SplitHCursor);
        break;
    default:
        unsetCursor();
        break;
    }

    if(thumbnailsRect().contains(event->pos()) && thumbnailsLoader() && thumbnailsLoader()->timeStep() != 0 && m_oldThumbnailData.isEmpty() && !m_thumbnailsUpdateTimer->isActive()) {
        qint64 time = qRound(valueFromPosition(event->pos()), thumbnailsLoader()->timeStep());

        setThumbnailSelecting(m_lastHoverThumbnail, false);
        setThumbnailSelecting(time, true);
        m_lastHoverThumbnail = time;
    } else {
        setThumbnailSelecting(m_lastHoverThumbnail, false);
        m_lastHoverThumbnail = -1;
    }
}

void QnTimeSlider::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        m_dragMarker = markerFromPosition(event->pos(), hoverEffectDistance);
    } else if(event->button() == Qt::RightButton) {
        if(m_options & SelectionEditable) {
            m_dragMarker = CreateSelectionMarker; 
        } else {
            m_dragMarker = NoMarker;
        }
    }

    bool processed = false;
    if(thumbnailsLoader() && thumbnailsLoader()->timeStep() != 0 && event->button() == Qt::LeftButton && thumbnailsRect().contains(event->pos())) {
        qint64 time = qRound(valueFromPosition(event->pos(), false), thumbnailsLoader()->timeStep());
        QMap<qint64, ThumbnailData>::const_iterator pos = m_thumbnailData.find(time);
        if(pos != m_thumbnailData.end()) {
            setSliderPosition(pos->thumbnail.actualTime(), true);
            processed = true;
        }
    }

    dragProcessor()->mousePressEvent(this, event, m_dragMarker == NoMarker && !processed);

    event->accept();
}

void QnTimeSlider::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    dragProcessor()->mouseMoveEvent(this, event);

    event->accept();
}

void QnTimeSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    dragProcessor()->mouseReleaseEvent(this, event);

    m_dragMarker = NoMarker;

    if(m_dragIsClick && event->button() == Qt::RightButton)
        emit customContextMenuRequested(event->pos(), event->screenPos());

    event->accept();
}

void QnTimeSlider::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    event->ignore();
    return;
}

void QnTimeSlider::startDragProcess(DragInfo *) {
    m_dragIsClick = true;
}

void QnTimeSlider::startDrag(DragInfo *info) {
    setSliderDown(true);

    if(m_dragMarker == CreateSelectionMarker) {
        qint64 pos = valueFromPosition(info->mousePressItemPos());
        setSelectionValid(true);
        setSelection(pos, pos);
        m_dragMarker = SelectionStartMarker;
    }

    if(m_dragMarker == SelectionStartMarker || m_dragMarker == SelectionEndMarker) {
        m_selecting = true;
        emit selectionPressed();
    }

    m_dragIsClick = false;

    if(m_dragMarker == NoMarker) {
        m_dragDelta = QPointF();
    } else {
        m_dragDelta = positionFromMarker(m_dragMarker) - info->mousePressItemPos();
    }
}

void QnTimeSlider::dragMove(DragInfo *info) {
    QPointF mousePos = info->mouseItemPos() + m_dragDelta;

    if(m_dragMarker == NoMarker || m_dragMarker == SelectionStartMarker || m_dragMarker == SelectionEndMarker)
        setSliderPosition(valueFromPosition(mousePos));
    
    if(m_dragMarker == SelectionStartMarker || m_dragMarker == SelectionEndMarker) {
        qint64 selectionStart = m_selectionStart;
        qint64 selectionEnd = m_selectionEnd;
        Marker otherMarker = NoMarker;
        switch(m_dragMarker) {
        case SelectionStartMarker:
            selectionStart = sliderPosition();
            otherMarker = SelectionEndMarker;
            break;
        case SelectionEndMarker:
            selectionEnd = sliderPosition();
            otherMarker = SelectionStartMarker;
            break;
        default:
            break;
        }

        if(selectionStart > selectionEnd) {
            qSwap(selectionStart, selectionEnd);
            m_dragMarker = otherMarker;
        }

        setSelection(selectionStart, selectionEnd);
    }
}

void QnTimeSlider::finishDrag(DragInfo *) {
    if(m_selecting) {
        emit selectionReleased();
        m_selecting = false;
    }

    setSliderDown(false);
}


