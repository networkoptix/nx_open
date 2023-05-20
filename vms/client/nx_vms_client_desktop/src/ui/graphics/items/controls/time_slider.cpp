// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_slider.h"

#include <array>
#include <cmath>
#include <limits>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtCore/QtMath>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>
#include <QtGui/QWindow>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QGraphicsSceneWheelEvent>
#include <QtWidgets/QGraphicsView>

#include <qt_graphics_items/graphics_label.h>
#include <qt_graphics_items/graphics_slider_p.h>

#include <client/client_runtime_settings.h>
#include <core/resource/camera_bookmark.h>
#include <nx/utils/math/arithmetic.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/graphics_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/vms/client/desktop/workbench/timeline/live_preview.h>
#include <nx/vms/client/desktop/workbench/timeline/thumbnail_loading_manager.h>
#include <nx/vms/client/desktop/workbench/timeline/thumbnail_panel.h>
#include <nx/vms/client/desktop/workbench/timeline/timeline_globals.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/text/time_strings.h>
#include <nx/vms/time/formatter.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/controls/bookmarks_viewer.h>
#include <ui/graphics/items/controls/time_slider_pixmap_cache.h>
#include <ui/help/help_topics.h>
#include <ui/processors/drag_processor.h>
#include <ui/processors/kinetic_cutting_processor.h>
#include <ui/utils/bookmark_merge_helper.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <utils/common/aspect_ratio.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>
#include <utils/math/math.h>

using std::chrono::milliseconds;
using namespace std::literals::chrono_literals;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::workbench::timeline;
using namespace nx::vms::common;
using nx::vms::client::core::Geometry;

namespace {

/* Note that most numbers below are given relative to time slider size. */

/* Tickmark bar. */

/** Maximal number of simultaneously shown tickmark levels. */
const int kNumTickmarkLevels = 4;

/** Last displayed text level. */
const int kMaxDisplayedTextLevel = 2;

/** Tickmark lengths for all levels. */
const std::array<int, kNumTickmarkLevels + 1> kTickmarkLengthPixels =
    { 15, 10, 5, 5, 0 };

/** Font pixel heights for all tickmark levels. */
const std::array<int, kNumTickmarkLevels + 1> kTickmarkFontHeights =
    { 12, 12, 11, 11, 0 };

/** Tickmark text pixel heights for all levels. */
const std::array<int, kNumTickmarkLevels + 1> kTickmarkTextHeightPixels =
    { 20, 20, 20, 20, 0 };

/** Font weights for all tickmark levels. */
const std::array<QFont::Weight, kNumTickmarkLevels + 1> kTickmarkFontWeights =
    { QFont::Normal, QFont::Normal, QFont::Normal, QFont::Normal, QFont::Normal };

/** Minimal distance between tickmarks from the same group for this group to be visible.
    * Note that because of the fact that tickmarks do not disappear instantly, in some cases
    * step may become smaller that this value. */
const qreal kMinTickmarkLineStepPixels = 5.0;

/** Minimal distance between tickmark labels from the same group. */
const qreal kMinTickmarkTextSpacingPixels = 5.0;

/** Estimated maximal size of tickmark text label. */
const qreal kMaxTickmarkTextSizePixels = 80.0;

/* Tickmark animation parameters. */
const qreal kTickmarkHeightAnimationMs = 500.0;
const qreal kTickmarkOpacityAnimationMs = 500.0;

/* Dates bar. */

const qreal kDateTextFontHeight = 12;
const auto kDateTextFontWeight = QFont::DemiBold;

const qreal kMinDateSpanFraction = 0.15;
const qreal kMinDateSpanPixels = 160;


/* Lines bar. */

/** Minimal color coefficient for the most noticeable chunk color in range */
const qreal kLineBarMinNoticeableFraction = 0.5;


/* Thumbnails bar. */

const qreal kThumbnailHeightForDrawing = 10.0;


/* Global */

/** Top-level metrics. */
static constexpr qreal kDateBarHeightPixels = 16;
static constexpr qreal kLineBarHeightPixels = 20;
static constexpr qreal kTickBarHeightPixels = 36;

/** Minimal relative change of msecs-per-pixel value of a time slider for animation parameters to be recalculated.
    * This value was introduced so that the parameters are not recalculated constantly when changes are small. */
const qreal kPerPixelChangeThresholdMs = 1.0e-4;

/** Lower limit on time slider scale. */
const qreal kMinTimePerPixelMs = 2.0;

/** Maximal number of lines in a time slider. */
const int kMaxLines = 2;

const int kLastMinuteStripeWidth = 9;

const int kLineLabelFontHeight = 14;
const auto kLineLabelFontWeight = QFont::DemiBold;

/** Padding at the left of line labels. */
const int kLineLabelPaddingPixels = 6;

/** Mouse wheel sensitivities. */
const qreal kWheelDegreesFor2xScale = 30.0;
const qreal kWheelDegreesFor1PixelScroll = 3.0;

/** Window zooming maximum speed for kinetic processor. */
const qreal kWindowZoomMaxSpeed = kWheelDegreesFor2xScale * 8.0;

/** Window zooming friction for kinetic processor. */
const qreal kWindowZoomFriction = kWheelDegreesFor2xScale;

/** Window dragging maximum speed for kinetic processor. */
const qreal kWindowScrollMaxSpeed = 10000.0;

/** Window dragging friction for kinetic processor. */
const qreal kWindowScrollFriction = 1000.0;

static constexpr milliseconds kFullThumbnailPanelFadingTime = 200ms;

const qreal kZoomSideSnapDistance = 0.075;

const qreal kHoverEffectDistance = 5.0;

const int kStartDragDistance = 5;

const int kBookmarkFontHeight = 11;
const auto kBookmarkFontWeight = QFont::DemiBold;
const int kBookmarkTextPadding = 6;
const int kMinBookmarkTextCharsVisible = 6;

/** Width of sensitive areas at the left and right of the window.
    * When a marker is dragged to these areas it causes window scroll.
    * Has effect only with DragScrollsWindow option. */
const qreal kWindowScrollPixelThreshold = 1.0;

const int kNoThumbnailsFontPixelSize = 16;

// Timeline border area width in pixels where single click will auto-scroll the timeline
static constexpr int kAutoShiftAreaWidth = 64;

// Auto-scroll will move the timeline such way, so cursor will be 180 pixels away from border.
static constexpr int kAutoShiftOffsetWidth = 180;

inline qreal adjust(qreal value, qreal target, qreal delta)
{
    return (value < target) ?
        ((target - value < delta) ? target : (value + delta)) :
        ((value - target < delta) ? target : (value - delta));
}

qreal speed(qreal current, qreal target, qreal timeMs)
{
    return qAbs(target - current) / timeMs;
}

bool checkLine(int line)
{
    if (line < 0 || line >= kMaxLines)
    {
        NX_ASSERT(false, "Invalid line number '%1'.", line);
        return false;
    }

    return true;
}

bool checkLinePeriod(int line, Qn::TimePeriodContent type)
{
    if (!checkLine(line))
        return false;

    if (type < 0 || type >= Qn::TimePeriodContentCount)
    {
        NX_ASSERT(false, "Invalid time period type '%1'.", static_cast<int>(type));
        return false;
    }

    return true;
}

void drawData(QPainter* painter, const QRectF& targetRect, const QPixmap& data, const QRectF& sourceRect)
{
    painter->drawPixmap(targetRect, data, sourceRect);
}

void drawData(QPainter* painter, const QRectF& targetRect, const QImage& data, const QRectF& sourceRect)
{
    painter->drawImage(targetRect, data, sourceRect);
}

template<class Data>
void drawCroppedData(QPainter* painter, const QRectF& target, const QRectF& cropTarget, const Data& data, const QRectF& source, QRectF* drawnTarget = nullptr)
{
    QMarginsF targetMargins(
        qMax(0.0, cropTarget.left() - target.left()),
        qMax(0.0, cropTarget.top() - target.top()),
        qMax(0.0, target.right() - cropTarget.right()),
        qMax(0.0, target.bottom() - cropTarget.bottom())
    );

    QRectF erodedTarget;

    if (targetMargins.isNull())
        erodedTarget = target;
    else
        erodedTarget = Geometry::eroded(target, targetMargins);

    if (!erodedTarget.isValid())
    {
        erodedTarget = QRectF();
    }
    else
    {
        QMarginsF sourceMargins = Geometry::cwiseMul(Geometry::cwiseDiv(targetMargins, target.size()), source.size());
        QRectF erodedSource = Geometry::eroded(source, sourceMargins);

        drawData(painter, erodedTarget, data, erodedSource);
    }

    if (drawnTarget)
        *drawnTarget = erodedTarget;
}

void drawCroppedPixmap(QPainter* painter, const QRectF& target, const QRectF& cropTarget, const QPixmap& pixmap, const QRectF& source, QRectF* drawnTarget = nullptr)
{
    drawCroppedData(painter, target, cropTarget, pixmap, source, drawnTarget);
}

void drawCroppedImage(QPainter* painter, const QRectF& target, const QRectF& cropTarget, const QImage& image, const QRectF& source, QRectF* drawnTarget = nullptr)
{
    drawCroppedData(painter, target, cropTarget, image, source, drawnTarget);
}

QPoint mapItemToGlobal(const QGraphicsItem* item, const QPointF& point)
{
    const auto scene = item->scene();
    if (!scene)
        return {};

    if (scene->views().isEmpty() || !scene->views().first() || !scene->views().first()->viewport())
        return {};

    const QGraphicsView* v = scene->views().first();
    return v->viewport()->mapToGlobal(v->mapFromScene(item->mapToScene(point)));
}

} // anonymous namespace

// -------------------------------------------------------------------------- //
// QnTimeSliderChunkPainter
// -------------------------------------------------------------------------- //
// TODO: #sivanov
// An even better solution that will remove all blinking
// (we still have it with recorded chunks trapped between two motion chunks)
// would be to draw it pixel-by-pixel, counting the motion/recording percentage
// in each pixel. This approach can be made to work just as fast as the current one.
class QnTimeSliderChunkPainter
{
public:
    QnTimeSliderChunkPainter(QnTimeSlider* slider, QPainter* painter):
        m_slider(slider),
        m_painter(painter),
        m_minChunkLength(0),
        m_position(0),
        m_pendingLength(0),
        m_pendingPosition(0)
    {
        NX_ASSERT(m_painter && m_slider);

        // TODO: #vkutin Refactor this class to operate with "recording" and "extra" content types.
        const bool analytics = m_slider->selectedExtraContent() == Qn::AnalyticsContent;

        m_colors[Qn::RecordingContent] = colorTheme()->color("green_core");
        m_colors[Qn::MotionContent] = analytics
            ? colorTheme()->color("yellow_core")
            : colorTheme()->color("red_d1");
        m_colors[Qn::TimePeriodContentCount] = colorTheme()->color("green_d3");

        m_position = m_minChunkLength = 0ms;
    }

    void start(milliseconds startPos, milliseconds minChunkLength, const QRectF& rect)
    {
        m_position = startPos;
        m_minChunkLength = minChunkLength;
        m_rect = rect;

        std::fill(m_weights.begin(), m_weights.end(), 0);
    }

    void paintChunk(milliseconds length, Qn::TimePeriodContent content)
    {
        NX_ASSERT(length >= 0ms);

        if (m_pendingLength > 0ms && m_pendingLength + length > m_minChunkLength)
        {
            milliseconds delta = m_minChunkLength - m_pendingLength;
            length -= delta;

            storeChunk(delta, content);
            flushChunk();
        }

        storeChunk(length, content);
        if (m_pendingLength >= m_minChunkLength)
            flushChunk();
    }

    void stop()
    {
        if (m_pendingLength > 0ms)
            flushChunk();
    }

private:
    void storeChunk(milliseconds length, Qn::TimePeriodContent content)
    {
        if (m_pendingLength == 0ms)
            m_pendingPosition = m_position;

        m_weights[content] += length.count();
        m_pendingLength += length;
        m_position += length;
    }

    void flushChunk()
    {
        milliseconds leftPosition = m_pendingPosition;
        milliseconds rightPosition = m_pendingPosition + m_pendingLength;

        qreal l = m_slider->quickPositionFromTime(leftPosition);
        qreal r = m_slider->quickPositionFromTime(rightPosition);

        m_painter->fillRect(QRectF(l, m_rect.top(), r - l, m_rect.height()), currentColor(m_colors));

        m_pendingPosition = rightPosition;
        m_pendingLength = 0ms;

        std::fill(m_weights.begin(), m_weights.end(), 0);
    }

    QColor currentColor(const std::array<QColor, Qn::TimePeriodContentCount + 1>& colors) const
    {
        qreal rc = m_weights[Qn::RecordingContent];
        qreal mc = m_weights[Qn::MotionContent];
        qreal nc = m_weights[Qn::TimePeriodContentCount];
        qreal sum = m_pendingLength.count();

        if (!qFuzzyIsNull(mc))
        {
            /* Make sure motion is noticeable even if there isn't much of it.
             * Note that these adjustments don't change sum. */
            rc = rc * (1.0 - kLineBarMinNoticeableFraction);
            mc = sum * kLineBarMinNoticeableFraction + mc * (1.0 - kLineBarMinNoticeableFraction);
            nc = nc * (1.0 - kLineBarMinNoticeableFraction);
        }
        else if (!qFuzzyIsNull(rc) && rc < sum * kLineBarMinNoticeableFraction)
        {
            /* Make sure recording content is noticeable even if there isn't much of it.
             * Note that these adjustments don't change sum because mc == 0. */
            rc = sum * kLineBarMinNoticeableFraction;// + rc * (1.0 - kLineBarMinNoticeableFraction);
            nc = sum * (1.0 - kLineBarMinNoticeableFraction);
        }

        return linearCombine(rc / sum, colors[Qn::RecordingContent], 1.0, linearCombine(mc / sum,
            colors[Qn::MotionContent], nc / sum, colors[Qn::TimePeriodContentCount]));
    }

private:
    QnTimeSlider* m_slider;
    QPainter* m_painter;

    milliseconds m_minChunkLength;
    QRectF m_rect;

    milliseconds m_position;
    milliseconds m_pendingLength;
    milliseconds m_pendingPosition;

    std::array<quint64, Qn::TimePeriodContentCount + 1> m_weights;
    std::array<QColor, Qn::TimePeriodContentCount + 1> m_colors;
};


// -------------------------------------------------------------------------- //
// QnTimeSliderStepStorage
// -------------------------------------------------------------------------- //
// TODO: #sivanov Move all step construction here.
class QnTimeSliderStepStorage
{
public:
    QnTimeSliderStepStorage()
    {
        QnTimeSlider::createSteps(&m_absolute,& m_relative);
    }

    const QVector<QnTimeStep>& absolute() const
    {
        return m_absolute;
    }

    const QVector<QnTimeStep>& relative() const
    {
        return m_relative;
    }

private:
    QVector<QnTimeStep> m_absolute, m_relative;
};

Q_GLOBAL_STATIC(QnTimeSliderStepStorage, timeSteps);

// -------------------------------------------------------------------------- //
// QnTimeSlider::KineticHandlerBase
// -------------------------------------------------------------------------- //

class QnTimeSlider::KineticHandlerBase: public KineticProcessHandler
{
public:
    KineticHandlerBase(QnTimeSlider* slider): m_slider(slider) {}

    void hurry()
    {
        if (!kineticProcessor()->isRunning())
            return; /* Nothing to hurry up. */

        if (m_hurried)
            return;

        m_hurried = true;
        updateKineticProcessor();
    }

    void unhurry()
    {
        if (!m_hurried)
            return;

        m_hurried = false;
        updateKineticProcessor();
    }

    virtual void updateKineticProcessor() {}

    virtual void finishKinetic() override
    {
        unhurry();
    }

protected:
    QnTimeSlider* const m_slider;
    bool m_hurried = false;
};

// -------------------------------------------------------------------------- //
// QnTimeSlider::KineticZoomHandler
// -------------------------------------------------------------------------- //

class QnTimeSlider::KineticZoomHandler: public QnTimeSlider::KineticHandlerBase
{
public:
    using KineticHandlerBase::KineticHandlerBase;

    virtual void kineticMove(const QVariant& degrees) override
    {
        qreal factor = std::pow(2.0, -degrees.toReal() / kWheelDegreesFor2xScale);

        if (!m_slider->scaleWindow(factor, m_slider->m_zoomAnchor))
            kineticProcessor()->reset();
    }

    virtual void updateKineticProcessor() override
    {
        kineticProcessor()->setFriction(m_hurried
            ? kWindowZoomFriction * 2.0
            : kWindowZoomFriction);
    }
};

// -------------------------------------------------------------------------- //
// QnTimeSlider::KineticScrollHandler
// -------------------------------------------------------------------------- //

class QnTimeSlider::KineticScrollHandler: public QnTimeSlider::KineticHandlerBase
{
public:
    using KineticHandlerBase::KineticHandlerBase;

    virtual void kineticMove(const QVariant& distance) override
    {
        const auto oldWindowStart = m_slider->m_windowStart;
        m_slider->shiftWindow(milliseconds(qint64(distance.toReal() * m_slider->m_msecsPerPixel)));

        if (oldWindowStart == m_slider->m_windowStart)
            kineticProcessor()->reset();
    }

    virtual void updateKineticProcessor() override
    {
        kineticProcessor()->setFriction(m_hurried
            ? kWindowScrollFriction * 4.0
            : kWindowScrollFriction);
    }
};


// -------------------------------------------------------------------------- //
// QnTimeSlider
// -------------------------------------------------------------------------- //
QnTimeSlider::QnTimeSlider(
    QnWorkbenchContext* context,
    QGraphicsItem* parent,
    QGraphicsItem* tooltipParent)
    :
    base_type(Qt::Horizontal, parent),
    QnWorkbenchContextAware(context),
    m_view(context->mainWindow()->view()),
    m_tooltipParent(tooltipParent),
    m_tooltip(new TimeMarker(context)),
    m_windowStart(0),
    m_windowEnd(0),
    m_minimalWindow(0),
    m_selectionStart(0),
    m_selectionEnd(0),
    m_selectionValid(false),
    m_oldMinimum(0),
    m_oldMaximum(0),
    m_zoomAnchor(0),
    m_animatingSliderWindow(false),
    m_kineticsHurried(false),
    m_dragMarker(NoMarker),
    m_dragIsClick(false),
    m_selecting(false),
    m_lineCount(0),
    m_totalLineStretch(0.0),
    m_maxStepIndex(0),
    m_msecsPerPixel(1.0),
    m_animationUpdateMSecsPerPixel(1.0),
    m_lastThumbnailsUpdateTime(0),
    m_lastHoverThumbnail(-1),
    m_thumbnailsVisible(false),
    m_tooltipVisible(false),
    m_lastMinuteAnimationDelta(0),
    m_pixmapCache(new QnTimeSliderPixmapCache(kNumTickmarkLevels, this)),
    m_localOffset(0),
    m_lastLineBarValue(),
    m_bookmarksViewer(createBookmarksViewer(context->mainWindow()->view())),
    m_bookmarksVisible(false),
    m_bookmarksHelper(nullptr),
    m_liveSupported(false),
    m_selectionInitiated(false),
    m_livePreview(new LivePreview(this)),
    m_useLivePreview(ini().enableTimelinePreview && systemSettings()->showMouseTimelinePreview()),
    m_updatingValue(false),
    m_kineticZoomHandler(new KineticZoomHandler(this)),
    m_kineticScrollHandler(new KineticScrollHandler(this)),
    m_isLive(false)
{
    setObjectName("TimeSlider");

    setOrientation(Qt::Horizontal);
    setFlag(ItemSendsScenePositionChanges, true);

    connect(m_animationTimerListener.get(), &AnimationTimerListener::tick, this,
        &QnTimeSlider::tick);

    connect(this, &QGraphicsObject::visibleChanged, this,
        [this]()
        {
            m_tooltip->setShown(isVisible());
            updateLivePreview();
        });

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &QnTimeSlider::updateLivePreview);

    setPaletteColor(this, QPalette::Window, colorTheme()->color("dark6"));

    setSkipUpdateOnSliderChange({ SliderRangeChange, SliderStepsChange, SliderValueChange, SliderMappingChange });

    /* Prepare thumbnail update timer. */
    m_thumbnailsUpdateTimer = new QTimer(this);
    m_thumbnailsUpdateTimer->setSingleShot(true);
    m_thumbnailsUpdateTimer->setInterval(250);
    connect(m_thumbnailsUpdateTimer, &QTimer::timeout, this,
        [this]() { updateThumbnailsStepSize(/*instant*/ true); });

    /* Set default vector sizes. */
    m_lineData.resize(kMaxLines);
    m_lastMinuteIndicatorVisible.fill(true, kMaxLines);

    generateProgressPatterns();

    /* Fine-tuned kinetic processor constants. */
    static const qreal kZoomProcessorMaxShiftIntervalSeconds = 0.4;
    static const qreal kScrollProcessorMaxShiftIntervalSeconds = 0.05;
    static const qreal kZoomProcessorSpeedCuttingThreshold = kWindowZoomMaxSpeed / 24.0;
    static const qreal kScrollProcessorSpeedCuttingThreshold = kWindowScrollMaxSpeed / 10.0;

    /* Prepare kinetic zoom processor. */
    const auto kineticZoomProcessor = new KineticCuttingProcessor(QMetaType::QReal, this);
    kineticZoomProcessor->setHandler(m_kineticZoomHandler.data());
    kineticZoomProcessor->setMaxShiftInterval(kZoomProcessorMaxShiftIntervalSeconds);
    kineticZoomProcessor->setFlags(KineticProcessor::IgnoreDeltaTime);
    kineticZoomProcessor->setMaxSpeedMagnitude(kWindowZoomMaxSpeed);
    kineticZoomProcessor->setSpeedCuttingThreshold(kZoomProcessorSpeedCuttingThreshold);
    registerAnimation(kineticZoomProcessor->animationTimerListener());
    m_kineticZoomHandler->updateKineticProcessor();

    /* Prepare kinetic drag processor. */
    const auto kineticScrollProcessor = new KineticCuttingProcessor(QMetaType::QReal, this);
    kineticScrollProcessor->setHandler(m_kineticScrollHandler.data());
    kineticScrollProcessor->setMaxShiftInterval(kScrollProcessorMaxShiftIntervalSeconds);
    kineticScrollProcessor->setMaxSpeedMagnitude(kWindowScrollMaxSpeed);
    kineticScrollProcessor->setSpeedCuttingThreshold(kScrollProcessorSpeedCuttingThreshold);
    registerAnimation(kineticScrollProcessor->animationTimerListener());
    m_kineticScrollHandler->updateKineticProcessor();

    /* Prepare zoom processor. */
    DragProcessor* dragProcessor = new DragProcessor(this);
    dragProcessor->setHandler(this);
    dragProcessor->setFlags(DragProcessor::DontCompress);
    dragProcessor->setStartDragDistance(kStartDragDistance);
    dragProcessor->setStartDragTime(-1); /* No drag on timeout. */

    /* Prepare animation timer listener. */
    m_animationTimerListener->startListening();
    registerAnimation(m_animationTimerListener);

    /* Set default property values. */
    setAcceptHoverEvents(true);
    setProperty(nx::style::Properties::kSliderLength, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed, QSizePolicy::Slider);

    setWindowStart(minimum());
    setWindowEnd(maximum());
    Options defaultOptions = PreserveWindowSize
        | SelectionEditable | ClearSelectionOnClick | SnapZoomToSides | UnzoomOnDoubleClick
        | UpdateToolTip;
#ifdef TIMELINE_BEHAVIOR_2_5
#error Not supported any more since 4.0
    defaultOptions |= AdjustWindowToPosition;
#else
    defaultOptions |= StillPosition | HideLivePosition | LeftButtonSelection
        | DragScrollsWindow /*| StillBookmarksViewer*/;
#endif
    if (!qnRuntime->isAcsMode())
        defaultOptions |= StickToMinimum | StickToMaximum;

    setOptions(defaultOptions);

    /* Run handlers. */
    updateSteps();
    updateMinimalWindow();
    updatePixmapCache();
    sliderChange(SliderRangeChange);

    m_bookmarksViewer->setParent(this);
    m_bookmarksViewer->setParentItem(tooltipParent);
    m_bookmarksViewer->setZValue(std::numeric_limits<qreal>::max());

    connect(this, &AbstractGraphicsSlider::valueChanged,
        this, [this](qint64 value) { emit timeChanged(milliseconds(value)); });
    connect(this, &AbstractGraphicsSlider::rangeChanged,
        this,
        [this](qint64 min, qint64 max) { emit timeRangeChanged(milliseconds(min), milliseconds(max)); });

    connect(this, &QnTimeSlider::selectionPressed, this, &QnTimeSlider::updateLive);
    connect(this, &QnTimeSlider::selectionReleased, this, &QnTimeSlider::updateLive);

    if (!ini().debugDisableQmlTooltips)
        m_tooltip->widget()->installEventFilter(this);

    connect(qApp, &QApplication::focusChanged, this,
        [this](QWidget* old, QWidget* /*now*/) { m_lastFocusedWidget = old; });

    connect(systemSettings(), &SystemSettings::showMouseTimelinePreviewChanged,
        [this]
        {
            m_useLivePreview = ini().enableTimelinePreview
                && systemSettings()->showMouseTimelinePreview();
        });
}

bool QnTimeSlider::isWindowBeingDragged() const
{
    return dragProcessor()->isRunning() && m_dragMarker == NoMarker;
}

QnBookmarksViewer* QnTimeSlider::createBookmarksViewer(QWidget* tooltipParentWidget)
{
    const auto bookmarksAtLocationFunc =
        [this](qint64 location)
        {
            if (!m_bookmarksHelper)
                return QnCameraBookmarkList();

            auto timepos = timeFromPosition(QPointF(location, 0));

            static const auto searchOptions = QnBookmarkMergeHelper::OnlyTopmost
                | QnBookmarkMergeHelper::ExpandArea;

            return m_bookmarksHelper->bookmarksAtPosition(timepos,
                milliseconds(qint64(m_msecsPerPixel)), searchOptions);
        };

    const auto getPosFunc =
        [this](qint64 location)
        {
            if (location >= rect().width())
                return QnBookmarksViewer::PosAndBoundsPair();   //< Out of window

            const auto viewer = bookmarksViewer();

            qreal pos = static_cast<qreal>(location);

            const auto target = QPointF(pos, lineBarRect().top());

            Q_D(const GraphicsSlider);

            const auto left = viewer->mapFromItem(this, QPointF(d->pixelPosMin, 0));
            const auto right = viewer->mapFromItem(this, QPointF(d->pixelPosMax, 0));
            const QnBookmarksViewer::Bounds bounds(left.x(), right.x());
            const QPointF finalPos = viewer->mapToParent(viewer->mapFromItem(this, target));
            return QnBookmarksViewer::PosAndBoundsPair(finalPos, bounds);
        };

    return new QnBookmarksViewer(tooltipParentWidget, bookmarksAtLocationFunc, getPosFunc, this);
}

QnTimeSlider::~QnTimeSlider()
{
    disconnect(qApp, nullptr, this, nullptr);
}

void QnTimeSlider::createSteps(QVector<QnTimeStep>* absoluteSteps, QVector<QnTimeStep>* relativeSteps)
{

    static const QString mFormat = nx::vms::time::getFormatString(nx::vms::time::Format::hh_mm);
    static const QString hFormat = nx::vms::time::getFormatString(nx::vms::time::Format::hh);

    static const QString dFormat = nx::vms::time::getFormatString(nx::vms::time::Format::dd);
    static const QString moFormat = nx::vms::time::getFormatString(nx::vms::time::Format::MMM);
    static const QString yFormat = nx::vms::time::getFormatString(nx::vms::time::Format::yyyy);
    static const QString dateMinsFormat = nx::vms::time::getFormatString(nx::vms::time::Format::dd_MMMM_yyyy)
        + QChar::Space + nx::vms::time::getFormatString(nx::vms::time::Format::hh_mm);
    static const QString dateHoursFormat = nx::vms::time::getFormatString(nx::vms::time::Format::dd_MMMM_yyyy)
        + QChar::Space + nx::vms::time::getFormatString(nx::vms::time::Format::hh);
    static const QString dateDaysFormat = nx::vms::time::getFormatString(nx::vms::time::Format::dd_MMMM_yyyy);
    static const QString dateMonthsFormat = nx::vms::time::getFormatString(nx::vms::time::Format::MMMM_yyyy);
    static const QString dateYearsFormat = nx::vms::time::getFormatString(nx::vms::time::Format::yyyy);

    QString msSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Milliseconds);
    QString sSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds);
    QString mSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Minutes);
    QString hSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Hours);
    QString dSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Days);
    QString moSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Months);
    QString ySuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Years);

    *absoluteSteps <<
        QnTimeStep(QnTimeStep::Milliseconds,  1ms,        10,     1000,   msSuffix,     QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,  1ms,        50,     1000,   msSuffix,     QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,  1ms,        100,    1000,   msSuffix,     QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,  1ms,        500,    1000,   msSuffix,     QString(),          false) <<
        QnTimeStep(QnTimeStep::Seconds,       1s,         1,      60,     sSuffix,      QString(),          false) <<
        QnTimeStep(QnTimeStep::Seconds,       1s,         5,      60,     sSuffix,      QString(),          false) <<
        QnTimeStep(QnTimeStep::Seconds,       1s,         10,     60,     sSuffix,      QString(),          false) <<
        QnTimeStep(QnTimeStep::Seconds,       1s,         30,     60,     sSuffix,      QString(),          false) <<
        QnTimeStep(QnTimeStep::Minutes,       1min,       1,      24*60,  mFormat,      dateMinsFormat,     false) <<
        QnTimeStep(QnTimeStep::Minutes,       1min,       5,      24*60,  mFormat,      QString(),          false) <<
        QnTimeStep(QnTimeStep::Minutes,       1min,       10,     24*60,  mFormat,      QString(),          false) <<
        QnTimeStep(QnTimeStep::Minutes,       1min,       30,     24*60,  mFormat,      QString(),          false) <<
        QnTimeStep(QnTimeStep::Hours,         1h,         1,      24,     hFormat,      dateHoursFormat,    false) <<
        QnTimeStep(QnTimeStep::Hours,         1h,         3,      24,     hFormat,      QString(),          false) <<
        QnTimeStep(QnTimeStep::Hours,         1h,         12,     24,     hFormat,      QString(),          false) <<
        QnTimeStep(QnTimeStep::Days,          24h,        1,      31,     dFormat,      dateDaysFormat,     false) <<
        QnTimeStep(QnTimeStep::Months,        30 * 24h,   1,      12,     moFormat,     dateMonthsFormat,   false) <<
        QnTimeStep(QnTimeStep::Years,         365 * 24h,  1,      50000,  yFormat,      dateYearsFormat,    false) <<
        QnTimeStep(QnTimeStep::Years,         365 * 24h,  5,      50000,  yFormat,      QString(),          false) <<
        QnTimeStep(QnTimeStep::Years,         365 * 24h,  10,     50000,  yFormat,      dateYearsFormat,    false);
    enumerateSteps(*absoluteSteps);

    *relativeSteps <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ms,            50,     1000,   msSuffix,   QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ms,            10,     1000,   msSuffix,   QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ms,            100,    1000,   msSuffix,   QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ms,            500,    1000,   msSuffix,   QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1s,             1,      60,     sSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1s,             5,      60,     sSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1s,             10,     60,     sSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1s,             30,     60,     sSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1min,           1,      60,     mSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1min,           5,      60,     mSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1min,           10,     60,     mSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1min,           30,     60,     mSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1h,             1,      24,     hSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1h,             3,      24,     hSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1h,             12,     24,     hSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    24h,            1,      31,     dSuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    30 * 24h,       1,      12,     moSuffix,   QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    12 * 30 * 24h,  1,      50000,  ySuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    12 * 30 * 24h,  5,      50000,  ySuffix,    QString(),  true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    12 * 30 * 24h,  10,     50000,  ySuffix,    QString(),  true);
    enumerateSteps(*relativeSteps);
}

void QnTimeSlider::enumerateSteps(QVector<QnTimeStep>& steps)
{
    for (int i = 0; i < steps.size(); i++)
        steps[i].index = i;
}

void QnTimeSlider::invalidateWindow()
{
    m_oldMaximum = m_oldMinimum = -1ms;
}

int QnTimeSlider::lineCount() const
{
    return m_lineCount;
}

void QnTimeSlider::setLineCount(int lineCount)
{
    if (m_lineCount == lineCount)
        return;

    if (lineCount < 0 || lineCount > kMaxLines)
    {
        NX_ASSERT(false, "Invalid line count '%1'.", lineCount);
        return;
    }

    m_lineCount = lineCount;
}

void QnTimeSlider::setLineVisible(int line, bool visible)
{
    if (!checkLine(line))
        return;

    if (m_lineData[line].visible == visible)
        return;

    m_lineData[line].visible = visible;

    updateTotalLineStretch();
}

bool QnTimeSlider::isLineVisible(int line) const
{
    if (!checkLine(line))
        return false;

    return m_lineData[line].visible;
}

void QnTimeSlider::setLineStretch(int line, qreal stretch)
{
    if (!checkLine(line))
        return;

    if (qFuzzyEquals(m_lineData[line].stretch, stretch))
        return;

    m_lineData[line].stretch = stretch;

    updateTotalLineStretch();
}

qreal QnTimeSlider::lineStretch(int line) const
{
    if (!checkLine(line))
        return 0.0;

    return m_lineData[line].stretch;
}

void QnTimeSlider::setLineComment(int line, const QString& comment)
{
    if (!checkLine(line))
        return;

    if (m_lineData[line].comment == comment)
        return;

    m_lineData[line].comment = comment;

    updateLineCommentPixmap(line);

    emit lineCommentChanged(line, m_lineData[line].comment);
}

QString QnTimeSlider::lineComment(int line)
{
    if (!checkLine(line))
        return QString();

    return m_lineData[line].comment;
}

QnTimePeriodList QnTimeSlider::timePeriods(int line, Qn::TimePeriodContent type) const
{
    if (!checkLinePeriod(line, type))
        return QnTimePeriodList();

    return m_lineData[line].timeStorage.periods(type);
}

void QnTimeSlider::setTimePeriods(int line, Qn::TimePeriodContent type, const QnTimePeriodList& timePeriods)
{
    if (!checkLinePeriod(line, type))
        return;

    m_lineData[line].timeStorage.setPeriods(type, timePeriods);
}

QnTimeSlider::Options QnTimeSlider::options() const
{
    return m_options;
}

void QnTimeSlider::setOptions(Options options)
{
    if (m_options == options)
        return;

    Options difference = m_options ^ options;
    m_options = options;

    if (difference & UseUTC)
    {
        updateSteps();
        updateTickmarkTextSteps();
        updateToolTipText();
        invalidateWindow();
    }
}

void QnTimeSlider::setOption(Option option, bool value)
{
    if (value)
        setOptions(m_options | option);
    else
        setOptions(m_options & ~option);
}

void QnTimeSlider::setTimeRange(milliseconds min, milliseconds max)
{
    // All positions within slider range should satisfy (position + m_localOffset >= 0)
    // at all times. setTimeRange() and setLocalOffset() can be called independently, so adjust
    // range according to m_localOffset in both methods.
    min = qMax(-m_localOffset, min);
    max = qMax(min, max);
    setRange(min.count(), max.count());
}

milliseconds QnTimeSlider::minimum() const
{
    return milliseconds(base_type::minimum());
}

milliseconds QnTimeSlider::maximum() const
{
    return milliseconds(base_type::maximum());
}

milliseconds QnTimeSlider::windowStart() const
{
    return m_windowStart;
}

void QnTimeSlider::setWindowStart(milliseconds windowStart)
{
    setWindow(windowStart, qMax(windowStart + m_minimalWindow, m_windowEnd));
}

milliseconds QnTimeSlider::windowEnd() const
{
    return m_windowEnd;
}

void QnTimeSlider::setWindowEnd(milliseconds windowEnd)
{
    setWindow(qMin(m_windowStart, windowEnd - m_minimalWindow), windowEnd);
}

void QnTimeSlider::setWindow(milliseconds start, milliseconds end, bool animate, bool forceResize)
{
    milliseconds targetWindowSize = end - start;

    if (!qnRuntime->isAcsMode())
    {
        start = qBound(minimum(), start, maximum());
        end = qMax(start, qBound(minimum(), end, maximum()));
    }

    /* Check if window size was spoiled and fix it if possible. */
    if (!forceResize && m_options.testFlag(PreserveWindowSize))
    {
        milliseconds newWindowSize = end - start;
        milliseconds range = maximum() - minimum();
        if (newWindowSize < targetWindowSize && newWindowSize < range)
        {
            milliseconds windowSize = qMin(targetWindowSize, range);
            if (start == minimum())
                end = start + windowSize;
            else
                start = end - windowSize;
        }
    }

    if (end - start < m_minimalWindow)
    {
        milliseconds delta = (m_minimalWindow - (end - start) + 1ms) / 2;
        start -= delta;
        end += delta;

        if (start < minimum())
        {
            end += minimum() - start;
            start = minimum();
        }
        if (end > maximum())
        {
            start += maximum() - end;
            end = maximum();
        }

        start = qBound(minimum(), start, maximum());
        end = qBound(minimum(), end, maximum());
    }

    if (m_windowStart != start || m_windowEnd != end)
    {
        if (animate)
        {
            m_kineticZoomHandler->kineticProcessor()->reset();
            m_kineticScrollHandler->kineticProcessor()->reset();
            m_animatingSliderWindow = true;
            setTargetStart(start);
            setTargetEnd(end);
        }
        else
        {
            m_windowStart = start;
            m_windowEnd = end;

            sliderChange(SliderMappingChange);
            emit windowChanged(m_windowStart, m_windowEnd);

            updateToolTipVisibilityInternal();

            // If msecsPerPixel changed, ThumbnailLoadingManager::refresh() will be called by
            // updateMSecsPerPixel(). Update thumbnails period without full refresh otherwise.
            if (!updateMSecsPerPixel())
                updateThumbnailsPeriod();

            m_bookmarksViewer->updateOnWindowChange();
            updateBookmarksViewerLocation();
        }
    }
}

void QnTimeSlider::shiftWindow(milliseconds delta, bool animate)
{
    milliseconds windowSize = m_windowEnd - m_windowStart;
    if (delta < 0ms)
    {
        milliseconds newWindowStart = qMax(m_windowStart + delta, minimum());
        setWindow(newWindowStart, newWindowStart + windowSize, animate);
    }
    else
    {
        milliseconds newWindowEnd = qMin(m_windowEnd + delta, maximum());
        setWindow(newWindowEnd - windowSize, newWindowEnd, animate);
    }

    m_zoomAnchor = qBound(m_windowStart, m_zoomAnchor + delta, m_windowEnd);
}

bool QnTimeSlider::windowContains(milliseconds position)
{
    return m_windowStart <= position && position <= m_windowEnd;
}

void QnTimeSlider::ensureWindowContains(milliseconds position)
{
    if (position < m_windowStart)
        shiftWindow(position - m_windowStart);
    else if (m_windowEnd < position)
        shiftWindow(position - m_windowEnd);
}

void QnTimeSlider::scrollIntoWindow(milliseconds position, bool animated)
{
    const auto allowedOffset = milliseconds(qint64(m_msecsPerPixel * kAutoShiftAreaWidth));
    const auto targetOffset = milliseconds(qint64(m_msecsPerPixel * kAutoShiftOffsetWidth));
    const auto leftOffset = position - m_windowStart;
    const auto rightOffset = m_windowEnd - position;

    if (leftOffset <= allowedOffset)
    {
        // Shift window to the left (negative).
        shiftWindow(leftOffset - targetOffset, animated);
    }
    else if (rightOffset <= allowedOffset)
    {
        // Shift window to the right (positive).
        shiftWindow(targetOffset - rightOffset, animated);
    }
}

milliseconds QnTimeSlider::sliderTimePosition() const
{
    return milliseconds(base_type::sliderPosition());
}

void QnTimeSlider::setSliderPosition(milliseconds position, bool keepInWindow)
{
    if (!keepInWindow)
        return setSliderPosition(position.count());

    bool inWindow = windowContains(sliderTimePosition());
    setSliderPosition(position.count());
    if (inWindow)
        ensureWindowContains(sliderTimePosition());
}

milliseconds QnTimeSlider::value() const
{
    return milliseconds(base_type::value());
}

void QnTimeSlider::setValue(milliseconds value, bool keepInWindow)
{
    {
        /* To not change tooltip visibility in setValue or setWindow: */
        QScopedValueRollback<bool> updateRollback(m_updatingValue, true);

        milliseconds oldValue = this->value();
        setValue(value.count());

        if (keepInWindow && !isWindowBeingDragged() && windowContains(oldValue))
        {
            if (m_options.testFlag(StillPosition))
                shiftWindow(this->value() - oldValue);
            else
                ensureWindowContains(this->value());
        }
    }

    // Update tooltip visibility and position after both setValue and setWindow:
    queueTooltipPositionUpdate();
    updateToolTipVisibilityInternal();
}

void QnTimeSlider::navigateTo(milliseconds value)
{
    setValue(value, false);
    scrollIntoWindow(this->value(), true);
}

QnTimePeriod QnTimeSlider::selection() const
{
    return QnTimePeriod::fromInterval(m_selectionStart.count(), m_selectionEnd.count());
}

milliseconds QnTimeSlider::selectionStart() const
{
    return m_selectionStart;
}

void QnTimeSlider::setSelectionStart(milliseconds selectionStart)
{
    setSelection(selectionStart, qMax(selectionStart, m_selectionEnd));
}

milliseconds QnTimeSlider::selectionEnd() const
{
    return m_selectionEnd;
}

void QnTimeSlider::setSelectionEnd(milliseconds selectionEnd)
{
    setSelection(qMin(m_selectionStart, selectionEnd), selectionEnd);
}

void QnTimeSlider::setSelection(milliseconds start, milliseconds end)
{
    start = qBound(minimum(), start, maximum());
    end = qMax(start, qBound(minimum(), end, maximum()));

    if (m_selectionStart != start || m_selectionEnd != end)
    {
        m_selectionInitiated = false;
        m_selectionStart = start;
        m_selectionEnd = end;

        if (isSelectionValid())
            emit selectionChanged(selection());
    }
}

bool QnTimeSlider::isSelectionValid() const
{
    return m_selectionValid;
}

void QnTimeSlider::setSelectionValid(bool valid)
{
    if (!m_selectionValid)
        m_selectionInitiated = false;

    if (m_selectionValid == valid)
        return;

    m_selectionValid = valid;

    emit selectionChanged(m_selectionValid
        ? QnTimePeriod::fromInterval(m_selectionStart.count(), m_selectionEnd.count())
        : QnTimePeriod());
}

QnTimeSlider::ThumbnailLoadingManager* QnTimeSlider::thumbnailLoadingManager() const
{
    return m_thumbnailManager.get();
}

void QnTimeSlider::setThumbnailLoadingManager(std::unique_ptr<ThumbnailLoadingManager> manager)
{
    clearThumbnails();
    m_oldThumbnailData.clear();
    m_thumbnailsUpdateTimer->stop();

    m_thumbnailManager = std::move(manager);
    m_livePreview->setThumbnailLoadingManager(m_thumbnailManager.get());

    if (!m_thumbnailManager)
        return;

    connect(m_thumbnailManager.get(), &ThumbnailLoadingManager::panelThumbnailsInvalidated,
        [this]
        {
            clearThumbnails();
            updateThumbnailsStepSize(/*instant*/ true);
        });

    connect(m_thumbnailManager.get(), &ThumbnailLoadingManager::panelThumbnailLoaded,
        this, &QnTimeSlider::addThumbnail);

    connect(m_thumbnailManager.get(), &ThumbnailLoadingManager::aspectRatioChanged, this,
        [this]
        {
            clearThumbnails();
            updateThumbnailsStepSize(/*instant*/ true);
        });

    connect(m_thumbnailManager.get(), &ThumbnailLoadingManager::rotationChanged, this,
        [this]
        {
            clearThumbnails();
            updateThumbnailsStepSize(/*instant*/ true);
        });

    if (m_thumbnailManager->aspectRatio().isValid())
        updateThumbnailsStepSize(true);

    addAllThumbnailsFromManager();
}

void QnTimeSlider::setThumbnailPanel(ThumbnailPanel* thumbnailPanel)
{
    m_thumbnailPanel = thumbnailPanel;
}

QPointF QnTimeSlider::positionFromTime(milliseconds time, bool bound) const
{
    Q_D(const GraphicsSlider);

    d->ensureMapper();

    return QPointF(d->pixelPosMin
        + GraphicsStyle::sliderPositionFromValue(m_windowStart.count(), m_windowEnd.count(),
            time.count(), d->pixelPosMax - d->pixelPosMin, d->upsideDown, bound), 0.0);
}

milliseconds QnTimeSlider::timeFromPosition(const QPointF& position, bool bound) const
{
    Q_D(const GraphicsSlider);

    d->ensureMapper();

    return milliseconds(GraphicsStyle::sliderValueFromPosition(m_windowStart.count(),
        m_windowEnd.count(), position.x(), d->pixelPosMax - d->pixelPosMin, d->upsideDown, bound));
}

qreal QnTimeSlider::quickPositionFromTime(milliseconds logicalValue, bool bound) const
{
    return bound ? m_boundMapper(logicalValue.count()) : m_unboundMapper(logicalValue.count());
}

void QnTimeSlider::finishAnimations()
{
    animateStepValues(10 * 1000);
    animateThumbnails(10 * 1000);

    if (m_animatingSliderWindow)
    {
        setWindow(targetStart(), targetEnd());
        m_animatingSliderWindow = false;
    }

    m_kineticZoomHandler->kineticProcessor()->reset();
    m_kineticScrollHandler->kineticProcessor()->reset();
}

void QnTimeSlider::hurryKineticAnimations()
{
    m_kineticZoomHandler->hurry();
    m_kineticScrollHandler->hurry();
}

void QnTimeSlider::setTargetStart(milliseconds start)
{
    m_targetStart = (start == minimum()) ? milliseconds(std::numeric_limits<qint64>::min()) : start;
}

void QnTimeSlider::setTargetEnd(milliseconds end)
{
    m_targetEnd = (end == maximum()) ? milliseconds(std::numeric_limits<qint64>::max()) : end;
}

milliseconds QnTimeSlider::targetStart()
{
    return (m_targetStart == milliseconds(std::numeric_limits<qint64>::min())) ? minimum() : m_targetStart;
}

milliseconds QnTimeSlider::targetEnd()
{
    return (m_targetEnd == milliseconds(std::numeric_limits<qint64>::max())) ? maximum() : m_targetEnd;
}

void QnTimeSlider::generateProgressPatterns()
{
    static const qreal d0 = 0.0;
    static const qreal d1 = kLastMinuteStripeWidth;
    static const qreal d2 = kLastMinuteStripeWidth * 2;
    static const qreal d3 = kLastMinuteStripeWidth * 3;
    static const qreal d4 = kLastMinuteStripeWidth * 4;

    QPainterPath path;
    path.moveTo(d0, d1);
    path.lineTo(d1, d0);
    path.lineTo(d0, d0);
    path.closeSubpath();

    path.moveTo(d0, d2);
    path.lineTo(d0, d3);
    path.lineTo(d3, d0);
    path.lineTo(d2, d0);
    path.closeSubpath();

    path.moveTo(d0, d4);
    path.lineTo(d1, d4);
    path.lineTo(d4, d1);
    path.lineTo(d4, d0);
    path.closeSubpath();

    path.moveTo(d2, d4);
    path.lineTo(d3, d4);
    path.lineTo(d4, d3);
    path.lineTo(d4, d2);
    path.closeSubpath();

    static const QColor kLastMinuteStripeColor = colorTheme()->color("dark6", 38);

    m_progressPastPattern = QPixmap(d4, d4);
    m_progressPastPattern.fill(Qt::transparent);
    QPainter pastPainter(&m_progressPastPattern);
    pastPainter.setRenderHint(QPainter::Antialiasing);
    pastPainter.fillPath(path, kLastMinuteStripeColor);

    m_progressFuturePattern = QPixmap(d4, d4);
    m_progressFuturePattern.fill(Qt::transparent);
    QPainter futurePainter(&m_progressFuturePattern);
    futurePainter.setRenderHint(QPainter::Antialiasing);
    futurePainter.fillPath(path, kLastMinuteStripeColor);
}

bool QnTimeSlider::isAnimatingWindow() const
{
    return m_animatingSliderWindow
        || m_kineticZoomHandler->kineticProcessor()->isRunning()
        || m_kineticScrollHandler->kineticProcessor()->isRunning();
}

bool QnTimeSlider::scaleWindow(qreal factor, milliseconds anchor)
{
    qreal targetMSecsPerPixel = m_msecsPerPixel * factor;
    if (targetMSecsPerPixel < kMinTimePerPixelMs) {
        factor = kMinTimePerPixelMs / m_msecsPerPixel;
        if (qFuzzyCompare(factor, 1.0))
            return false; /* We've reached the min scale. */
    }

    milliseconds start = anchor + milliseconds(qint64((m_windowStart - anchor).count() * factor));
    milliseconds end = anchor + milliseconds(qint64((m_windowEnd - anchor).count() * factor));

    if (end > maximum())
    {
        start = maximum() - (end - start);
        end = maximum();
    }
    if (start < minimum())
    {
        end = minimum() + (end - start);
        start = minimum();
    }

    setWindow(start, end);

    /* If after two adjustments desired window end still lies outside the
     * slider range, then we've reached the max scale. */
    return end <= maximum();
}

QPointF QnTimeSlider::positionFromValue(qint64 logicalValue, bool bound) const
{
    return positionFromTime(milliseconds(logicalValue), bound);
}

qint64 QnTimeSlider::valueFromPosition(const QPointF& position, bool bound) const
{
    return timeFromPosition(position, bound).count();
}

QnTimeSlider::Marker QnTimeSlider::markerFromPosition(const QPointF& pos, qreal maxDistance) const
{
    if (m_selectionValid)
    {
        if (!selectionEnabledForPos(pos))
            return NoMarker;

        QPointF selectionStart = positionFromTime(m_selectionStart);
        if (qAbs(selectionStart.x() - pos.x()) < maxDistance)
            return SelectionStartMarker;

        QPointF selectionEnd = positionFromTime(m_selectionEnd);
        if (qAbs(selectionEnd.x() - pos.x()) < maxDistance)
            return SelectionEndMarker;
    }

    return NoMarker;
}

QPointF QnTimeSlider::positionFromMarker(Marker marker) const
{
    switch(marker)
    {
    case SelectionStartMarker:
        return m_selectionValid ? positionFromTime(m_selectionStart) : rect().topLeft();

    case SelectionEndMarker:
        return m_selectionValid ? positionFromTime(m_selectionEnd) : rect().topLeft();

    default:
        return rect().topLeft();
    }
}

QPointF QnTimeSlider::tooltipPointedTo() const
{
    if (ini().debugDisableQmlTooltips)
        return {};

    return mapFromScreen(m_tooltip->widget()->mapToGlobal(m_tooltip->pointerPos().toPoint()));
}

void QnTimeSlider::setMarkerSliderPosition(Marker marker, milliseconds position)
{
    switch(marker)
    {
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

void QnTimeSlider::extendSelection(milliseconds position)
{
    if (m_selectionValid)
    {
        if (position < m_selectionStart)
            setSelectionStart(position);
        else if (position > m_selectionEnd)
            setSelectionEnd(position);
    }
    else
    {
        milliseconds start = sliderTimePosition();
        milliseconds end = position;
        if (start > end)
            std::swap(start, end);
        setSelection(start, end);
        setSelectionValid(true);
    }
}

qreal QnTimeSlider::effectiveLineStretch(int line) const
{
    return m_lineData[line].visible ? m_lineData[line].stretch : 0.0;
}

int QnTimeSlider::tickmarkLevel(int stepIndex) const
{
    return qBound(0, m_maxStepIndex - stepIndex, kNumTickmarkLevels);
}

QColor QnTimeSlider::tickmarkLineColor(int level) const
{
    static const QList<QColor> kTickmarkLinesColors{{
        colorTheme()->color("light10"),
        colorTheme()->color("dark17"),
        colorTheme()->color("dark13"),
        colorTheme()->color("dark9"),
    }};

    return kTickmarkLinesColors[std::clamp(level, 0, kTickmarkLinesColors.size() - 1)];
}

QColor QnTimeSlider::tickmarkTextColor(int level) const
{
    static const QList<QColor> kTickmarkTextColors{{
        colorTheme()->color("light8"),
        colorTheme()->color("light16"),
        colorTheme()->color("dark15"),
    }};

    return kTickmarkTextColors[std::clamp(level, 0, kTickmarkTextColors.size() - 1)];
}

QRectF QnTimeSlider::dateBarRect() const
{
    QRectF r(rect());
    return QRectF(r.left(), 0, r.width(), kDateBarHeightPixels);
}

QRectF QnTimeSlider::tickmarkBarRect() const
{
    QRectF r(rect());
    return QRectF(r.left(), kDateBarHeightPixels, r.width(), kTickBarHeightPixels);
}

QRectF QnTimeSlider::lineBarRect() const
{
    QRectF r(rect());
    return QRectF(r.left(), kDateBarHeightPixels + kTickBarHeightPixels,
        r.width(), kLineBarHeightPixels);
}

qreal QnTimeSlider::expectedHeight()
{
    return kDateBarHeightPixels + kTickBarHeightPixels + kLineBarHeightPixels;
}

bool QnTimeSlider::isThumbnailsVisible() const
{
    return m_thumbnailsVisible;
}

qreal QnTimeSlider::thumbnailsHeight() const
{
    if (!m_thumbnailPanel)
        return 0;

    return m_thumbnailPanel->rect().height();
}

void QnTimeSlider::addThumbnail(ThumbnailPtr thumbnail)
{
    if (m_thumbnailsUpdateTimer->isActive())
        return;

    if (m_thumbnailData.contains(thumbnail->time()))
        return; /* There is no real point in overwriting existing thumbnails. Besides, it would result in flicker. */

    m_thumbnailData[thumbnail->time()] = ThumbnailData(thumbnail);
}

void QnTimeSlider::clearThumbnails()
{
    m_thumbnailData.clear();
}

void QnTimeSlider::addAllThumbnailsFromManager()
{
    if (!m_thumbnailManager)
        return;

    for (ThumbnailPtr thumbnail: m_thumbnailManager->panelThumbnails())
        addThumbnail(thumbnail);
}

void QnTimeSlider::freezeThumbnails()
{
    m_oldThumbnailData = m_thumbnailData.values();
    clearThumbnails();

    qreal center = m_thumbnailsPaintRect.center().x();
    qreal height = m_thumbnailsPaintRect.height();

    for (int i = 0; i < m_oldThumbnailData.size(); i++)
    {
        ThumbnailData& data = m_oldThumbnailData[i];
        data.hiding = true;

        qreal pos = m_thumbnailsUnboundMapper(data.thumbnail->time().count());
        qreal width = data.thumbnail->size().width() * height / data.thumbnail->size().height();
        data.pos = (pos - center) / width;
    }
}

void QnTimeSlider::animateLastMinute(int deltaMs)
{
    m_lastMinuteAnimationDelta += deltaMs;
}

const QVector<milliseconds>& QnTimeSlider::indicators() const
{
    return m_indicators;
}

void QnTimeSlider::setIndicators(const QVector<milliseconds>& indicators)
{
    m_indicators = indicators;
}

milliseconds QnTimeSlider::localOffset() const
{
    return m_localOffset;
}

void QnTimeSlider::setLocalOffset(milliseconds localOffset)
{
    if (m_localOffset == localOffset)
        return;

    m_localOffset = localOffset;
    setTimeRange(minimum(), maximum());

    updateToolTipText();
}

QnTimeSlider::TimeMarker* QnTimeSlider::tooltip() const
{
    return m_tooltip;
}

bool QnTimeSlider::isTooltipVisible() const
{
    return m_tooltipVisible;
}

void QnTimeSlider::setTooltipVisible(bool visible)
{
    if (m_tooltipVisible == visible)
        return;
    m_tooltipVisible = visible;
    updateToolTipVisibilityInternal();
}

void QnTimeSlider::stackWidgetUnderTooltip(QWidget* widget) const
{
    if (ini().debugDisableQmlTooltips)
        return;

    widget->stackUnder(m_tooltip->widget());
}

void QnTimeSlider::setLastMinuteIndicatorVisible(int line, bool visible)
{
    if (line >= kMaxLines)
        return;

    m_lastMinuteIndicatorVisible[line] = visible;
}

bool QnTimeSlider::isLastMinuteIndicatorVisible(int line) const
{
    if (line >= kMaxLines)
        return false;

    return m_lastMinuteIndicatorVisible[line];
}

QnBookmarksViewer* QnTimeSlider::bookmarksViewer()
{
    return m_bookmarksViewer;
}

void QnTimeSlider::setBookmarksHelper(const QnBookmarkMergeHelperPtr& helper)
{
    m_bookmarksHelper = helper;
}

bool QnTimeSlider::isBookmarksVisible() const
{
    return m_bookmarksVisible;
}

void QnTimeSlider::setBookmarksVisible(bool bookmarksVisible)
{
    if (m_bookmarksVisible == bookmarksVisible)
        return;

    m_bookmarksVisible = bookmarksVisible;

    update();
}

int QnTimeSlider::helpTopicAt(const QPointF& pos) const
{
    bool hasMotion = false;
    for (int i = 0; i < m_lineCount; i++)
    {
        if (!timePeriods(i, Qn::MotionContent).empty())
        {
            hasMotion = true;
            break;
        }
    }

    if (hasMotion)
        return Qn::MainWindow_MediaItem_SmartSearch_Help;

    if (!m_bookmarksHelper->bookmarksAtPosition(timeFromPosition(pos),
        milliseconds(qint64(m_msecsPerPixel))).isEmpty())
            return Qn::Bookmarks_Usage_Help;

    return Qn::MainWindow_Slider_Timeline_Help;
}


// -------------------------------------------------------------------------- //
// Updating
// -------------------------------------------------------------------------- //
void QnTimeSlider::updatePixmapCache()
{
    QFont localFont(font());
    m_pixmapCache->setDefaultFont(localFont);
    m_pixmapCache->setDefaultColor(palette().color(QPalette::BrightText));

    localFont.setWeight(kDateTextFontWeight);
    m_pixmapCache->setDateFont(localFont);
    m_pixmapCache->setDateColor(colorTheme()->color("light4"));

    m_noThumbnailsPixmap = m_pixmapCache->textPixmap(
        tr("No thumbnails available"),
        kNoThumbnailsFontPixelSize,
        colorTheme()->color("dark16"));

    for (int i = 0; i < kNumTickmarkLevels; ++i)
    {
        localFont.setWeight(kTickmarkFontWeights[i]);
        m_pixmapCache->setTickmarkColor(i, tickmarkTextColor(i));
        m_pixmapCache->setTickmarkFont(i, localFont);
    }

    updateLineCommentPixmaps();
    updateTickmarkTextSteps();
}

void QnTimeSlider::updateToolTipVisibilityInternal()
{
    if (ini().debugDisableQmlTooltips)
        return;

    if (m_updatingValue)
        return;

    const bool canBeVisible = (!isLive() || dragProcessor()->isRunning())
        && positionMarkerVisible() && isVisible();

    TimeMarkerMode mode = TimeMarkerMode::normal;
    if (canBeVisible)
    {
        const milliseconds pos = sliderTimePosition();
        if (pos < m_windowStart)
            mode = TimeMarkerMode::leftmost;
        else if (pos > m_windowEnd)
            mode = TimeMarkerMode::rightmost;
    }

    const bool visible = canBeVisible && m_tooltipVisible;
    if (visible && mode != m_tooltip->mode())
    {
        m_tooltip->setMode(mode);
        queueTooltipPositionUpdate();
    }

    m_tooltip->setShown(visible);
    m_livePreview->widget()->raise();
}

void QnTimeSlider::updateToolTipPosition()
{
    if (ini().debugDisableQmlTooltips)
        return;

    if (m_updatingValue) //< Prevent inconsistent position updates.
        return;

    const qreal x = positionFromValue(sliderPosition()).x();
    const QPoint positionAtTopLine = mapItemToGlobal(this, {x, 0.0});
    const QRect horizontalBounds(
        mapItemToGlobal(this, rect().topLeft()),
        mapItemToGlobal(this, rect().bottomRight()));

    m_tooltip->setPosition(positionAtTopLine.x(), positionAtTopLine.y(),
        horizontalBounds.left(), horizontalBounds.right());

    m_livePreview->widget()->raise();
}

void QnTimeSlider::queueTooltipPositionUpdate()
{
    m_tooltipPositionUpdateQueued = true;
}

void QnTimeSlider::updateToolTipText()
{
    if (!m_options.testFlag(UpdateToolTip))
        return;

    m_tooltip->setTimeContent(tooltipTimeContent(sliderTimePosition()));
}

workbench::timeline::TimeMarker::TimeContent QnTimeSlider::tooltipTimeContent(
    milliseconds pos) const
{
    TimeMarker::TimeContent content;
    const bool isUtc = m_options.testFlag(UseUTC);
    content.displayPosition = isUtc ? (pos + localOffset()) : pos;
    content.archivePosition = pos;
    content.isTimestamp = isUtc;
    if (!isUtc)
        content.localFileLength = maximum();
    content.showMilliseconds = m_steps[m_textMinStepIndex].unitMSecs == 1ms;
    content.showDate = m_steps[m_textMinStepIndex].unitMSecs > 24h;
    return content;
}

void QnTimeSlider::updateLineCommentPixmap(int line)
{
    int maxHeight = qFloor((kLineBarHeightPixels - 2.0) * m_lineData[line].stretch / m_totalLineStretch);
    if (maxHeight <= 0)
        return;

    QFont font(m_pixmapCache->defaultFont());
    font.setPixelSize(qMin(maxHeight, kLineLabelFontHeight));
    font.setWeight(kLineLabelFontWeight);

    m_lineData[line].commentPixmap = m_pixmapCache->textPixmap(m_lineData[line].comment, m_pixmapCache->defaultColor(), font);
}

void QnTimeSlider::updateLineCommentPixmaps()
{
    for (int i = 0; i < m_lineCount; i++)
        updateLineCommentPixmap(i);
}

void QnTimeSlider::updateSteps()
{
    m_steps = m_options.testFlag(UseUTC) ? timeSteps()->absolute() : timeSteps()->relative();

    m_nextTickmarkPos.resize(m_steps.size());
    m_tickmarkLines.resize(m_steps.size());
    m_stepData.resize(m_steps.size());
}

void QnTimeSlider::updateTickmarkTextSteps()
{
    // TODO: #sivanov This one is VERY slow right now.

    for (int i = 0; i < m_steps.size(); i++)
    {
        static const int referenceHeight = kTickmarkFontHeights[0];
        QString text = toLongestShortString(m_steps[i]);
        QPixmap pixmap = m_pixmapCache->textPixmap(text, referenceHeight);
        m_stepData[i].textWidthToHeight = static_cast<qreal>(pixmap.width()) / referenceHeight;
    }
}

bool QnTimeSlider::positionMarkerVisible() const
{
    return !m_options.testFlag(HideLivePosition) || !isLive() || (isSliderDown() && !m_dragIsClick);
}

void QnTimeSlider::setLivePreviewAllowed(bool allowed)
{
    if (allowed == m_livePreviewAllowed)
        return;

    m_livePreviewAllowed = allowed;
    updateLivePreview();
}

void QnTimeSlider::updateLivePreview()
{
    if (ini().debugDisableQmlTooltips)
        return;

    if (!m_useLivePreview)
        return;

    const QPoint globalCursorPos = QCursor::pos();
    const QPointF localCursorPos = WidgetUtils::mapFromGlobal(this, globalCursorPos);

    if (m_livePreviewAllowed
        && isVisible()
        && m_view->underMouse()
        && rect().contains(localCursorPos)
        && navigator()->currentResource()
        && m_bookmarksViewer->getDisplayedBookmarks().isEmpty()
        && !m_tooltip->widget()->underMouse())
    {
        QRectF globalRect = QRectF(
            mapItemToGlobal(this, rect().topLeft()),
            mapItemToGlobal(this, rect().bottomRight()));

        m_livePreview->showAt(
            globalCursorPos,
            globalRect,
            tooltipTimeContent(timeFromPosition(localCursorPos)));

        m_livePreview->widget()->raise();
    }
    else
    {
        m_livePreview->hide();
    }
}

bool QnTimeSlider::isLiveSupported() const
{
    return m_liveSupported;
}

void QnTimeSlider::setLiveSupported(bool value)
{
    if (m_liveSupported == value)
        return;

    m_liveSupported = value;
    updateLive();
    updateToolTipVisibilityInternal();
}

bool QnTimeSlider::isLive() const
{
    return m_isLive;
}

void QnTimeSlider::updateLive()
{
    const bool isLive = m_liveSupported && !m_selecting && value() == maximum();
    if (m_isLive == isLive)
        return;

    NX_VERBOSE(this, "Live changed to %1", isLive);
    if (!isLive)
        NX_VERBOSE(this, "Value %1 while maximum is %2", value(), maximum());
    m_isLive = isLive;
}

milliseconds QnTimeSlider::msecsPerPixel() const
{
    return milliseconds(qint64(m_msecsPerPixel));
}

bool QnTimeSlider::updateMSecsPerPixel()
{
    qreal msecsPerPixel = (m_windowEnd - m_windowStart).count() / qMax(size().width(), 1);
    msecsPerPixel = qMax(1.0, msecsPerPixel);

    if (qFuzzyCompare(m_msecsPerPixel, msecsPerPixel))
        return false;

    m_msecsPerPixel = msecsPerPixel;

    updateThumbnailsStepSize(false);
    updateStepAnimationTargets();
    updateAggregationValue();

    emit msecsPerPixelChanged();
    return true;
}

void QnTimeSlider::updateMinimalWindow()
{
    milliseconds minimalWindow = milliseconds(qint64(size().width() * kMinTimePerPixelMs));
    if (minimalWindow == m_minimalWindow)
        return;

    m_minimalWindow = minimalWindow;

    /* Re-bound. */
    setWindowStart(m_windowStart);
}

int QnTimeSlider::findTopLevelStepIndex() const
{
    int stepCount = m_steps.size();
    qreal width = rect().width();

    /* Detect which steps map into visible tickmark levels. */
    int minStepIndex = stepCount - 1;
    int maxStepIndexLimit = minStepIndex; /* m_maxStepIndex won't be greater than maxStepIndexLimit */
    int extraLevelsAllowed = -1; /* possible extra levels without text labels */

    for (; minStepIndex >= 0; --minStepIndex)
    {
        /* Pixel distance between tickmarks of this level: */
        qreal separationPixels = m_steps[minStepIndex].stepMSecs.count() / m_msecsPerPixel;

        /* Correct maxStepIndexLimit if needed: */
        if (separationPixels >= width)
        {
            --maxStepIndexLimit;
            continue;
        }

        /* Check against minimal allowed tickmark line distance: */
        if (separationPixels <= kMinTickmarkLineStepPixels)
            break;

        /* If extra levels didn't start yet: */
        if (extraLevelsAllowed < 0)
        {
            qreal labelWidth = m_stepData[minStepIndex].textWidthToHeight * kTickmarkFontHeights[kMaxDisplayedTextLevel];
            if ((minStepIndex < maxStepIndexLimit) && kMaxDisplayedTextLevel)
            {
                qreal prevLabelWidth = m_stepData[minStepIndex + 1].textWidthToHeight * kTickmarkFontHeights[kMaxDisplayedTextLevel - 1];
                labelWidth = qMax(labelWidth, (labelWidth + prevLabelWidth) / 2.0);
            }

            qreal minTextStepPixels = labelWidth + kMinTickmarkTextSpacingPixels;

            /* Check against minimal allowed tickmark text distance: */
            if (separationPixels < minTextStepPixels)
            {
                extraLevelsAllowed = kNumTickmarkLevels - kMaxDisplayedTextLevel - 1;
                if (extraLevelsAllowed <= 0)
                    break;
            }
        }
        else
        {
            /* If no more extra levels allowed: */
            if (--extraLevelsAllowed <= 0)
                break;
        }
    }

    ++minStepIndex;

    /* We want at least 2 levels: */
    maxStepIndexLimit = qMax(maxStepIndexLimit, 1);

    /* Correct max step index: */
    return qMin(minStepIndex + kNumTickmarkLevels - 1, maxStepIndexLimit);
}

void QnTimeSlider::updateStepAnimationTargets()
{
    bool updateNeeded = (qAbs(m_msecsPerPixel - m_animationUpdateMSecsPerPixel)
        / qMin(m_msecsPerPixel, m_animationUpdateMSecsPerPixel)) > kPerPixelChangeThresholdMs;
    if (!updateNeeded || m_steps.empty())
        return;

    m_maxStepIndex = findTopLevelStepIndex();

    /* Adjust target tickmark heights and opacities. */
    qreal prevTextVisible = 1.0; /* - "boolean" 0.0 or 1.0, used as multiplier */
    qreal prevLabelWidth = 0.0; /* - we track previous level text label widths to avoid overlapping with them */
    int minLevelStepIndexMinusOne = qMax(0, m_maxStepIndex - kNumTickmarkLevels);

    int prevLevel = -1;
    for (int i = m_steps.size() - 1; i >= minLevelStepIndexMinusOne; --i)
    {
        TimeStepData& data = m_stepData[i];
        qreal separationPixels = m_steps[i].stepMSecs.count() / m_msecsPerPixel;

        int level = tickmarkLevel(i);
        data.targetHeight = separationPixels >= kMinTickmarkLineStepPixels ? kTickmarkLengthPixels[level] : 0.0;

        qreal speedFactor = qMax(1.0, m_msecsPerPixel / m_animationUpdateMSecsPerPixel);
        qreal labelWidth = data.textWidthToHeight * kTickmarkFontHeights[level];

        if (level > kMaxDisplayedTextLevel)
        {
            data.targetHeight *= prevTextVisible;
            data.targetTextOpacity = 0.0;
            speedFactor *= qPow(2.0, level - kMaxDisplayedTextLevel);
        }
        else
        {
            qreal minTextStepPixels = qMax(labelWidth, (labelWidth + prevLabelWidth) / 2.0) + kMinTickmarkTextSpacingPixels;
            data.targetTextOpacity = separationPixels < minTextStepPixels ? 0.0 : 1.0;
            data.targetTextOpacity *= prevTextVisible;
            prevLabelWidth = prevLevel == level
                ? qMax(labelWidth, prevLabelWidth)
                : labelWidth;
        }

        prevLevel = level;

        data.targetLineOpacity = qFuzzyIsNull(data.targetHeight) ? 0.0 : 1.0;
        if (prevTextVisible && data.targetTextOpacity == 0)
        {
            m_textMinStepIndex = qMin(i + 1, m_steps.size() - 1);
            updateToolTipText();
        }
        prevTextVisible = data.targetTextOpacity;

        /* Compute speeds. */
        data.heightSpeed = qMax(data.heightSpeed, speedFactor *
            speed(data.currentHeight, data.targetHeight, kTickmarkHeightAnimationMs));
        data.lineOpacitySpeed = qMax(data.lineOpacitySpeed, speedFactor *
            speed(data.currentLineOpacity, data.targetLineOpacity, kTickmarkOpacityAnimationMs));
        data.textOpacitySpeed = qMax(data.textOpacitySpeed, speedFactor *
            speed(data.currentTextOpacity, data.targetTextOpacity, kTickmarkOpacityAnimationMs) * speedFactor);
    }

    /* Clean up remaining steps. */
    for (int i = minLevelStepIndexMinusOne - 1; i >= 0; --i)
    {
        TimeStepData& data = m_stepData[i];
        data.currentHeight = data.targetHeight = 0.0;
        data.currentLineOpacity = data.targetLineOpacity = 0.0;
        data.currentTextOpacity = data.targetTextOpacity = 0.0;
    }

    m_animationUpdateMSecsPerPixel = m_msecsPerPixel;
}

void QnTimeSlider::animateStepValues(int deltaMs)
{
    int stepCount = m_steps.size();
    for (int i = 0; i < stepCount; i++)
    {
        TimeStepData& data = m_stepData[i];
        data.currentHeight      = adjust(data.currentHeight,        data.targetHeight,      data.heightSpeed * deltaMs);
        data.currentLineOpacity = adjust(data.currentLineOpacity,   data.targetLineOpacity, data.lineOpacitySpeed * deltaMs);
        data.currentTextOpacity = adjust(data.currentTextOpacity,   data.targetTextOpacity, data.textOpacitySpeed * deltaMs);

        if (qFuzzyIsNull(data.currentHeight - data.targetHeight))
            data.heightSpeed = 0.0;
        if (qFuzzyIsNull(data.currentLineOpacity - data.targetLineOpacity))
            data.lineOpacitySpeed = 0.0;
        if (qFuzzyIsNull(data.currentTextOpacity - data.targetTextOpacity))
            data.textOpacitySpeed = 0.0;
    }
}

void QnTimeSlider::animateThumbnails(int deltaMs)
{
    if (m_thumbnailsOpacityIncrementSign != 0)
    {
        const qreal minOpacity = ini().timelineThumbnailBehindPreviewOpacity;
        const qreal opacityIncrement =
            (qreal) deltaMs / kFullThumbnailPanelFadingTime.count()
            * (1.0 - minOpacity)
            * m_thumbnailsOpacityIncrementSign;
        m_thumbnailsOpacity = qBound(minOpacity, m_thumbnailsOpacity + opacityIncrement, 1.0);
        if (m_thumbnailsOpacity == minOpacity || m_thumbnailsOpacity == 1.0)
            m_thumbnailsOpacityIncrementSign = 0;
    }

    qreal dt = deltaMs / 1000.0;

    bool oldShown = false;
    for (auto pos = m_oldThumbnailData.begin(); pos != m_oldThumbnailData.end(); pos++)
        oldShown |= animateThumbnail(dt, *pos);

    if (!oldShown)
    {
        m_oldThumbnailData.clear();

        for (auto pos = m_thumbnailData.begin(); pos != m_thumbnailData.end(); pos++)
            animateThumbnail(dt, *pos);
    }
}

bool QnTimeSlider::animateThumbnail(qreal dt, ThumbnailData& data)
{
    if (data.selecting && !data.hiding)
        data.selection = qMin(1.0, data.selection + dt * 4.0);
    else
        data.selection = qMax(0.0, data.selection - dt * 4.0);

    if (data.hiding)
    {
        data.opacity = qMax(0.0, data.opacity - dt * 4.0);
        return !qFuzzyIsNull(data.opacity);
    }

    data.opacity = qMin(1.0, data.opacity + dt * 4.0);
    return true;
}

void QnTimeSlider::updateAggregationValue()
{
    if (m_lineData.isEmpty())
        return;

    /* Aggregate to 1/16-pixels. */
    qreal aggregationMSecs = qMax(m_msecsPerPixel / 16.0, 1.0);

    /* Calculate only once presuming current value is the same on all lines. */
    qreal oldAggregationMSecs = m_lineData[0].timeStorage.aggregationMSecs();
    if (oldAggregationMSecs / 2.0 < aggregationMSecs && aggregationMSecs < oldAggregationMSecs * 2.0)
        return;

    for (int line = 0; line < m_lineCount; line++)
        m_lineData[line].timeStorage.setAggregationMSecs(aggregationMSecs);
}

void QnTimeSlider::updateTotalLineStretch()
{
    qreal totalLineStretch = 0.0;
    for (int line = 0; line < m_lineCount; line++)
        totalLineStretch += effectiveLineStretch(line);

    if (qFuzzyEquals(m_totalLineStretch, totalLineStretch))
        return;

    m_totalLineStretch = totalLineStretch;
    updateLineCommentPixmaps();
}

void QnTimeSlider::updateThumbnailsPeriod()
{
    if (!m_thumbnailManager)
        return;

    if (m_thumbnailsUpdateTimer->isActive() || !m_oldThumbnailData.isEmpty())
        return;

    m_thumbnailManager->setPanelWindowPeriod(m_thumbnailPanel->fullTimePeriod());
}

void QnTimeSlider::updateThumbnailsStepSize(bool instant)
{
    if (!m_thumbnailManager)
        return; /* Nothing to update. */

    if (m_thumbnailsUpdateTimer->isActive())
    {
        if (instant)
            m_thumbnailsUpdateTimer->stop();
        else
            return;
    }

    /* Calculate new bounding size. */
    int boundingHeigth = qRound(thumbnailsHeight());
    if (boundingHeigth < kThumbnailHeightForDrawing)
        boundingHeigth = 0;

    int pxPerTimeStep;
    milliseconds prevTimeStep;
    if (isThumbnailsVisible())
    {
        QnAspectRatio aspectRatio = m_thumbnailManager->aspectRatio();
        if (!aspectRatio.isValid())
            return;

        if (QnAspectRatio::isRotated90(m_thumbnailManager->rotation()))
            aspectRatio = aspectRatio.inverted();

        pxPerTimeStep = qRound(boundingHeigth * aspectRatio.toFloat());
        prevTimeStep = m_thumbnailManager->panelTimeStep();
    }
    else
    {
        pxPerTimeStep = LivePreview::kStepPixels;
        prevTimeStep = m_thumbnailManager->previewTimeStep();
    }

    const auto timeStep = milliseconds((qint64)(m_msecsPerPixel * pxPerTimeStep));
    const bool timeStepChanged =
        qAbs(timeStep.count() / m_msecsPerPixel - prevTimeStep.count() / m_msecsPerPixel) >= 1;

    const bool panelVisibilityChanged =
        isThumbnailsVisible() != m_thumbnailManager->wasPanelVisibleAtLastRefresh();

    const bool boundingHeightChanged = m_thumbnailManager->panelBoundingHeight() != boundingHeigth;
    if (timeStepChanged || boundingHeightChanged)
    {
        // Ok, thumbnails have to be re-generated. So we first freeze our old thumbnails.
        if (m_oldThumbnailData.isEmpty())
            freezeThumbnails();
    }
    else if (!panelVisibilityChanged && !instant && isLive())
    {
        return;
    }

    /* If animation is running, we want to wait until it's finished.
     * We also don't want to update thumbnails too often. */

    milliseconds currentTime = milliseconds(QDateTime::currentMSecsSinceEpoch());
    if (!instant || isAnimatingWindow() || currentTime - m_lastThumbnailsUpdateTime < 1s)
    {
        m_thumbnailsUpdateTimer->start();
        return;
    }

    m_lastThumbnailsUpdateTime = currentTime;
    m_thumbnailManager->refresh(boundingHeigth, timeStep, m_thumbnailPanel->fullTimePeriod());

    if (!isThumbnailsVisible())
        return;

    addAllThumbnailsFromManager();
}

void QnTimeSlider::setThumbnailSelecting(milliseconds time, bool selecting)
{
    if (time < 0ms)
        return;

    auto pos = m_thumbnailData.find(time);
    if (pos == m_thumbnailData.end())
        return;

    milliseconds actualTime = pos->thumbnail->actualTime();

    for (auto ipos = pos; ipos->thumbnail->actualTime() == actualTime; --ipos)
    {
        ipos->selecting = selecting;
        if (ipos == m_thumbnailData.begin())
            break;
    }

    for (auto ipos = pos + 1;
        ipos != m_thumbnailData.end() && ipos->thumbnail->actualTime() == actualTime; ++ipos)
    {
        ipos->selecting = selecting;
    }
}

void QnTimeSlider::updateThumbnailsVisibility()
{
    bool visible = thumbnailsHeight() >= kThumbnailHeightForDrawing;
    if (m_thumbnailsVisible == visible)
        return;

    m_thumbnailsVisible = visible;
    emit thumbnailsVisibilityChanged();
}

// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnTimeSlider::paint(QPainter* painter, const QStyleOptionGraphicsItem* , QWidget* widget)
{
    sendPendingMouseMoves(widget);

    m_unboundMapper = QnLinearFunction(m_windowStart.count(), positionFromTime(m_windowStart).x(),
        m_windowEnd.count(), positionFromTime(m_windowEnd).x());
    m_boundMapper = QnBoundedLinearFunction(m_unboundMapper, rect().left(), rect().right());

    QRectF lineBarRect = this->lineBarRect();
    qreal lineTop, lineUnit = qFuzzyIsNull(m_totalLineStretch) ? 0.0 : lineBarRect.height() / m_totalLineStretch;

    /* Draw background. */
    if (qFuzzyIsNull(m_totalLineStretch))
    {
        drawSolidBackground(painter, rect());
        painter->fillRect(lineBarRect, colorTheme()->color("green_core"));
    }
    else
    {
        drawSolidBackground(painter, tickmarkBarRect());

        /* Draw lines background (that is, time periods). */
        lineTop = lineBarRect.top();
        QnTimePeriodList extraContent;

        for (int line = 0; line < m_lineCount; line++)
        {
            qreal lineHeight = lineUnit * effectiveLineStretch(line);
            if (qFuzzyIsNull(lineHeight))
                continue;

            QRectF lineRect(lineBarRect.left(), lineTop, lineBarRect.width(), lineHeight);
            switch (m_selectedExtraContent)
            {
                case Qn::MotionContent:
                    extraContent = m_lineData[line].timeStorage.aggregated(Qn::MotionContent);
                    break;

                case Qn::AnalyticsContent:
                    extraContent = m_lineData[line].timeStorage.aggregated(Qn::AnalyticsContent);
                    break;

                default:
                    extraContent = QnTimePeriodList();
            }

            drawPeriodsBar(
                painter,
                m_lineData[line].timeStorage.aggregated(Qn::RecordingContent),
                extraContent,
                lineRect);

            lineTop += lineHeight;
        }

        lineTop = lineBarRect.top();
        for (int line = 0; line < m_lineCount; line++)
        {
            qreal lineHeight = lineUnit * effectiveLineStretch(line);
            if (qFuzzyIsNull(lineHeight))
                continue;

            QRectF lineRect(lineBarRect.left(), lineTop, lineBarRect.width(), lineHeight);

            if (m_lastMinuteIndicatorVisible[line])
                drawLastMinute(painter, lineRect);

            if (line)
                drawSeparator(painter, lineRect);

            lineTop += lineHeight;
        }
    }

    /* Draw line comments. */
    lineTop = lineBarRect.top();
    for (int line = 0; line < m_lineCount; line++)
    {
        qreal lineHeight = lineUnit * effectiveLineStretch(line);
        if (qFuzzyIsNull(lineHeight))
            continue;

        QPixmap pixmap = m_lineData[line].commentPixmap;

        const auto pixmapSize = pixmap.size() / pixmap.devicePixelRatio();
        QRectF fullRect(lineBarRect.left() + kLineLabelPaddingPixels, lineTop, pixmapSize.width(), lineHeight);
        QRectF centeredRect = Geometry::aligned(pixmapSize, fullRect);

        painter->drawPixmap(centeredRect, pixmap, pixmap.rect());

        lineTop += lineHeight;
    }

    /* Draw bookmarks. */
    drawBookmarks(painter, lineBarRect);

    /* Draw tickmarks. */
    drawTickmarks(painter, tickmarkBarRect());

    /* Draw dates. */
    drawDates(painter, dateBarRect());

    /* Draw selection. */
    drawSelection(painter);

    /* Draw position marker. */
    static const QColor kPositionMarkerColor = colorTheme()->color("light4");
    if (positionMarkerVisible())
    {
        drawMarker(painter,
            sliderTimePosition(), tooltipPointedTo().y() - 1.0, kPositionMarkerColor, 2.0);
    }

    /* Draw indicators. */
    static const QColor kIndicatorColor = colorTheme()->color("light4", 102);
    foreach (milliseconds position, m_indicators)
        drawMarker(painter, position, rect().top(), kIndicatorColor);

    // Update tooltip position after paint event (significantly reduces lags).
    if (std::exchange(m_tooltipPositionUpdateQueued, false))
        updateToolTipPosition();
}

bool QnTimeSlider::eventFilter(QObject* target, QEvent* event)
{
    if (target != m_tooltip->widget())
        return false;

    // Handle Time Marker mouse wheel rolling.
    if (event->type() == QEvent::Wheel)
    {
        const auto markerEvent = static_cast<QWheelEvent*>(event);
        QGraphicsSceneWheelEvent sceneEvent(QEvent::GraphicsSceneWheel);
        // Set only those sceneEvent fields which used by QnTimeSlider::wheelEvent().
        sceneEvent.setDelta(markerEvent->angleDelta().y());
        sceneEvent.setModifiers(markerEvent->modifiers());

        QPointF pos;
        switch (m_tooltip->mode())
        {
            case TimeMarkerMode::normal:
                pos = tooltipPointedTo();
                break;
            case TimeMarkerMode::leftmost:
                pos = rect().topLeft();
                break;
            case TimeMarkerMode::rightmost:
                pos = rect().topRight();
                break;
        }
        sceneEvent.setPos(pos);
        event->accept();
        wheelEvent(&sceneEvent);
        return true;
    }

    if (m_tooltip->mode() != TimeMarkerMode::normal)
    {
        if (event->type() == QEvent::MouseButtonPress)
            scrollIntoWindow(sliderTimePosition(), /*animated*/ true);

        return false;
    }

    // Handle Time Marker tooltip dragging.
    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        {
            auto mouseEvent = static_cast<QMouseEvent*>(event);
            m_dragDelta = m_tooltip->pointerPos() - mouseEvent->pos();
            m_dragMarker = DragMarker;
            dragProcessor()->mousePressEvent(m_view->viewport(), mouseEvent, /*instantDrag*/ true);

            // Click on tooltip mustn't change focused widget, but it resets focused widget at all
            // and 'now' in QApplication::focusChanged(QWidget *old, QWidget *now) signal is nullptr,
            // so the workaround activates last focused widget.
            if (m_lastFocusedWidget)
                m_lastFocusedWidget->activateWindow();

            return true;
        }

        case QEvent::MouseButtonRelease:
            dragProcessor()->mouseReleaseEvent(
                m_view->viewport(), static_cast<QMouseEvent*>(event));
            return true;

        case QEvent::MouseMove:
            dragProcessor()->mouseMoveEvent(m_view->viewport(), static_cast<QMouseEvent*>(event));
            return true;

        default:
            break;
    }
    return false;
}

void QnTimeSlider::drawSeparator(QPainter* painter, const QRectF& rect)
{
    if (qFuzzyEquals(rect.top(), this->rect().top()))
        return; /* Don't draw separator at the top of the widget. */

    static const QColor kSeparatorColor = colorTheme()->color("dark6");
    QnScopedPainterPenRollback penRollback(painter, QPen(kSeparatorColor, 0));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
    painter->drawLine(rect.topLeft(), rect.topRight());
}

void QnTimeSlider::drawLastMinute(QPainter* painter, const QRectF& rect)
{
    const qreal moveSpeed = 0.05;

    milliseconds startTime = maximum() - 3s;
    qreal startPos = quickPositionFromTime(startTime);
    if (startPos >= rect.right())
        return;

    qreal shift = qMod(m_lastMinuteAnimationDelta * moveSpeed, static_cast<qreal>(m_progressPastPattern.width()));
    m_lastMinuteAnimationDelta = shift / moveSpeed;
    QTransform brushTransform = QTransform::fromTranslate(-shift, 0.0);

    qreal sliderPos = quickPositionFromTime(sliderTimePosition());

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    static const QColor kLastMinuteBackgroundColor = colorTheme()->color("dark6", 127);

    if (sliderPos > startPos && !qFuzzyEquals(startPos, sliderPos))
    {
        QRectF pastRect(QPointF(startPos, rect.top()), QPointF(sliderPos, rect.bottom()));
        QBrush brush(m_progressPastPattern);
        brush.setTransform(brushTransform);
        painter->fillRect(pastRect, kLastMinuteBackgroundColor);
        painter->fillRect(pastRect, brush);
    }

    if (!qFuzzyEquals(rect.right(), sliderPos))
    {
        QRectF futureRect(QPointF(qMax(startPos, sliderPos), rect.top()), rect.bottomRight());
        QBrush brush(m_progressFuturePattern);
        brush.setTransform(brushTransform);
        painter->fillRect(futureRect, kLastMinuteBackgroundColor);
        painter->fillRect(futureRect, brush);
    }
}

void QnTimeSlider::drawSelection(QPainter* painter)
{
    if (!m_selectionValid)
        return;

    static const QColor kSelectionMarkerColor = colorTheme()->color("blue9", 153);
    static const QColor kSelectionColor = colorTheme()->color("blue8", 77);

    if (m_selectionStart == m_selectionEnd)
    {
        drawMarker(painter, m_selectionStart, rect().top(), kSelectionMarkerColor);
        return;
    }

    milliseconds selectionStart = qMax(m_selectionStart, m_windowStart);
    milliseconds selectionEnd = qMin(m_selectionEnd, m_windowEnd);
    if (selectionStart < selectionEnd)
    {
        QRectF rect = this->rect();
        QRectF selectionRect(
            QPointF(quickPositionFromTime(selectionStart), rect.top()),
            QPointF(quickPositionFromTime(selectionEnd), rect.bottom())
        );

        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
        painter->fillRect(selectionRect, kSelectionColor);
    }

    drawMarker(painter, m_selectionStart, rect().top(), kSelectionMarkerColor);
    drawMarker(painter, m_selectionEnd, rect().top(), kSelectionMarkerColor);
}

void QnTimeSlider::drawMarker(QPainter* painter,
    milliseconds pos,
    qreal topY,
    const QColor& color,
    qreal width /*= 1.0*/)
{
    if (!windowContains(pos))
        return;

    QPen pen(color, width);
    pen.setCapStyle(Qt::FlatCap);

    QnScopedPainterPenRollback penRollback(painter, pen);
    QnScopedPainterAntialiasingRollback aaRollback(painter, false);

    qreal x = quickPositionFromTime(pos);
    painter->drawLine(QPointF(x, topY), QPointF(x, rect().bottom()));
}

void QnTimeSlider::drawPeriodsBar(QPainter* painter, const QnTimePeriodList& recorded, const QnTimePeriodList& extra, const QRectF& rect)
{
    milliseconds minimumValue = windowStart();
    milliseconds maximumValue = windowEnd();

    /* The code here may look complicated, but it takes care of not rendering
     * different motion periods several times over the same location.
     * It makes transparent time slider look better. */

    /* Note that constness of period lists is important here as requesting
     * iterators from a non-const object will result in detach. */
    const QnTimePeriodList periods[Qn::TimePeriodContentCount] = {recorded, extra};

    QnTimePeriodList::const_iterator pos[Qn::TimePeriodContentCount];
    QnTimePeriodList::const_iterator end[Qn::TimePeriodContentCount];
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
    {
         pos[i] = periods[i].findNearestPeriod(minimumValue.count(), true);
         end[i] = periods[i].findNearestPeriod(maximumValue.count(), true);
         if (end[i] != periods[i].end() && end[i]->contains(maximumValue.count()))
             end[i]++;
    }

    milliseconds value = minimumValue;
    bool inside[Qn::TimePeriodContentCount];
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        inside[i] = pos[i] == end[i] ? false : pos[i]->contains(value.count());

    QnTimeSliderChunkPainter chunkPainter(this, painter);
    chunkPainter.start(value, milliseconds(qint64(m_msecsPerPixel)), rect);

    while (value != maximumValue)
    {
        milliseconds nextValue[Qn::TimePeriodContentCount] = {maximumValue, maximumValue};
        for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        {
            if (pos[i] == end[i])
                continue;

            if (!inside[i])
            {
                nextValue[i] = qMin(maximumValue, milliseconds(pos[i]->startTimeMs));
                continue;
            }

            if (!pos[i]->isInfinite())
                nextValue[i] = qMin(maximumValue, milliseconds(pos[i]->startTimeMs + pos[i]->durationMs));
        }

        milliseconds bestValue = qMin(nextValue[Qn::RecordingContent], nextValue[Qn::MotionContent]);

        Qn::TimePeriodContent content;
        if (inside[Qn::MotionContent])
            content = Qn::MotionContent;
        else if (inside[Qn::RecordingContent])
            content = Qn::RecordingContent;
        else
            content = Qn::TimePeriodContentCount;

        chunkPainter.paintChunk(bestValue - value, content);

        for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        {
            if (bestValue != nextValue[i])
                continue;

            if (inside[i])
                pos[i]++;

            inside[i] = !inside[i];
        }

        value = bestValue;
    }

    chunkPainter.stop();
}


void QnTimeSlider::drawSolidBackground(QPainter* painter, const QRectF& rect)
{
    qreal leftPos = quickPositionFromTime(windowStart());
    qreal rightPos = quickPositionFromTime(windowEnd());
    qreal centralPos = quickPositionFromTime(sliderTimePosition());

    if (!qFuzzyEquals(leftPos, centralPos))
        painter->fillRect(QRectF(leftPos, rect.top(), centralPos - leftPos, rect.height()), palette().window());

    if (!qFuzzyEquals(rightPos, centralPos))
        painter->fillRect(QRectF(centralPos, rect.top(), rightPos - centralPos, rect.height()), palette().window());
}

void QnTimeSlider::drawTickmarks(QPainter* painter, const QRectF& rect)
{
    int stepCount = m_steps.size();

    /* Find minimal tickmark step index. */
    int minStepIndex = qMax(m_maxStepIndex - kNumTickmarkLevels, 0); /* min level step index minus one */
    for (; minStepIndex < stepCount; minStepIndex++)
        if (!qFuzzyIsNull(m_stepData[minStepIndex].currentHeight))
            break;

    if (minStepIndex >= stepCount)
        minStepIndex = stepCount - 1; /* Tests show that we can actually get here. */

    /* Find initial and maximal positions. */
    QPointF overlap(kMaxTickmarkTextSizePixels / 2.0, 0.0);
    milliseconds startPos = qMax(minimum() + 1ms,
        timeFromPosition(positionFromTime(m_windowStart) - overlap, false)) + m_localOffset;
    milliseconds endPos = qMin(maximum() - 1ms,
        timeFromPosition(positionFromTime(m_windowEnd) + overlap, false)) + m_localOffset;

    /* Initialize next positions for tickmark steps. */
    for (int i = minStepIndex; i < stepCount; i++)
        m_nextTickmarkPos[i] = milliseconds(roundUp(startPos, m_steps[i]));

    /* Draw tickmarks. */
    for (int i = 0; i < m_tickmarkLines.size(); i++)
        m_tickmarkLines[i].clear();

    while(true)
    {
        milliseconds pos = m_nextTickmarkPos[minStepIndex];
        if (pos > endPos)
            break;

        /* Find index of the step to use. */
        int index = minStepIndex;
        for (; index < stepCount; index++)
        {
            if (m_nextTickmarkPos[index] == pos)
                m_nextTickmarkPos[index] = milliseconds(add(pos, m_steps[index]));
            else
                break;
        }

        index--;
        int level = tickmarkLevel(index);

        qreal x = quickPositionFromTime(pos - m_localOffset, false);

        /* Draw label if needed. */
        qreal lineHeight = m_stepData[index].currentHeight;
        if (!qFuzzyIsNull(m_stepData[index].currentTextOpacity) && kTickmarkFontHeights[level])
        {
            QPixmap pixmap = m_pixmapCache->tickmarkTextPixmap(level, pos,
                kTickmarkFontHeights[level], m_steps[index]);
            const auto pixmapSize = pixmap.size() / pixmap.devicePixelRatio();
            qreal topMargin = qFloor((kTickmarkTextHeightPixels[level] - pixmapSize.height()) * 0.5);
            QRectF textRect(x - pixmapSize.width() / 2.0, rect.top() + lineHeight + topMargin, pixmapSize.width(), pixmapSize.height());

            QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * m_stepData[index].currentTextOpacity);
            drawCroppedPixmap(painter, textRect, rect, pixmap, pixmap.rect());
        }

        /* Calculate line ends. */
        if (pos - m_localOffset >= m_windowStart && pos - m_localOffset <= m_windowEnd)
        {
            m_tickmarkLines[index] <<
                QPointF(x, rect.top()) <<
                QPointF(x, rect.top() + lineHeight);
        }
    }

    /* Draw tickmarks. */
    {
        QPen pen(Qt::SolidLine);
        pen.setCapStyle(Qt::FlatCap);

        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
        for (int i = minStepIndex; i < stepCount; i++)
        {
            pen.setColor(toTransparent(tickmarkLineColor(tickmarkLevel(i)), m_stepData[i].currentLineOpacity));
            painter->setPen(pen);
            painter->drawLines(m_tickmarkLines[i]);
        }
    }
}

void QnTimeSlider::drawDates(QPainter* painter, const QRectF& rect)
{
    const auto backgroundColor =
        [this](qint64 number) -> QColor
        {
           static const QList<QColor> kDateBarBackgrounds{{
                colorTheme()->color("dark11"),
                colorTheme()->color("dark9"),
           }};

            return kDateBarBackgrounds[number % kDateBarBackgrounds.size()];
        };

    int stepCount = m_steps.size();

    /* Find index of the highlight time step. */
    int highlightIndex = 0;
    qreal highlightSpanPixels = qMax(size().width() * kMinDateSpanFraction, kMinDateSpanPixels);
    for (; highlightIndex < stepCount; highlightIndex++)
        if (!m_steps[highlightIndex].longFormat.isEmpty()
            && m_steps[highlightIndex].stepMSecs.count() / m_msecsPerPixel >= highlightSpanPixels)
                break;

    highlightIndex = qMin(highlightIndex, stepCount - 1); //< TODO: #sivanov Remove this line.
    const QnTimeStep& highlightStep = m_steps[highlightIndex];

    qreal topMargin = qFloor((rect.height() - kDateTextFontHeight) * 0.5);

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);

    /* Draw highlight. */
    qint64 pos1 = roundUp(m_windowStart + m_localOffset, highlightStep);
    qint64 pos0 = sub(milliseconds(pos1), highlightStep);
    qreal x0 = quickPositionFromTime(m_windowStart);
    qint64 number = absoluteNumber(milliseconds(pos1), highlightStep);
    while(true)
    {
        qreal x1 = quickPositionFromTime(milliseconds(pos1) - m_localOffset);

        painter->setPen(Qt::NoPen);
        painter->setBrush(backgroundColor(number));
        painter->drawRect(QRectF(x0, rect.top(), x1 - x0, rect.height()));

        QPixmap pixmap = m_pixmapCache->
            dateTextPixmap(milliseconds(pos0), kDateTextFontHeight, highlightStep);
        const auto pixmapSize = pixmap.size() / pixmap.devicePixelRatio();
        QRectF textRect((x0 + x1) / 2.0 - pixmapSize.width() / 2.0,
            rect.top() + topMargin, pixmapSize.width(), pixmapSize.height());
        if (textRect.left() < rect.left())
            textRect.moveRight(x1);
        if (textRect.right() > rect.right())
            textRect.moveLeft(x0);

        drawCroppedPixmap(painter, textRect, rect, pixmap, pixmap.rect());

        if (pos1 >= (m_windowEnd + m_localOffset).count())
            break;

        pos0 = pos1;
        pos1 = add(milliseconds(pos1), highlightStep);
        number++;
        x0 = x1;
    }
}

void QnTimeSlider::setThumbnailsFaded(bool faded)
{
    const qreal targetOpacity = faded ? ini().timelineThumbnailBehindPreviewOpacity : 1.0;
    if (qFuzzyEquals(m_thumbnailsOpacity, targetOpacity))
    {
        m_thumbnailsOpacityIncrementSign = 0;
        return;
    }

    m_thumbnailsOpacityIncrementSign = (targetOpacity > m_thumbnailsOpacity) ? 1 : -1;
}

void QnTimeSlider::drawThumbnails(
    QPainter* painter,
    const QRectF& rect,
    const QnTimePeriod& timePeriod,
    const QnLinearFunction& unboundedPositionFromTimeFunc,
    const QnBoundedLinearFunction& boundedPositionFromTimeFunc)
{
    if (rect.height() < kThumbnailHeightForDrawing)
        return;

    m_thumbnailsPaintRect = rect;

    if (!m_thumbnailManager)
    {
        // Use logical coordinates for label size.
        const auto pixelRatio = m_noThumbnailsPixmap.devicePixelRatio();
        const QSizeF labelSizeBound(rect.width(), m_noThumbnailsPixmap.height() / pixelRatio);

        QRect labelRect = Geometry::aligned(Geometry::expanded(
            Geometry::aspectRatio(m_noThumbnailsPixmap.size()), labelSizeBound,
            Qt::KeepAspectRatio), rect, Qt::AlignCenter).toRect();

        drawCroppedPixmap(painter, labelRect, rect, m_noThumbnailsPixmap, m_noThumbnailsPixmap.rect());
        return;
    }

    milliseconds step = m_thumbnailManager->panelTimeStep();
    if (m_thumbnailManager->panelTimeStep() <= 0ms)
        return;

    if (m_thumbnailManager->panelThumbnailSize().isEmpty())
        return;

    qreal aspectRatio = Geometry::aspectRatio(m_thumbnailManager->panelThumbnailSize());
    qreal thumbnailWidth = rect.height() * aspectRatio;

    if (!m_oldThumbnailData.isEmpty() || m_thumbnailsUpdateTimer->isActive())
    {
        QRectF boundingRect = rect;
        for (int i = 0; i < m_oldThumbnailData.size(); i++)
        {
            const ThumbnailData& data = m_oldThumbnailData[i];
            if (data.thumbnail->isEmpty())
                continue;

            qreal x = rect.width() / 2 + data.pos * thumbnailWidth;;
            QSizeF targetSize(thumbnailWidth, rect.height());
            QRectF targetRect(x - targetSize.width() / 2, rect.top(), targetSize.width(), targetSize.height());

            drawThumbnail(painter, data, targetRect, boundingRect, boundedPositionFromTimeFunc);

            boundingRect.setLeft(qMax(boundingRect.left(), targetRect.right()));
        }
    }
    else
    {
        QnScopedPainterPenRollback penRollback(painter);
        QnScopedPainterBrushRollback brushRollback(painter);

        m_thumbnailsUnboundMapper = unboundedPositionFromTimeFunc;
        const auto startTime = milliseconds(qFloor(timePeriod.startTimeMs, step.count()));
        const auto endTime = milliseconds(qCeil(timePeriod.endTimeMs(), step.count()));

        QRectF boundingRect = rect;
        for (milliseconds time = startTime; time <= endTime; time += step)
        {
            auto pos = m_thumbnailData.find(time);
            if (pos == m_thumbnailData.end())
                continue;

            ThumbnailData& data = *pos;
            if (data.thumbnail->isEmpty())
                continue;

            qreal x = m_thumbnailsUnboundMapper(time.count());
            QSizeF targetSize(thumbnailWidth, rect.height());
            QRectF targetRect(x - targetSize.width() / 2, rect.top(), targetSize.width(), targetSize.height());

            drawThumbnail(painter, data, targetRect, boundingRect, boundedPositionFromTimeFunc);

            boundingRect.setLeft(qMax(boundingRect.left(), targetRect.right()));
        }
    }
}

void QnTimeSlider::drawThumbnail(
    QPainter* painter,
    const ThumbnailData& data,
    const QRectF& targetRect,
    const QRectF& boundingRect,
    const QnBoundedLinearFunction& boundedPositionFromTimeFunc)
{
    const QImage& image = data.thumbnail->image();

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * data.opacity * m_thumbnailsOpacity);

    QRectF rect;
    drawCroppedImage(painter, targetRect, boundingRect, image, image.rect(),& rect);

    if (!rect.isEmpty())
    {
        static const QColor kSelectionMarkerColor = colorTheme()->color("blue9", 153);

        qreal a = data.selection;
        qreal width = 1.0 + a * 2.0;
        QColor color = linearCombine(
            1.0 - a,
            QColor(255, 255, 255, 32), //< TODO: #customize
            a,
            kSelectionMarkerColor);
        rect = Geometry::eroded(rect, width / 2.0);

        painter->setPen(QPen(color, width));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect);

        qreal x = boundedPositionFromTimeFunc(data.thumbnail->actualTime().count());
        if (x >= rect.left() && x <= rect.right())
        {
            painter->setPen(Qt::NoPen);
            painter->setBrush(withAlpha(color, (color.alpha() + 255) / 2));
            painter->drawEllipse(QPointF(x, rect.bottom()), width * 2, width * 2);
        }
    }
    painter->setOpacity(opacity);
}

// TODO: #sivanov Check drawBookmarks() against m_localOffset.
void QnTimeSlider::drawBookmarks(QPainter* painter, const QRectF& rect)
{
    if (!m_bookmarksVisible)
        return;

    QnTimelineBookmarkItemList bookmarks = m_bookmarksHelper
        ? m_bookmarksHelper->bookmarks(milliseconds(qint64(m_msecsPerPixel)))
        : QnTimelineBookmarkItemList();
    if (bookmarks.isEmpty())
        return;

    QFontMetricsF fontMetrics(m_pixmapCache->defaultFont());

    static const QBrush kBookmarkBoundBrush(colorTheme()->color("blue10"));

    QFont font(m_pixmapCache->defaultFont());
    font.setWeight(kBookmarkFontWeight);

    const auto isBookmarkHovered =
        [this](const QnTimelineBookmarkItem& timelineBookmark) -> bool
        {
            const auto displayedBookmarks = m_bookmarksViewer->getDisplayedBookmarks();
            return std::any_of(displayedBookmarks.cbegin(), displayedBookmarks.cend(),
                [timelineBookmark](const auto& displayedBookmark)
                {
                    if (timelineBookmark.isCluster())
                    {
                        const auto cluster = timelineBookmark.cluster();
                        return displayedBookmark.startTimeMs >= cluster.startTime
                            && displayedBookmark.endTime() <= cluster.endTime();
                    }
                    else
                    {
                        return displayedBookmark.guid == timelineBookmark.bookmark().guid;
                    }
                });
        };

    /* Draw bookmarks: */
    for (int i = 0; i < bookmarks.size(); ++i)
    {
        const QnTimelineBookmarkItem& bookmarkItem = bookmarks[i];

        if (bookmarkItem.startTime() >= m_windowEnd || bookmarkItem.endTime() <= m_windowStart)
            continue;

        QRectF bookmarkRect = rect;
        bookmarkRect.setLeft(quickPositionFromTime(qMax(bookmarkItem.startTime(), m_windowStart)));
        bookmarkRect.setRight(quickPositionFromTime(qMin(bookmarkItem.endTime(), m_windowEnd)));

        static const QColor kBookmarkColor = colorTheme()->color("blue12", 204);
        static const QColor kHoveredBookmarkColor = colorTheme()->color("blue10", 230);

        const bool isHovered = isBookmarkHovered(bookmarkItem);
        const QColor& bgColor = isHovered
            ? kHoveredBookmarkColor
            : kBookmarkColor;

        painter->fillRect(bookmarkRect, bgColor);
        painter->fillRect(bookmarkRect.left(), bookmarkRect.top(), 1, bookmarkRect.height(),
            kBookmarkBoundBrush);
        painter->fillRect(bookmarkRect.right() - 1, bookmarkRect.top(), 1, bookmarkRect.height(),
            kBookmarkBoundBrush);

        if (bookmarkItem.isCluster())
            continue;

        const QnCameraBookmark& bookmark = bookmarkItem.bookmark();

        QRectF textRect = bookmarkRect;
        if (i < bookmarks.size() - 1)
        {
            textRect.setRight(qMin(bookmarkRect.right(),
                quickPositionFromTime(milliseconds(bookmarks[i + 1].startTime()))));
        }
        textRect.adjust(kBookmarkTextPadding, 0, -kBookmarkTextPadding, 0);

        QString text = fontMetrics.elidedText(bookmark.name, Qt::ElideRight, textRect.width());
        static const int elideStringLength = 3; /* "..." */
        if (text != bookmark.name && text.length() - elideStringLength < kMinBookmarkTextCharsVisible)
            continue;

        QPixmap pixmap = m_pixmapCache->textPixmap(text, kBookmarkFontHeight, palette().color(QPalette::BrightText), font);
        const auto dpHeight = pixmap.height() / pixmap.devicePixelRatio();
        qreal textY = qRound(bookmarkRect.top() + (bookmarkRect.height() - dpHeight + 1.0) / 2.0);
        painter->drawPixmap(textRect.left(), textY, pixmap);
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
QSizeF QnTimeSlider::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    switch(which)
    {
    case Qt::MinimumSize:
    case Qt::PreferredSize:
    {
        QSizeF result = base_type::sizeHint(which, constraint);
        result.setHeight(kDateBarHeightPixels + kTickBarHeightPixels + kLineBarHeightPixels);
        return result;
    }
    default:
        return base_type::sizeHint(which, constraint);
    }
}

void QnTimeSlider::tick(int deltaMs)
{
    if (m_options.testFlag(AdjustWindowToPosition) && !m_animatingSliderWindow && !isSliderDown())
    {
        /* Apply window position corrections if no animation or user interaction is in progress. */
        milliseconds position = sliderTimePosition();
        if (position >= m_windowStart && position <= m_windowEnd)
        {
            milliseconds windowRange = m_windowEnd - m_windowStart;

            qreal speed;
            if (position < m_windowStart + windowRange / 3)
            {
                speed = -1.0 + static_cast<qreal>((position - m_windowStart).count())
                    / (windowRange.count() / 3);
            }
            else
            if (position < m_windowEnd - windowRange / 3)
                speed = 0.0;
            else
            {
                speed = 1.0 - static_cast<qreal>((m_windowEnd - position).count())
                    / (windowRange.count() / 3);
            }

            speed = speed * 0.5 * qMax(windowRange, milliseconds(64s)).count();

            milliseconds delta = milliseconds(qint64(speed * deltaMs / 1000.0));
            if (m_windowEnd + delta > maximum())
                delta = maximum() - m_windowEnd;
            if (m_windowStart + delta < minimum())
                delta = minimum() - m_windowStart;

            setWindow(m_windowStart + delta, m_windowEnd + delta);
        }
    }

    if (m_animatingSliderWindow)
    {
        /* 0.5^t convergence: */
        static const qreal kHalfwayTimeMs = 15.0;
        qreal timeFactor = pow(0.5, deltaMs / kHalfwayTimeMs);
        milliseconds untilStart = milliseconds(qint64((targetStart() - m_windowStart).count() * timeFactor));
        milliseconds untilEnd = milliseconds(qint64((targetEnd() - m_windowEnd).count() * timeFactor));

        milliseconds desiredWindowSize = targetEnd() - targetStart();
        milliseconds maxRemainder =
            milliseconds(qMax(qAbs(untilStart.count()), qAbs(untilEnd.count())));

        if (maxRemainder < qMax(desiredWindowSize / 1000, milliseconds(1s)))
        {
            m_animatingSliderWindow = false;
            untilStart = untilEnd = 0ms;
        }

        setWindow(targetStart() - untilStart, targetEnd() - untilEnd);
    }

    animateStepValues(deltaMs);
    animateThumbnails(deltaMs);
    animateLastMinute(deltaMs);
}

void QnTimeSlider::sliderChange(SliderChange change)
{
    base_type::sliderChange(change);

    switch (change)
    {
        case SliderRangeChange:
        {
            /* If a window was invalidated: */
            if (m_oldMinimum < 0ms && m_oldMinimum == m_oldMaximum)
            {
                auto min = minimum();
                auto max = maximum();
                m_oldMinimum = min;
                m_oldMaximum = max;
                m_zoomAnchor = (min + max) / 2;
                setWindow(min, max);
                setSelectionValid(false);
                finishAnimations();
                break;
            }

            milliseconds windowStart = m_windowStart;
            milliseconds windowEnd = m_windowEnd;

            const bool keepAtMinimum =
                windowStart == m_oldMinimum && m_options.testFlag(StickToMinimum);
            const bool keepAtMaximum =
                windowEnd == m_oldMaximum && m_options.testFlag(StickToMaximum);

            if (m_options.testFlag(PreserveWindowSize))
            {
                // PreserveWindowSize && keepAtMinimum && keepAtMaximum are impossible together,
                // do not stick to minimum in such case.
                if (keepAtMaximum)
                {
                    windowStart += maximum() - windowEnd;
                    windowEnd = maximum();
                }
                else if (keepAtMinimum)
                {
                    windowEnd += minimum() - windowStart;
                    windowStart = minimum();
                }
            }
            else
            {
                if (keepAtMaximum)
                    windowEnd = maximum();
                if (keepAtMinimum)
                    windowStart = minimum();
            }

            /* Stick zoom anchor. */
            if (m_zoomAnchor == m_oldMinimum)
                m_zoomAnchor = minimum();
            if (m_zoomAnchor == m_oldMaximum)
                m_zoomAnchor = maximum();

            m_oldMinimum = minimum();
            m_oldMaximum = maximum();

            /* Re-bound window. */
            setWindow(windowStart, windowEnd);

            /* Re-bound selection. Invalidate if it's outside the new range. */
            if ((m_selectionStart >= minimum() && m_selectionStart <= maximum()) || (m_selectionEnd >= minimum() && m_selectionEnd <= maximum()))
                setSelection(m_selectionStart, m_selectionEnd);
            else
                setSelectionValid(false);
            break;
        }

        case SliderValueChange:
            updateLive();
            queueTooltipPositionUpdate();
            updateToolTipVisibilityInternal();
            updateToolTipText();
            break;

        case SliderMappingChange:
            queueTooltipPositionUpdate();
            break;

        default:
            break;
    }

    updateLivePreview();
}

QVariant QnTimeSlider::itemChange(GraphicsItemChange change, const QVariant& value)
{
    const QVariant result = base_type::itemChange(change, value);
    if (change == ItemScenePositionHasChanged)
        queueTooltipPositionUpdate();
    return result;
}

void QnTimeSlider::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    if (m_animatingSliderWindow)
    {
        event->accept();
        return; /* Do nothing if animated zoom is in progress. */
    }

    /* delta() returns the distance that the wheel is rotated
     * in eighths (1/8s) of a degree. */
    const qreal degrees = event->delta() / 8.0;

    if (event->modifiers().testFlag(Qt::ControlModifier)
        || event->modifiers().testFlag(Qt::ShiftModifier))
    {
        /* Kinetic drag: */
        m_kineticScrollHandler->unhurry();
        m_kineticScrollHandler->kineticProcessor()->shift(degrees / kWheelDegreesFor1PixelScroll);
        m_kineticScrollHandler->kineticProcessor()->start();
    }
    else
    {
        /* Kinetic zoom: */
        m_zoomAnchor = timeFromPosition(event->pos());

        /* Snap zoom anchor to window sides. */
        if (m_options.testFlag(SnapZoomToSides))
        {
            qreal windowRange = (m_windowEnd - m_windowStart).count();
            if ((m_zoomAnchor - m_windowStart).count() / windowRange < kZoomSideSnapDistance)
                m_zoomAnchor = m_windowStart;
            else if ((m_windowEnd - m_zoomAnchor).count() / windowRange < kZoomSideSnapDistance)
                m_zoomAnchor = m_windowEnd;
        }

        m_kineticZoomHandler->unhurry();
        m_kineticZoomHandler->kineticProcessor()->shift(degrees);
        m_kineticZoomHandler->kineticProcessor()->start();
    }

    event->accept();
}

void QnTimeSlider::handleThumbnailsWheel(QGraphicsSceneWheelEvent* event)
{
    wheelEvent(event);
}

void QnTimeSlider::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    base_type::resizeEvent(event);

    if (event->oldSize() != event->newSize())
    {
        updateMSecsPerPixel();
        updateMinimalWindow();
    }
}

void QnTimeSlider::handleThumbnailsResize()
{
    updateThumbnailsVisibility();
    updateThumbnailsStepSize(/*instant*/ false);
    emit thumbnailsHeightChanged();
}

void QnTimeSlider::keyPressEvent(QKeyEvent* event)
{
    switch(event->key())
    {
    case Qt::Key_BracketLeft:
    case Qt::Key_BracketRight:
        if (m_options.testFlag(SelectionEditable))
        {
            if (!isSelectionValid())
            {
                setSelection(sliderTimePosition(), sliderTimePosition());
                setSelectionValid(true);
            }
            else
            {
                if (event->key() == Qt::Key_BracketLeft)
                    setSelectionStart(sliderTimePosition());
                else
                    setSelectionEnd(sliderTimePosition());
            }

            /* To handle two cases - when the first begin/end marker was either placed for the first time
             *  or when it was moved to another location before complementary end/begin marker was placed -
             *  we can do this simple check: */
            m_selectionInitiated = m_selectionStart == m_selectionEnd;

            /* Accept selection events. */
            event->accept();
            return;
        }
        break;
    }

    /* Ignore all other key events; don't call inherited implementation. */
    event->ignore();
}

void QnTimeSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    base_type::mouseDoubleClickEvent(event);

    if (m_options.testFlag(UnzoomOnDoubleClick) && event->button() == Qt::LeftButton)
        setWindow(minimum(), maximum(), true);
}

void QnTimeSlider::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    base_type::hoverEnterEvent(event);
    m_hoverMousePos = event->pos();
    unsetCursor();
}

void QnTimeSlider::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    base_type::hoverLeaveEvent(event);
    m_hoverMousePos.reset();
    unsetCursor();
    m_livePreview->hide();
    setThumbnailSelecting(m_lastHoverThumbnail, false);
    m_lastHoverThumbnail = -1ms;
}

void QnTimeSlider::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    base_type::hoverMoveEvent(event);

    m_hoverMousePos = event->pos();
    updateBookmarksViewerLocation();

    // If mouse really moved, allow Live Preview previously disallowed due to mouse button pressed.
    if (event->pos() != event->lastPos())
        setLivePreviewAllowed(true);
    updateLivePreview();

    if (isWindowBeingDragged())
        return;

    setThumbnailSelecting(m_lastHoverThumbnail, false);
    m_lastHoverThumbnail = -1ms;

    switch (markerFromPosition(event->pos(), kHoverEffectDistance))
    {
        case SelectionStartMarker:
        case SelectionEndMarker:
            setCursor(Qt::SplitHCursor);
            break;

        default:
            unsetCursor();
            break;
    }
}

void QnTimeSlider::handleThumbnailsHoverMove(milliseconds hoveredTime)
{
    if (isWindowBeingDragged())
        return;

    if (m_thumbnailManager
        && m_thumbnailManager->panelTimeStep() != 0ms && m_oldThumbnailData.isEmpty()
        && !m_thumbnailsUpdateTimer->isActive())
    {
        const auto time = milliseconds(qRound(
            hoveredTime, m_thumbnailManager->panelTimeStep().count()));

        setThumbnailSelecting(m_lastHoverThumbnail, false);
        setThumbnailSelecting(time, true);
        m_lastHoverThumbnail = time;
        setCursor(Qt::PointingHandCursor);
    }
    else
    {
        setThumbnailSelecting(m_lastHoverThumbnail, false);
        m_lastHoverThumbnail = -1ms;
    }
}

void QnTimeSlider::updateBookmarksViewerLocation()
{
    if (m_hoverMousePos.has_value() && lineBarRect().contains(*m_hoverMousePos))
    {
        qint64 location = (*m_hoverMousePos).x();
        m_bookmarksViewer->setTargetLocation(location);
    }
}

void QnTimeSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    bool immediateDrag = true;
    m_dragDelta = QPointF();

    /* Modify current selection with Shift or Control. */
    bool extendSelectionRequested = event->modifiers().testFlag(Qt::ShiftModifier)
        || event->modifiers().testFlag(Qt::ControlModifier);

    bool canSelect = m_options.testFlag(SelectionEditable);
    if (event->button() == Qt::LeftButton)
        canSelect &= m_options.testFlag(LeftButtonSelection);
    else if (event->button() == Qt::RightButton)
        canSelect &= !m_options.testFlag(LeftButtonSelection);

    if (canSelect && extendSelectionRequested)
        extendSelection(timeFromPosition(event->pos()));

    switch (event->button())
    {
        case Qt::LeftButton:
            m_dragMarker = markerFromPosition(event->pos(), kHoverEffectDistance);

            if (canSelect && m_dragMarker == NoMarker && !extendSelectionRequested
                && selectionEnabledForPos(event->pos()))
            {
                m_dragMarker = CreateSelectionMarker;
            }

            immediateDrag = (m_dragMarker == NoMarker) && !extendSelectionRequested;
            if (m_dragMarker == NoMarker)
                m_dragMarker = DragMarker; // TODO: #vkutin #3.2 Simplify all this logic!
            break;

        case Qt::RightButton:
            immediateDrag = false;
            m_dragMarker = canSelect ?
                CreateSelectionMarker :
                NoMarker;
            break;

        default:
            m_dragMarker = DragMarker;
            break;
    }

    dragProcessor()->mousePressEvent(this, event, immediateDrag);
    event->accept();
}

void QnTimeSlider::handleThumbnailsMousePress(milliseconds pressedTime, Qt::MouseButton button)
{
    if (!m_thumbnailManager || m_thumbnailManager->panelTimeStep() == 0ms)
        return;

    const auto time = milliseconds(qRound(
        pressedTime, m_thumbnailManager->panelTimeStep().count()));
    const auto pos = m_thumbnailData.find(time);
    if (pos != m_thumbnailData.end())
    {
        if (button == Qt::LeftButton)
        {
            setSliderPosition(pos->thumbnail->actualTime(), /*keepInWindow*/ true);
            emit thumbnailClicked();
        }
    }
}

void QnTimeSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    dragProcessor()->mouseMoveEvent(this, event);
    event->accept();
}

void QnTimeSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (dragProcessor()->isWaiting())
    {
        event->accept();
        return;
    }

    dragProcessor()->mouseReleaseEvent(this, event);

    if (m_dragIsClick && event->button() == Qt::LeftButton)
        scrollIntoWindow(timeFromPosition(event->pos()), true);

    if (m_options.testFlag(LeftButtonSelection) && m_options.testFlag(ClearSelectionOnClick)
        && m_dragMarker == CreateSelectionMarker && !m_selectionInitiated)
    {
        setSelectionValid(false);
    }

    m_dragMarker = NoMarker;

    if (m_dragIsClick && event->button() == Qt::RightButton)
    {
        emit customContextMenuRequested(event->pos(), event->screenPos());
        m_selectionInitiated = m_selectionValid && m_selectionStart == m_selectionEnd;
    }

    event->accept();
}

void QnTimeSlider::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    event->ignore();
    return;
}

void QnTimeSlider::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);

    switch (event->type())
    {
    case QEvent::FontChange:
    case QEvent::PaletteChange:
        updatePixmapCache(); // - to update when standard palette is customized (not at the same moment as setColors)
        break;
    default:
        break;
    }
}

void QnTimeSlider::startDragProcess(DragInfo* info)
{
    m_dragIsClick = !m_kineticScrollHandler->kineticProcessor()->isRunning();
    m_kineticScrollHandler->kineticProcessor()->reset();

    /* We have to use mapping because these events can be caused by the tooltip as well as by the slider itself. */
    if (m_dragMarker == CreateSelectionMarker)
    {
        setSliderDown(true);
        setLivePreviewAllowed(false);
        setSliderPosition(valueFromPosition(mapFromScene(info->mousePressScenePos())));
    }
}

void QnTimeSlider::startDrag(DragInfo* info)
{
    /* We have to use mapping because these events can be caused by the tooltip as well as by the slider itself. */
    const QPointF mousePos = (m_dragMarker == DragMarker)
        ? mapFromScreen(info->mousePressScreenPos())
        : mapFromScene(info->mousePressScenePos());
    milliseconds pos = timeFromPosition(mousePos + m_dragDelta);

    m_dragIsClick = false;

    if (m_dragMarker != NoMarker)
        setSliderDown(true);

    switch (m_dragMarker)
    {
        case NoMarker:
            m_dragWindowStart = m_windowStart;
            setCursor(Qt::ClosedHandCursor);
            break;

        case DragMarker:
            setLivePreviewAllowed(false);
            break;

        case CreateSelectionMarker:
            setSelection(pos, pos);
            setSelectionValid(true);
            m_dragMarker = SelectionStartMarker;
            /* FALL THROUGH */
        case SelectionStartMarker:
        case SelectionEndMarker:
            m_selecting = true;
            m_dragDelta = positionFromMarker(m_dragMarker) - mousePos;
            emit selectionPressed();
            break;
    }
}

void QnTimeSlider::dragMove(DragInfo* info)
{
    // We have to use mapping because these events can be caused by the tooltip as well as by the slider itself.
    // We have to use scene coordinates because of possible redrag events when real tooltip coordinates
    // differ from those stored in the DragInfo object.
    const QPointF mousePos = (m_dragMarker == DragMarker)
        ? mapFromScreen(info->mouseScreenPos())
        : mapFromScene(info->mouseScenePos());

    if (m_dragMarker == NoMarker)
    {
        const auto delta = timeFromPosition(mapFromScene(info->mousePressScenePos()))
            - timeFromPosition(mousePos);
        const auto relativeDelta = m_dragWindowStart - m_windowStart + delta;
        const auto prevWindowStart = m_windowStart;
        shiftWindow(relativeDelta, false);

        m_kineticScrollHandler->unhurry();
        m_kineticScrollHandler->kineticProcessor()->shift(
            qreal((m_windowStart - prevWindowStart).count()) / m_msecsPerPixel);
        return;
    }

    const QPointF targetMousePosition = mousePos + m_dragDelta;
    const QPointF sliderMousePosition = positionFromValue(sliderPosition());
    const bool targetXPositionIsDifferent =
        lround(targetMousePosition.x()) != lround(sliderMousePosition.x());

    if (m_options.testFlag(DragScrollsWindow) && targetXPositionIsDifferent)
    {
        const qreal newX = targetMousePosition.x()
            + kWindowScrollPixelThreshold * (int) nx::utils::math::sign(m_dragDelta.x());
        const QPointF newPos(newX, 0);
        if (!rect().contains(newPos))
        {
            ensureWindowContains(timeFromPosition(newPos, /*bound*/ false));
            const auto redragCallback =
                [this]()
                {
                    // Delayed execution can happen after mouse released, don't redrag if so.
                    if (dragProcessor()->isRunning())
                        dragProcessor()->redrag();
                };
            executeDelayedParented(redragCallback, this);
        }
    }

    setLivePreviewAllowed(false);

    if (targetXPositionIsDifferent)
        setSliderPosition(valueFromPosition(targetMousePosition));

    if (m_dragMarker == SelectionStartMarker || m_dragMarker == SelectionEndMarker)
    {
        milliseconds selectionStart = m_selectionStart;
        milliseconds selectionEnd = m_selectionEnd;
        Marker otherMarker = NoMarker;

        switch (m_dragMarker)
        {
            case SelectionStartMarker:
                selectionStart = sliderTimePosition();
                otherMarker = SelectionEndMarker;
                break;

            case SelectionEndMarker:
                selectionEnd = sliderTimePosition();
                otherMarker = SelectionStartMarker;
                break;

            default:
                break;
        }

        if (selectionStart > selectionEnd)
        {
            qSwap(selectionStart, selectionEnd);
            m_dragMarker = otherMarker;
        }

        setSelection(selectionStart, selectionEnd);
    }
}

void QnTimeSlider::finishDragProcess(DragInfo* info)
{
    Q_UNUSED(info);

    if (m_dragMarker == NoMarker)
    {
        unsetCursor();
        m_kineticScrollHandler->kineticProcessor()->start();
    }

    if (m_selecting)
    {
        m_selecting = false;
        emit selectionReleased();
    }

    setSliderDown(false);
    m_dragDelta = QPointF();
}

QPointF QnTimeSlider::mapFromScreen(const QPoint& screenPos) const
{
    return mapFromItem(m_tooltipParent, m_view->mapFromGlobal(screenPos));
}

bool QnTimeSlider::selectionEnabledForPos(const QPointF& pos) const
{
    return lineBarRect().contains(pos) || ini().enableTimelineSelectionAtTickmarkArea;
}

Qn::TimePeriodContent QnTimeSlider::selectedExtraContent() const
{
    return m_selectedExtraContent;
}

void QnTimeSlider::setSelectedExtraContent(Qn::TimePeriodContent value)
{
    if (m_selectedExtraContent == value)
        return;

    m_selectedExtraContent = value;
    update();
}
