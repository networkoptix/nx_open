#include "time_slider.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <array>

#include <ini.h>

#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/QtMath>
#include <QtCore/QScopedValueRollback>

#include <QtGui/QPainter>

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QGraphicsSceneWheelEvent>

#include <camera/thumbnails_loader.h>

#include <core/resource/camera_bookmark.h>

#include <recording/time_period_list.h>

#include <text/time_strings.h>

#include <ui/animation/opacity_animator.h>
#include <ui/style/globals.h>
#include <ui/style/helper.h>
#include <ui/graphics/items/controls/bookmarks_viewer.h>
#include <ui/graphics/items/controls/time_slider_pixmap_cache.h>
#include <ui/graphics/items/standard/graphics_slider_p.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/processors/kinetic_cutting_processor.h>
#include <ui/processors/drag_processor.h>
#include <ui/utils/bookmark_merge_helper.h>
#include <nx/client/desktop/ui/workbench/workbench_animations.h>
#include <nx/client/core/utils/geometry.h>

#include <ui/help/help_topics.h>

#include <utils/common/delayed.h>
#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/checked_cast.h>
#include <utils/math/math.h>
#include <utils/math/color_transformations.h>

using nx::client::core::Geometry;

namespace
{
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
    const std::array<int, kNumTickmarkLevels + 1> kTickmarkFontWeights =
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
    const qreal kDateTextFontWeight = QFont::DemiBold;

    const qreal kMinDateSpanFraction = 0.15;
    const qreal kMinDateSpanPixels = 160;


    /* Lines bar. */

    /** Minimal color coefficient for the most noticeable chunk color in range */
    const qreal kLineBarMinNoticeableFraction = 0.5;


    /* Thumbnails bar. */

    const qreal kThumbnailHeightForDrawing = 10.0;


    /* Global */

    /** Top-level metrics. */
    const qreal kDateBarHeightPixels = 16;
    const qreal kLineBarHeightPixels = 20;
    const qreal kTickBarHeightPixels = 36;

    /** Minimal relative change of msecs-per-pixel value of a time slider for animation parameters to be recalculated.
     * This value was introduced so that the parameters are not recalculated constantly when changes are small. */
    const qreal kPerPixelChangeThresholdMs = 1.0e-4;

    /** Lower limit on time slider scale. */
    const qreal kMinTimePerPixelMs = 2.0;

    /** Maximal number of lines in a time slider. */
    const int kMaxLines = 2;

    const int kLastMinuteStripeWidth = 9;

    const int kLineLabelFontHeight = 14;
    const int kLineLabelFontWeight = QFont::DemiBold;

    /** Padding at the left of line labels. */
    const int kLineLabelPaddingPixels = 6;

    /** Minimal width of big datetime tooltips. */
    const qreal kDateTimeTooltipMinimalWidth = 128.0;

    /** Gap between position marker and tooltip tail. */
    const qreal kToolTipMargin = 4.0;

    /** Mouse wheel sensitivities. */
    const qreal kWheelDegreesFor2xScale = 90.0;
    const qreal kWheelDegreesFor1PixelScroll = 3.0;

    /** Window zooming maximum speed for kinetic processor. */
    const qreal kWindowZoomMaxSpeed = kWheelDegreesFor2xScale * 8;

    /** Window zooming friction for kinetic processor. */
    const qreal kWindowZoomFriction = kWheelDegreesFor2xScale / 2.0;

    /** Window dragging maximum speed for kinetic processor. */
    const qreal kWindowScrollMaxSpeed = 10000.0;

    /** Window dragging friction for kinetic processor. */
    const qreal kWindowScrollFriction = 1000.0;

    const qreal kZoomSideSnapDistance = 0.075;

    const qreal kHoverEffectDistance = 5.0;

    const int kStartDragDistance = 5;

    const int kBookmarkFontHeight = 11;
    const int kBookmarkFontWeight = QFont::DemiBold;
    const int kBookmarkTextPadding = 6;
    const int kMinBookmarkTextCharsVisible = 6;

    /** Invalid hover position. To denote it we use a position outside of client area: */
    const QPointF kInvalidHoverPos(-1.0, -1.0);

    /** Width of sensitive areas at the left and right of the window.
      * When a marker is dragged to these areas it causes window scroll.
      * Has effect only with DragScrollsWindow option. */
    const qreal kWindowScrollPixelThreshold = 1.0;

    const int kNoThumbnailsFontPixelSize = 16;

    // Timeline border area width in pixels where single click will auto-scroll the timeline
    static constexpr int kAutoShiftAreaWidth = 64;

    // Auto-scroll will move the timeline such way, so cursor will be 180 pixels away from border.
    static constexpr int kAutoShiftOffsetWidth = 180;

    QTime msecsToTime(qint64 msecs)
    {
        return QTime(0, 0, 0, 0).addMSecs(msecs);
    }

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
            qnWarning("Invalid line number '%1'.", line);
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
            qnWarning("Invalid time period type '%1'.", static_cast<int>(type));
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

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnTimeSliderChunkPainter
// -------------------------------------------------------------------------- //
// TODO: #Elric
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
        m_centralPosition(0),
        m_centralCoordinate(0),
        m_minChunkLength(0),
        m_position(0),
        m_pendingLength(0),
        m_pendingPosition(0)
    {
        NX_ASSERT(m_painter && m_slider);

        const QnTimeSliderColors& colors = slider->colors();

        m_pastColor[Qn::RecordingContent]           = colors.pastRecording;
        m_pastColor[Qn::MotionContent]              = colors.pastMotion;
        m_pastColor[Qn::TimePeriodContentCount]     = colors.pastBackground;

        m_futureColor[Qn::RecordingContent]         = colors.futureRecording;
        m_futureColor[Qn::MotionContent]            = colors.futureMotion;
        m_futureColor[Qn::TimePeriodContentCount]   = colors.futureBackground;

        m_position = m_centralPosition = m_minChunkLength = 0;
    }

    void start(qint64 startPos, qint64 centralPos, qint64 minChunkLength, const QRectF& rect)
    {
        m_position = startPos;
        m_centralPosition = centralPos;
        m_centralCoordinate = m_slider->quickPositionFromValue(m_centralPosition);
        m_minChunkLength = minChunkLength;
        m_rect = rect;

        std::fill(m_weights.begin(), m_weights.end(), 0);
    }

    void paintChunk(qint64 length, Qn::TimePeriodContent content)
    {
        NX_ASSERT(length >= 0);

        if (m_pendingLength > 0 && m_pendingLength + length > m_minChunkLength)
        {
            qint64 delta = m_minChunkLength - m_pendingLength;
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
        if (m_pendingLength > 0)
            flushChunk();
    }

private:
    void storeChunk(qint64 length, Qn::TimePeriodContent content)
    {
        if (m_pendingLength == 0)
            m_pendingPosition = m_position;

        m_weights[content] += length;
        m_pendingLength += length;
        m_position += length;
    }

    void flushChunk()
    {
        qint64 leftPosition = m_pendingPosition;
        qint64 rightPosition = m_pendingPosition + m_pendingLength;

        qreal l = m_slider->quickPositionFromValue(leftPosition);
        qreal r = m_slider->quickPositionFromValue(rightPosition);

        if (rightPosition <= m_centralPosition)
        {
            m_painter->fillRect(QRectF(l, m_rect.top(), r - l, m_rect.height()), currentColor(m_pastColor));
        }
        else if (leftPosition >= m_centralPosition)
        {
            m_painter->fillRect(QRectF(l, m_rect.top(), r - l, m_rect.height()), currentColor(m_futureColor));
        }
        else
        {
            m_painter->fillRect(QRectF(l, m_rect.top(), m_centralCoordinate - l, m_rect.height()), currentColor(m_pastColor));
            m_painter->fillRect(QRectF(m_centralCoordinate, m_rect.top(), r - m_centralCoordinate, m_rect.height()), currentColor(m_futureColor));
        }

        m_pendingPosition = rightPosition;
        m_pendingLength = 0;

        std::fill(m_weights.begin(), m_weights.end(), 0);
    }

    QColor currentColor(const std::array<QColor, Qn::TimePeriodContentCount + 1>& colors) const
    {
        qreal rc = m_weights[Qn::RecordingContent];
        qreal mc = m_weights[Qn::MotionContent];
        qreal nc = m_weights[Qn::TimePeriodContentCount];
        qreal sum = m_pendingLength;

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

        return linearCombine(rc / sum, colors[Qn::RecordingContent], 1.0, linearCombine(mc / sum, colors[Qn::MotionContent], nc / sum, colors[Qn::TimePeriodContentCount]));
    }

private:
    QnTimeSlider* m_slider;
    QPainter* m_painter;

    qint64 m_centralPosition;
    qreal m_centralCoordinate;
    qint64 m_minChunkLength;
    QRectF m_rect;

    qint64 m_position;
    qint64 m_pendingLength;
    qint64 m_pendingPosition;

    std::array<qint64, Qn::TimePeriodContentCount + 1> m_weights;
    std::array<QColor, Qn::TimePeriodContentCount + 1> m_pastColor;
    std::array<QColor, Qn::TimePeriodContentCount + 1> m_futureColor;
};


// -------------------------------------------------------------------------- //
// QnTimeSliderStepStorage
// -------------------------------------------------------------------------- //
// TODO: #Elric move all step construction hell here.
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
            ? kWindowZoomFriction * 4.0
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
        m_slider->shiftWindow(qint64(distance.toReal() * m_slider->m_msecsPerPixel));

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
QnTimeSlider::QnTimeSlider(QGraphicsItem* parent, QGraphicsItem* tooltipParent):
    base_type(parent),
    m_windowStart(0),
    m_windowEnd(0),
    m_minimalWindow(0),
    m_selectionStart(0),
    m_selectionEnd(0),
    m_selectionValid(false),
    m_oldMinimum(0),
    m_oldMaximum(0),
    m_options(0),
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
    m_thumbnailsAspectRatio(-1.0),
    m_lastThumbnailsUpdateTime(0),
    m_lastHoverThumbnail(-1),
    m_thumbnailsVisible(false),
    m_tooltipVisible(true),
    m_rulerHeight(kDateBarHeightPixels + kTickBarHeightPixels + kLineBarHeightPixels),
    m_lastMinuteAnimationDelta(0),
    m_pixmapCache(new QnTimeSliderPixmapCache(kNumTickmarkLevels, this)),
    m_localOffset(0),
    m_hoverMousePos(kInvalidHoverPos),
    m_lastLineBarValue(),
    m_bookmarksViewer(createBookmarksViewer()),
    m_bookmarksVisible(false),
    m_bookmarksHelper(nullptr),
    m_liveSupported(false),
    m_selectionInitiated(false),
    m_tooltipLine1(new GraphicsLabel(this)),
    m_tooltipLine2(new GraphicsLabel(this)),
    m_updatingValue(false),
    m_kineticZoomHandler(new KineticZoomHandler(this)),
    m_kineticScrollHandler(new KineticScrollHandler(this))
{
    setAutoHideToolTip(false);

    /* Prepare thumbnail update timer. */
    m_thumbnailsUpdateTimer = new QTimer(this);
    connect(m_thumbnailsUpdateTimer, SIGNAL(timeout()), this, SLOT(updateThumbnailsStepSizeTimer()));

    /* Set default vector sizes. */
    m_lineData.resize(kMaxLines);
    m_lastMinuteIndicatorVisible.fill(true, kMaxLines);

    generateProgressPatterns();

    /* Create tooltip layout. */
    QGraphicsLinearLayout* tooltipLayout = new QGraphicsLinearLayout(Qt::Vertical);
    tooltipLayout->setContentsMargins(5.0, 4.0, 5.0, 3.0);
    tooltipLayout->setSpacing(2.0);
    tooltipLayout->addItem(m_tooltipLine1);
    tooltipLayout->addItem(m_tooltipLine2);

    toolTipItem()->setLayout(tooltipLayout);
    toolTipItem()->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    setTooltipMargin(kToolTipMargin);

    m_tooltipLine1->setAlignment(Qt::AlignCenter);
    m_tooltipLine2->setAlignment(Qt::AlignCenter);

    QFont font;
    font.setPixelSize(font.pixelSize() + 2);
    font.setBold(true);
    m_tooltipLine2->setFont(font);

    /* Fine-tuned kinetic processor contants. */
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
    registerAnimation(kineticZoomProcessor);
    m_kineticZoomHandler->updateKineticProcessor();

    /* Prepare kinetic drag processor. */
    const auto kineticScrollProcessor = new KineticCuttingProcessor(QMetaType::QReal, this);
    kineticScrollProcessor->setHandler(m_kineticScrollHandler.data());
    kineticScrollProcessor->setMaxShiftInterval(kScrollProcessorMaxShiftIntervalSeconds);
    kineticScrollProcessor->setMaxSpeedMagnitude(kWindowScrollMaxSpeed);
    kineticScrollProcessor->setSpeedCuttingThreshold(kScrollProcessorSpeedCuttingThreshold);
    registerAnimation(kineticScrollProcessor);
    m_kineticScrollHandler->updateKineticProcessor();

    /* Prepare zoom processor. */
    DragProcessor* dragProcessor = new DragProcessor(this);
    dragProcessor->setHandler(this);
    dragProcessor->setFlags(DragProcessor::DontCompress);
    dragProcessor->setStartDragDistance(kStartDragDistance);
    dragProcessor->setStartDragTime(-1); /* No drag on timeout. */

    /* Prepare animation timer listener. */
    startListening();
    registerAnimation(this);

    /* Set default property values. */
    setAcceptHoverEvents(true);
    setProperty(style::Properties::kSliderLength, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred, QSizePolicy::Slider);

    setWindowStart(minimum());
    setWindowEnd(maximum());
    QnTimeSlider::Options defaultOptions = StickToMinimum | StickToMaximum | PreserveWindowSize
        | SelectionEditable | ClearSelectionOnClick | SnapZoomToSides | UnzoomOnDoubleClick
        | UpdateToolTip;
#ifdef TIMELINE_BEHAVIOR_2_5
    defaultOptions |= AdjustWindowToPosition;
#else
    defaultOptions |= StillPosition | HideLivePosition | LeftButtonSelection
        | DragScrollsWindow | StillBookmarksViewer;
#endif
    setOptions(defaultOptions);

    /* Run handlers. */
    updateSteps();
    updateMinimalWindow();
    updatePixmapCache();
    sliderChange(SliderRangeChange);

    m_bookmarksViewer->setParent(this);
    m_bookmarksViewer->setParentItem(tooltipParent);
    m_bookmarksViewer->setZValue(std::numeric_limits<qreal>::max());
    toolTipItem()->setParentItem(tooltipParent);
    toolTipItem()->stackBefore(m_bookmarksViewer);
}

bool QnTimeSlider::isWindowBeingDragged() const
{
    return dragProcessor()->isRunning() && m_dragMarker == NoMarker;
}

QnBookmarksViewer* QnTimeSlider::createBookmarksViewer()
{
    const auto bookmarksAtLocationFunc =
        [this](qint64 location)
        {
            if (!m_bookmarksHelper)
                return QnCameraBookmarkList();

            if (m_options.testFlag(StillBookmarksViewer))
                location = valueFromPosition(QPointF(location, 0));

            static const auto searchOptions = QnBookmarkMergeHelper::OnlyTopmost
                | QnBookmarkMergeHelper::ExpandArea;

            return m_bookmarksHelper->bookmarksAtPosition(location, m_msecsPerPixel, searchOptions);
        };

    const auto getPosFunc =
        [this](qint64 location)
        {
            if (m_options.testFlag(StillBookmarksViewer))
            {
                if (location >= rect().width())
                    return QnBookmarksViewer::PosAndBoundsPair();   //< Out of window
            }
            else
            {
                if (!windowContains(location))
                    return QnBookmarksViewer::PosAndBoundsPair();   //< Out of window
            }

            const auto viewer = bookmarksViewer();

            qreal pos = m_options.testFlag(StillBookmarksViewer) ?
                static_cast<qreal>(location) :
                positionFromValue(location).x();

            const auto target = QPointF(pos, lineBarRect().top());

            Q_D(const GraphicsSlider);

            const auto left = viewer->mapFromItem(this, QPointF(d->pixelPosMin, 0));
            const auto right = viewer->mapFromItem(this, QPointF(d->pixelPosMax, 0));
            const QnBookmarksViewer::Bounds bounds(left.x(), right.x());
            const QPointF finalPos = viewer->mapToParent(viewer->mapFromItem(this, target));
            return QnBookmarksViewer::PosAndBoundsPair(finalPos, bounds);
        };

    return new QnBookmarksViewer(bookmarksAtLocationFunc, getPosFunc, this);
}

QnTimeSlider::~QnTimeSlider()
{
}

void QnTimeSlider::createSteps(QVector<QnTimeStep>* absoluteSteps, QVector<QnTimeStep>* relativeSteps)
{
    static const QString hmFormat = tr("hh:mm",
        "Format for displaying hours and minutes on timeline.");
    static const QString hmApFormat = tr("h:mm ap",
        "Format for displaying hours and minutes on timeline, with am/pm indicator.");
    static const QString hApFormat = tr("h ap",
        "Format for displaying hours on timeline, with am/pm indicator.");
    static const QString dFormat = tr("dd",
        "Format for displaying days on timeline.");
    static const QString moFormat = tr("MMMM",
        "Format for displaying months on timeline.");
    static const QString yFormat = tr("yyyy",
        "Format for displaying years on timeline");
    static const QString dateMinsFormat = tr("dd MMMM yyyy hh:mm",
        "Format for displaying minute caption in timeline's header, without am/pm indicator.");
    static const QString dateMinsApFormat = tr("dd MMMM yyyy h:mm ap",
        "Format for displaying minute caption in timeline's header, with am/pm indicator.");
    static const QString dateHoursFormat = tr("dd MMMM yyyy hh:mm",
        "Format for displaying hour caption in timeline's header, without am/pm indicator.");
    static const QString dateHoursApFormat = tr("dd MMMM yyyy h ap",
        "Format for displaying hour caption in timeline's header, with am/pm indicator.");
    static const QString dateDaysFormat = tr("dd MMMM yyyy",
        "Format for displaying day caption in timeline's header.");
    static const QString dateMonthsFormat = tr("MMMM yyyy",
        "Format for displaying month caption in timeline's header.");
    static const QString dateYearsFormat = tr("yyyy",
        "Format for displaying year caption in timeline's header");

    QString msSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Milliseconds);
    QString sSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds);
    QString mSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Minutes);
    QString hSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Hours);
    QString dSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Days);
    QString moSuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Months);
    QString ySuffix = QnTimeStrings::suffix(QnTimeStrings::Suffix::Years);

    bool ampm = QLocale().timeFormat().contains(lit("ap"), Qt::CaseInsensitive);

    const QString& hFormat = ampm ? hApFormat : hmFormat;
    const QString& mFormat = ampm ? hmApFormat : hmFormat;

    *absoluteSteps <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                10,     1000,   msSuffix,       QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                50,     1000,   msSuffix,       QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                100,    1000,   msSuffix,       QString(),          false) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                500,    1000,   msSuffix,       QString(),          false) <<
        QnTimeStep(QnTimeStep::Seconds,         1000ll,                             1,      60,     sSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Seconds,         1000ll,                             5,      60,     sSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Seconds,         1000ll,                             10,     60,     sSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Seconds,         1000ll,                             30,     60,     sSuffix,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Minutes,         1000ll * 60,                        1,      24*60,  mFormat,        ampm ? dateMinsApFormat : dateMinsFormat, false) <<
        QnTimeStep(QnTimeStep::Minutes,         1000ll * 60,                        5,      24*60,  mFormat,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Minutes,         1000ll * 60,                        10,     24*60,  mFormat,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Minutes,         1000ll * 60,                        30,     24*60,  mFormat,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Hours,           1000ll * 60 * 60,                   1,      24,     hFormat,        ampm ? dateHoursApFormat : dateHoursFormat, false) <<
        QnTimeStep(QnTimeStep::Hours,           1000ll * 60 * 60,                   3,      24,     hFormat,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Hours,           1000ll * 60 * 60,                   12,     24,     hFormat,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Days,            1000ll * 60 * 60 * 24,              1,      31,     dFormat,        dateDaysFormat,     false) <<
        QnTimeStep(QnTimeStep::Months,          1000ll * 60 * 60 * 24 * 31,         1,      12,     moFormat,       dateMonthsFormat,   false) <<
        QnTimeStep(QnTimeStep::Years,           1000ll * 60 * 60 * 24 * 365,        1,      50000,  yFormat,        dateYearsFormat,    false) <<
        QnTimeStep(QnTimeStep::Years,           1000ll * 60 * 60 * 24 * 365,        5,      50000,  yFormat,        QString(),          false) <<
        QnTimeStep(QnTimeStep::Years,           1000ll * 60 * 60 * 24 * 365,        10,     50000,  yFormat,        dateYearsFormat,    false);
    enumerateSteps(*absoluteSteps);

    *relativeSteps <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                10,     1000,   msSuffix,       QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                50,     1000,   msSuffix,       QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                100,    1000,   msSuffix,       QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1ll,                                500,    1000,   msSuffix,       QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             1,      60,     sSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             5,      60,     sSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             10,     60,     sSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll,                             30,     60,     sSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        1,      60,     mSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        5,      60,     mSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        10,     60,     mSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60,                        30,     60,     mSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   1,      24,     hSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   3,      24,     hSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60,                   12,     24,     hSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24,              1,      31,     dSuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30,         1,      12,     moSuffix,       QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30 * 12,    1,      50000,  ySuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30 * 12,    5,      50000,  ySuffix,        QString(),          true) <<
        QnTimeStep(QnTimeStep::Milliseconds,    1000ll * 60 * 60 * 24 * 30 * 12,    10,     50000,  ySuffix,        QString(),          true);
    enumerateSteps(*relativeSteps);
}

void QnTimeSlider::enumerateSteps(QVector<QnTimeStep>& steps)
{
    for (int i = 0; i < steps.size(); i++)
        steps[i].index = i;
}

void QnTimeSlider::setupShowAnimator(VariantAnimator* animator) const
{
    using namespace nx::client::desktop::ui::workbench;
    qnWorkbenchAnimations->setupAnimator(animator,
        Animations::Id::TimelineTooltipShow);
}

void QnTimeSlider::setupHideAnimator(VariantAnimator* animator) const
{
    using namespace nx::client::desktop::ui::workbench;
    qnWorkbenchAnimations->setupAnimator(animator,
        Animations::Id::TimelineTooltipHide);
}

void QnTimeSlider::invalidateWindow()
{
    m_oldMaximum = m_oldMinimum = -1;
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
        qnWarning("Invalid line count '%1'.", lineCount);
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

qint64 QnTimeSlider::windowStart() const
{
    return m_windowStart;
}

void QnTimeSlider::setWindowStart(qint64 windowStart)
{
    setWindow(windowStart, qMax(windowStart + m_minimalWindow, m_windowEnd));
}

qint64 QnTimeSlider::windowEnd() const
{
    return m_windowEnd;
}

void QnTimeSlider::setWindowEnd(qint64 windowEnd)
{
    setWindow(qMin(m_windowStart, windowEnd - m_minimalWindow), windowEnd);
}

void QnTimeSlider::setWindow(qint64 start, qint64 end, bool animate)
{
    qint64 targetWindowSize = end - start;

    start = qBound(minimum(), start, maximum());
    end = qMax(start, qBound(minimum(), end, maximum()));

    /* Check if window size was spoiled and fix it if possible. */
    if (m_options.testFlag(PreserveWindowSize))
    {
        qint64 newWindowSize = end - start;
        qint64 range = maximum() - minimum();
        if (newWindowSize < targetWindowSize && newWindowSize < range)
        {
            qint64 windowSize = qMin(targetWindowSize, range);
            if (start == minimum())
                end = start + windowSize;
            else
                start = end - windowSize;
        }
    }

    if (end - start < m_minimalWindow)
    {
        qint64 delta = (m_minimalWindow - (end - start) + 1) / 2;
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

            updateToolTipVisibilityInternal(true);
            updateMSecsPerPixel();
            updateThumbnailsPeriod();

            m_bookmarksViewer->updateOnWindowChange();
            updateBookmarksViewerLocation();
        }
    }
}

void QnTimeSlider::shiftWindow(qint64 delta, bool animate)
{
    qint64 windowSize = m_windowEnd - m_windowStart;
    if (delta < 0)
    {
        qint64 newWindowStart = qMax(m_windowStart + delta, minimum());
        setWindow(newWindowStart, newWindowStart + windowSize, animate);
    }
    else
    {
        qint64 newWindowEnd = qMin(m_windowEnd + delta, maximum());
        setWindow(newWindowEnd - windowSize, newWindowEnd, animate);
    }

    m_zoomAnchor = qBound(m_windowStart, m_zoomAnchor + delta, m_windowEnd);
}

bool QnTimeSlider::windowContains(qint64 position)
{
    return m_windowStart <= position && position <= m_windowEnd;
}

void QnTimeSlider::ensureWindowContains(qint64 position)
{
    if (position < m_windowStart)
        shiftWindow(position - m_windowStart);
    else if (m_windowEnd < position)
        shiftWindow(position - m_windowEnd);
}

void QnTimeSlider::setSliderPosition(qint64 position, bool keepInWindow)
{
    if (!keepInWindow)
        return setSliderPosition(position);

    bool inWindow = windowContains(sliderPosition());
    setSliderPosition(position);
    if (inWindow)
        ensureWindowContains(sliderPosition());
}

void QnTimeSlider::setValue(qint64 value, bool keepInWindow)
{
    {
        /* To not change tooltip visibility in setValue or setWindow: */
        QScopedValueRollback<bool> updateRollback(m_updatingValue, true);

        qint64 oldValue = this->value();
        setValue(value);

        if (keepInWindow && !isWindowBeingDragged() && windowContains(oldValue))
        {
            if (m_options.testFlag(StillPosition))
                shiftWindow(this->value() - oldValue);
            else
                ensureWindowContains(this->value());
        }
    }

    /* Update tooltip visibility after both setValue and setWindow: */
    updateToolTipVisibilityInternal(true);
}

qint64 QnTimeSlider::selectionStart() const
{
    return m_selectionStart;
}

void QnTimeSlider::setSelectionStart(qint64 selectionStart)
{
    setSelection(selectionStart, qMax(selectionStart, m_selectionEnd));
}

qint64 QnTimeSlider::selectionEnd() const
{
    return m_selectionEnd;
}

void QnTimeSlider::setSelectionEnd(qint64 selectionEnd)
{
    setSelection(qMin(m_selectionStart, selectionEnd), selectionEnd);
}

void QnTimeSlider::setSelection(qint64 start, qint64 end)
{
    start = qBound(minimum(), start, maximum());
    end = qMax(start, qBound(minimum(), end, maximum()));

    if (m_selectionStart != start || m_selectionEnd != end)
    {
        m_selectionInitiated = false;
        m_selectionStart = start;
        m_selectionEnd = end;

        emit selectionChanged(m_selectionStart, m_selectionEnd);
    }
}

bool QnTimeSlider::isSelectionValid() const
{
    return m_selectionValid;
}

void QnTimeSlider::setSelectionValid(bool valid)
{
    m_selectionValid = valid;

    if (!m_selectionValid)
        m_selectionInitiated = false;
}

QnThumbnailsLoader* QnTimeSlider::thumbnailsLoader() const
{
    return m_thumbnailsLoader.data();
}

void QnTimeSlider::setThumbnailsLoader(QnThumbnailsLoader* loader, qreal aspectRatio)
{
    if (m_thumbnailsLoader.data() == loader && qFuzzyEquals(m_thumbnailsAspectRatio, aspectRatio))
        return;

    clearThumbnails();
    m_oldThumbnailData.clear();
    m_thumbnailsUpdateTimer->stop();

    if (m_thumbnailsLoader)
        disconnect(m_thumbnailsLoader.data(), nullptr, this, nullptr);

    m_thumbnailsLoader = loader;
    m_thumbnailsAspectRatio = aspectRatio;

    if (m_thumbnailsLoader)
    {
        //connect(m_thumbnailsLoader.data(), SIGNAL(sourceSizeChanged()), this, SLOT(updateThumbnailsStepSizeForced())); // TODO: #Elric
        connect(m_thumbnailsLoader.data(), SIGNAL(thumbnailsInvalidated()), this, SLOT(clearThumbnails()));
        connect(m_thumbnailsLoader.data(), SIGNAL(thumbnailLoaded(const QnThumbnail& )), this, SLOT(addThumbnail(const QnThumbnail& )));
    }

    updateThumbnailsPeriod();
    updateThumbnailsStepSize(true); // TODO: #Elric

    if (m_thumbnailsLoader)
        foreach(const QnThumbnail& thumbnail, m_thumbnailsLoader->thumbnails())
            addThumbnail(thumbnail);
}

QPointF QnTimeSlider::positionFromValue(qint64 logicalValue, bool bound) const
{
    Q_D(const GraphicsSlider);

    d->ensureMapper();

    return QPointF(
        d->pixelPosMin + GraphicsStyle::sliderPositionFromValue(m_windowStart, m_windowEnd, logicalValue, d->pixelPosMax - d->pixelPosMin, d->upsideDown, bound),
        0.0
    );
}

qint64 QnTimeSlider::valueFromPosition(const QPointF& position, bool bound) const
{
    Q_D(const GraphicsSlider);

    d->ensureMapper();

    return GraphicsStyle::sliderValueFromPosition(m_windowStart, m_windowEnd, position.x(), d->pixelPosMax - d->pixelPosMin, d->upsideDown, bound);
}

qreal QnTimeSlider::quickPositionFromValue(qint64 logicalValue, bool bound) const
{
    return bound ? m_boundMapper(logicalValue) : m_unboundMapper(logicalValue);
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

void QnTimeSlider::setTargetStart(qint64 start)
{
    m_targetStart = start == minimum() ? std::numeric_limits<qint64>::min() : start;
}

void QnTimeSlider::setTargetEnd(qint64 end)
{
    m_targetEnd = end == maximum() ? std::numeric_limits<qint64>::max() : end;
}

qint64 QnTimeSlider::targetStart()
{
    return m_targetStart == std::numeric_limits<qint64>::min() ? minimum() : m_targetStart;
}

qint64 QnTimeSlider::targetEnd()
{
    return m_targetEnd == std::numeric_limits<qint64>::max() ? maximum() : m_targetEnd;
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

    m_progressPastPattern = QPixmap(d4, d4);
    m_progressPastPattern.fill(Qt::transparent);
    QPainter pastPainter(&m_progressPastPattern);
    pastPainter.setRenderHint(QPainter::Antialiasing);
    pastPainter.fillPath(path, m_colors.pastLastMinuteStripe);

    m_progressFuturePattern = QPixmap(d4, d4);
    m_progressFuturePattern.fill(Qt::transparent);
    QPainter futurePainter(&m_progressFuturePattern);
    futurePainter.setRenderHint(QPainter::Antialiasing);
    futurePainter.fillPath(path, m_colors.futureLastMinuteStripe);
}

bool QnTimeSlider::isAnimatingWindow() const
{
    return m_animatingSliderWindow
        || m_kineticZoomHandler->kineticProcessor()->isRunning()
        || m_kineticScrollHandler->kineticProcessor()->isRunning();
}

bool QnTimeSlider::scaleWindow(qreal factor, qint64 anchor)
{
    qreal targetMSecsPerPixel = m_msecsPerPixel * factor;
    if (targetMSecsPerPixel < kMinTimePerPixelMs) {
        factor = kMinTimePerPixelMs / m_msecsPerPixel;
        if (qFuzzyCompare(factor, 1.0))
            return false; /* We've reached the min scale. */
    }

    qint64 start = anchor + (m_windowStart - anchor) * factor;
    qint64 end = anchor + (m_windowEnd - anchor) * factor;

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

QnTimeSlider::Marker QnTimeSlider::markerFromPosition(const QPointF& pos, qreal maxDistance) const
{
    if (m_selectionValid)
    {
        QPointF selectionStart = positionFromValue(m_selectionStart);
        if (qAbs(selectionStart.x() - pos.x()) < maxDistance)
            return SelectionStartMarker;

        QPointF selectionEnd = positionFromValue(m_selectionEnd);
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
        return m_selectionValid ? positionFromValue(m_selectionStart) : rect().topLeft();

    case SelectionEndMarker:
        return m_selectionValid ? positionFromValue(m_selectionEnd) : rect().topLeft();

    default:
        return rect().topLeft();
    }
}

void QnTimeSlider::setMarkerSliderPosition(Marker marker, qint64 position) {
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

void QnTimeSlider::extendSelection(qint64 position)
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
        qint64 start = sliderPosition();
        qint64 end = position;
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
    if (m_colors.tickmarkLines.empty())
        return QColor(255, 255, 255, 255);

    return m_colors.tickmarkLines[qBound(0, level, static_cast<int>(m_colors.tickmarkLines.size() - 1))];
}

QColor QnTimeSlider::tickmarkTextColor(int level) const
{
    if (m_colors.tickmarkText.empty())
        return QColor(255, 255, 255, 255);

    return m_colors.tickmarkText[qBound(0, level, static_cast<int>(m_colors.tickmarkText.size() - 1))];
}

QRectF QnTimeSlider::thumbnailsRect() const
{
    QRectF r(rect());
    return QRectF(r.left(), r.top(), r.width(), thumbnailsHeight());
}

QRectF QnTimeSlider::rulerRect() const
{
    QRectF r(rect());
    return QRectF(r.left(), thumbnailsHeight(), r.width(), rulerHeight());
}

QRectF QnTimeSlider::dateBarRect() const
{
    QRectF r(rect());
    return QRectF(r.left(), thumbnailsHeight(), r.width(), kDateBarHeightPixels);
}

QRectF QnTimeSlider::tickmarkBarRect() const
{
    QRectF r(rect());
    return QRectF(r.left(), thumbnailsHeight() + kDateBarHeightPixels, r.width(), kTickBarHeightPixels);
}

QRectF QnTimeSlider::lineBarRect() const
{
    QRectF r(rect());
    return QRectF(r.left(), thumbnailsHeight() + kDateBarHeightPixels + kTickBarHeightPixels, r.width(), kLineBarHeightPixels);
}

bool QnTimeSlider::isThumbnailsVisible() const
{
    return m_thumbnailsVisible;
}

qreal QnTimeSlider::thumbnailsHeight() const
{
    return size().height() - m_rulerHeight;
}

qreal QnTimeSlider::rulerHeight() const
{
    return m_rulerHeight;
}

void QnTimeSlider::addThumbnail(const QnThumbnail& thumbnail)
{
    if (m_thumbnailsUpdateTimer->isActive())
        return;

    if (m_thumbnailData.contains(thumbnail.time()))
        return; /* There is no real point in overwriting existing thumbnails. Besides, it would result in flicker. */

    m_thumbnailData[thumbnail.time()] = ThumbnailData(thumbnail);
}

void QnTimeSlider::clearThumbnails()
{
    m_thumbnailData.clear();
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

        qreal pos = quickPositionFromValue(data.thumbnail.time(), false);
        qreal width = data.thumbnail.size().width() * height / data.thumbnail.size().height();
        data.pos = (pos - center) / width;
    }
}

void QnTimeSlider::animateLastMinute(int deltaMs)
{
    m_lastMinuteAnimationDelta += deltaMs;
}

const QVector<qint64>& QnTimeSlider::indicators() const
{
    return m_indicators;
}

void QnTimeSlider::setIndicators(const QVector<qint64>& indicators)
{
    m_indicators = indicators;
}

qint64 QnTimeSlider::localOffset() const
{
    return m_localOffset;
}

void QnTimeSlider::setLocalOffset(qint64 localOffset)
{
    if (m_localOffset == localOffset)
        return;

    m_localOffset = localOffset;

    updateToolTipText();
}

const QnTimeSliderColors& QnTimeSlider::colors() const
{
    return m_colors;
}

void QnTimeSlider::setColors(const QnTimeSliderColors& colors)
{
    m_colors = colors;
    generateProgressPatterns();
    updatePixmapCache();
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
    updateToolTipVisibilityInternal(true);
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
    if (thumbnailsRect().contains(pos))
        return Qn::MainWindow_Thumbnails_Help;

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

    if (!m_bookmarksHelper->bookmarksAtPosition(valueFromPosition(pos), m_msecsPerPixel).isEmpty())
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
    m_pixmapCache->setDateColor(m_colors.dateBarText);

    m_noThumbnailsPixmap = m_pixmapCache->textPixmap(tr("No thumbnails available"),
        kNoThumbnailsFontPixelSize, m_colors.noThumbnailsLabel);

    for (int i = 0; i < kNumTickmarkLevels; ++i)
    {
        localFont.setWeight(kTickmarkFontWeights[i]);
        m_pixmapCache->setTickmarkColor(i, tickmarkTextColor(i));
        m_pixmapCache->setTickmarkFont(i, localFont);
    }

    updateLineCommentPixmaps();
    updateTickmarkTextSteps();
}

void QnTimeSlider::updateToolTipVisibilityInternal(bool animated)
{
    if (m_updatingValue)
        return;

    bool canBeVisible = windowContains(sliderPosition())
        && positionMarkerVisible()
        && isVisible();

    if (!canBeVisible)
        animated = false;

    const bool visible = canBeVisible && m_tooltipVisible;
    if (visible == actualToolTipVisibility())
        return;

    if (visible)
        showToolTip(false); //< Always show tooltip immediately.
    else
        hideToolTip(animated);
}

void QnTimeSlider::updateToolTipText()
{
    if (!m_options.testFlag(UpdateToolTip))
        return;

    const auto pos = sliderPosition();

    QString line1;
    QString line2;

    if (isLive())
    {
        line1 = tr("Live");
    }
    else
    {
        static const QString tooltipFormatDate = lit("dd MMMM yyyy");
        static const QString tooltipFormatTimeLong = lit("hh:mm:ss");
        static const QString tooltipFormatTimeLongAP = lit("h:mm:ss ap");
        static const QString tooltipFormatTimeShort = lit("mm:ss");

        const bool ampm = m_locale.timeFormat().contains(lit("ap"), Qt::CaseInsensitive);
        const auto tooltipFormatTime = ampm ? tooltipFormatTimeLongAP : tooltipFormatTimeLong;

        if (m_options.testFlag(UseUTC))
        {
            QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(pos + m_localOffset);
            line1 = m_locale.toString(dateTime, tooltipFormatDate);
            line2 = m_locale.toString(dateTime, tooltipFormatTime);
        }
        else
        {
            const auto& format = maximum() >= 60ll * 60ll * 1000ll /* Longer than 1 hour? */
                ? tooltipFormatTimeLong
                : tooltipFormatTimeShort;

            line1 = m_locale.toString(msecsToTime(pos), format);
        }
    }

    m_tooltipLine1->setText(line1);

    if (line2.isEmpty())
    {
        m_tooltipLine2->setVisible(false);
        toolTipItem()->setMinimumWidth(0.0);
    }
    else
    {
        m_tooltipLine2->setText(line2);
        m_tooltipLine2->setVisible(true);

        /* Big datetime tooltips shouldn't be narrower than some minimal value: */
        toolTipItem()->setMinimumWidth(kDateTimeTooltipMinimalWidth);
    }
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
    // TODO: #Elric this one is VERY slow right now.

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

bool QnTimeSlider::isLiveSupported() const
{
    return m_liveSupported;
}

void QnTimeSlider::setLiveSupported(bool value)
{
    if (m_liveSupported == value)
        return;

    m_liveSupported = value;
    updateToolTipVisibilityInternal(false);
}

bool QnTimeSlider::isLive() const
{
    return m_liveSupported && !m_selecting && value() == maximum();
}

qreal QnTimeSlider::msecsPerPixel() const
{
    return m_msecsPerPixel;
}

void QnTimeSlider::updateMSecsPerPixel()
{
    qreal msecsPerPixel = (m_windowEnd - m_windowStart) / qMax(size().width(), 1);
    if (qFuzzyIsNull(msecsPerPixel))
        msecsPerPixel = 1.0; /* Technically, we should never get here, but we want to feel safe. */

    if (qFuzzyCompare(m_msecsPerPixel, msecsPerPixel))
        return;

    m_msecsPerPixel = msecsPerPixel;

    updateThumbnailsStepSize(false);
    updateStepAnimationTargets();
    updateAggregationValue();

    emit msecsPerPixelChanged();
}

void QnTimeSlider::updateMinimalWindow()
{
    qint64 minimalWindow = size().width() * kMinTimePerPixelMs;
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
        qreal separationPixels = m_steps[minStepIndex].stepMSecs / m_msecsPerPixel;

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
    bool updateNeeded = (qAbs(m_msecsPerPixel - m_animationUpdateMSecsPerPixel) / qMin(m_msecsPerPixel, m_animationUpdateMSecsPerPixel)) > kPerPixelChangeThresholdMs;
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
        qreal separationPixels = m_steps[i].stepMSecs / m_msecsPerPixel;

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
    qreal dt = deltaMs / 1000.0;

    bool oldShown = false;
    for (QList<ThumbnailData>::iterator pos = m_oldThumbnailData.begin(); pos != m_oldThumbnailData.end(); pos++)
        oldShown |= animateThumbnail(dt, *pos);

    if (!oldShown)
    {
        m_oldThumbnailData.clear();

        for (QMap<qint64, ThumbnailData>::iterator pos = m_thumbnailData.begin(); pos != m_thumbnailData.end(); pos++)
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
    if (!thumbnailsLoader())
        return;

    if (m_thumbnailsUpdateTimer->isActive() || !m_oldThumbnailData.isEmpty())
        return;

    if (qFuzzyIsNull(thumbnailsHeight()))
        return; // TODO: #Elric may be a wrong place for the check

    thumbnailsLoader()->setTimePeriod(m_windowStart, m_windowEnd);
}

void QnTimeSlider::updateThumbnailsStepSizeLater()
{
    m_thumbnailsUpdateTimer->start(250); /* Re-start it if it's active. */
}

void QnTimeSlider::updateThumbnailsStepSizeForced()
{
    m_thumbnailsUpdateTimer->stop();
    updateThumbnailsStepSize(true, true);
}

void QnTimeSlider::updateThumbnailsStepSizeTimer()
{
    m_thumbnailsUpdateTimer->stop();
    updateThumbnailsStepSize(true);
}

void QnTimeSlider::updateThumbnailsStepSize(bool instant, bool forced)
{
    if (!thumbnailsLoader())
        return; /* Nothing to update. */

    if (m_thumbnailsUpdateTimer->isActive())
    {
        if (instant || forced)
            m_thumbnailsUpdateTimer->stop();
        else
            return;
    }

    /* Calculate new bounding size. */
    int boundingHeigth = qRound(thumbnailsHeight());
    if (boundingHeigth < kThumbnailHeightForDrawing)
        boundingHeigth = 0;

    QSize boundingSize = QSize(boundingHeigth * 256, boundingHeigth);
    bool boundingSizeChanged = thumbnailsLoader()->boundingSize() != boundingSize;

    /* Calculate actual thumbnail size. */
#if 0
    QSize size = thumbnailsLoader()->thumbnailSize();
    if (size.isEmpty())
        size = QSize(boundingHeigth * 4 / 3, boundingHeigth); /* For uninitialized loader, assume 4:3 aspect ratio. */
    else if (size.height() != boundingHeigth) { // TODO: evil hack, should work on signals.
        size = QSize(boundingHeigth * size.width() / size.height() , boundingHeigth);
#else
    QSize size;
    if (m_thumbnailsAspectRatio > 0)
        size = QSize(boundingHeigth * m_thumbnailsAspectRatio, boundingHeigth);
#endif
    if (size.isEmpty())
        return;

    /* Calculate new time step. */
    qint64 timeStep = m_msecsPerPixel * size.width();
    bool timeStepChanged = qAbs(timeStep / m_msecsPerPixel - thumbnailsLoader()->timeStep() / m_msecsPerPixel) >= 1;

    /* Nothing changed? Leave. */ //TODO #vkutin Check if removing "!m_thumbnailData.isEmpty()" is fine:
    if (!timeStepChanged && !boundingSizeChanged && !forced) // && !m_thumbnailData.isEmpty())
        return;

    /* Ok, thumbnails have to be re-generated. So we first freeze our old thumbnails. */
    if (m_oldThumbnailData.isEmpty())
        freezeThumbnails();

    /* If animation is running, we want to wait until it's finished.
     * We also don't want to update thumbnails too often. */
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if ((!instant || isAnimatingWindow() || currentTime - m_lastThumbnailsUpdateTime < 1000) && !forced)
    {
        updateThumbnailsStepSizeLater();
    }
    else
    {
        m_lastThumbnailsUpdateTime = currentTime;
        thumbnailsLoader()->setBoundingSize(boundingSize + QSize(1, 1)); /* Evil hack to force update even if size didn't change. */
        thumbnailsLoader()->setBoundingSize(boundingSize);
        thumbnailsLoader()->setTimeStep(timeStep);
        updateThumbnailsPeriod();
    }
}

void QnTimeSlider::setThumbnailSelecting(qint64 time, bool selecting)
{
    if (time < 0)
        return;

    QMap<qint64, ThumbnailData>::iterator pos = m_thumbnailData.find(time);
    if (pos == m_thumbnailData.end())
        return;

    qint64 actualTime = pos->thumbnail.actualTime();

    QMap<qint64, ThumbnailData>::iterator ipos;
    for (ipos = pos; ipos->thumbnail.actualTime() == actualTime; ipos--)
    {
        ipos->selecting = selecting;
        if (ipos == m_thumbnailData.begin())
            break;
    }

    for (ipos = pos + 1; ipos != m_thumbnailData.end() && ipos->thumbnail.actualTime() == actualTime; ipos++)
        ipos->selecting = selecting;
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

    m_unboundMapper = QnLinearFunction(m_windowStart, positionFromValue(m_windowStart).x(), m_windowEnd, positionFromValue(m_windowEnd).x());
    m_boundMapper = QnBoundedLinearFunction(m_unboundMapper, rect().left(), rect().right());

    QRectF lineBarRect = this->lineBarRect();
    qreal lineTop, lineUnit = qFuzzyIsNull(m_totalLineStretch) ? 0.0 : lineBarRect.height() / m_totalLineStretch;

    /* Draw background. */
    if (qFuzzyIsNull(m_totalLineStretch))
    {
        drawSolidBackground(painter, rect());
        painter->fillRect(lineBarRect, m_colors.pastRecording);
    }
    else
    {
        drawSolidBackground(painter, thumbnailsRect());
        drawSolidBackground(painter, tickmarkBarRect());

        /* Draw lines background (that is, time periods). */
        lineTop = lineBarRect.top();
        for (int line = 0; line < m_lineCount; line++)
        {
            qreal lineHeight = lineUnit * effectiveLineStretch(line);
            if (qFuzzyIsNull(lineHeight))
                continue;

            QRectF lineRect(lineBarRect.left(), lineTop, lineBarRect.width(), lineHeight);

            drawPeriodsBar(
                painter,
                m_lineData[line].timeStorage.aggregated(Qn::RecordingContent),
                m_lineData[line].timeStorage.aggregated(Qn::MotionContent),
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

    /* Draw thumbnails. */
    drawThumbnails(painter, thumbnailsRect());

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
    if (positionMarkerVisible())
        drawMarker(painter, sliderPosition(), m_colors.positionMarker, 2.0);

    /* Draw indicators. */
    foreach (qint64 position, m_indicators)
        drawMarker(painter, position, m_colors.indicator);
}

bool QnTimeSlider::eventFilter(QObject* target, QEvent* event)
{
    /* Handle tooltip drag here by our dragProcessor instead of base class: */
    if (target == toolTipItem())
    {
        switch (event->type())
        {
        case QEvent::GraphicsSceneMousePress:
            m_dragDelta = toolTipItem()->tailPos() - static_cast<QGraphicsSceneMouseEvent*>(event)->pos();
            m_dragMarker = DragMarker;
            dragProcessor()->mousePressEvent(toolTipItem(), static_cast<QGraphicsSceneMouseEvent*>(event), true);
            return true;

        case QEvent::GraphicsSceneMouseRelease:
            dragProcessor()->mouseReleaseEvent(toolTipItem(), static_cast<QGraphicsSceneMouseEvent*>(event));
            return true;

        case QEvent::GraphicsSceneMouseMove:
            dragProcessor()->mouseMoveEvent(toolTipItem(), static_cast<QGraphicsSceneMouseEvent*>(event));
            return true;

        default:
            break;
        }
    }

    return base_type::eventFilter(target, event);
}

void QnTimeSlider::drawSeparator(QPainter* painter, const QRectF& rect)
{
    if (qFuzzyEquals(rect.top(), this->rect().top()))
        return; /* Don't draw separator at the top of the widget. */

    QnScopedPainterPenRollback penRollback(painter, QPen(m_colors.separator, 0));
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
    painter->drawLine(rect.topLeft(), rect.topRight());
}

void QnTimeSlider::drawLastMinute(QPainter* painter, const QRectF& rect)
{
    const qreal moveSpeed = 0.05;

    qint64 startTime = maximum() - 60 * 1000;
    qreal startPos = quickPositionFromValue(startTime);
    if (startPos >= rect.right())
        return;

    qreal shift = qMod(m_lastMinuteAnimationDelta * moveSpeed, static_cast<qreal>(m_progressPastPattern.width()));
    m_lastMinuteAnimationDelta = shift / moveSpeed;
    QTransform brushTransform = QTransform::fromTranslate(-shift, 0.0);

    qreal sliderPos = quickPositionFromValue(sliderPosition());

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

    if (sliderPos > startPos && !qFuzzyEquals(startPos, sliderPos))
    {
        QRectF pastRect(QPointF(startPos, rect.top()), QPointF(sliderPos, rect.bottom()));
        QBrush brush(m_progressPastPattern);
        brush.setTransform(brushTransform);
        painter->fillRect(pastRect, m_colors.pastLastMinuteBackground);
        painter->fillRect(pastRect, brush);
    }

    if (!qFuzzyEquals(rect.right(), sliderPos))
    {
        QRectF futureRect(QPointF(qMax(startPos, sliderPos), rect.top()), rect.bottomRight());
        QBrush brush(m_progressFuturePattern);
        brush.setTransform(brushTransform);
        painter->fillRect(futureRect, m_colors.futureLastMinuteBackground);
        painter->fillRect(futureRect, brush);
    }
}

void QnTimeSlider::drawSelection(QPainter* painter)
{
    if (!m_selectionValid)
        return;

    if (m_selectionStart == m_selectionEnd)
    {
        drawMarker(painter, m_selectionStart, m_colors.selectionMarker);
        return;
    }

    qint64 selectionStart = qMax(m_selectionStart, m_windowStart);
    qint64 selectionEnd = qMin(m_selectionEnd, m_windowEnd);
    if (selectionStart < selectionEnd)
    {
        QRectF rect = this->rect();
        QRectF selectionRect(
            QPointF(quickPositionFromValue(selectionStart), rect.top()),
            QPointF(quickPositionFromValue(selectionEnd), rect.bottom())
        );

        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, false);
        painter->fillRect(selectionRect, m_colors.selection);
    }

    drawMarker(painter, m_selectionStart, m_colors.selectionMarker);
    drawMarker(painter, m_selectionEnd, m_colors.selectionMarker);
}

void QnTimeSlider::drawMarker(QPainter* painter, qint64 pos, const QColor& color, qreal width)
{
    if (!windowContains(pos))
        return;

    QPen pen(color, width);
    pen.setCapStyle(Qt::FlatCap);

    QnScopedPainterPenRollback penRollback(painter, pen);
    QnScopedPainterAntialiasingRollback aaRollback(painter, false);

    qreal x = quickPositionFromValue(pos);
    painter->drawLine(QPointF(x, rect().top()), QPointF(x, rect().bottom()));
}

void QnTimeSlider::drawPeriodsBar(QPainter* painter, const QnTimePeriodList& recorded, const QnTimePeriodList& motion, const QRectF& rect)
{
    qint64 minimumValue = this->windowStart();
    qint64 maximumValue = this->windowEnd();

    /* The code here may look complicated, but it takes care of not rendering
     * different motion periods several times over the same location.
     * It makes transparent time slider look better. */

    /* Note that constness of period lists is important here as requesting
     * iterators from a non-const object will result in detach. */
    const QnTimePeriodList periods[Qn::TimePeriodContentCount] = {recorded, motion};

    QnTimePeriodList::const_iterator pos[Qn::TimePeriodContentCount];
    QnTimePeriodList::const_iterator end[Qn::TimePeriodContentCount];
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
    {
         pos[i] = periods[i].findNearestPeriod(minimumValue, true);
         end[i] = periods[i].findNearestPeriod(maximumValue, true);
         if (end[i] != periods[i].end() && end[i]->contains(maximumValue))
             end[i]++;
    }

    qint64 value = minimumValue;
    bool inside[Qn::TimePeriodContentCount];
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        inside[i] = pos[i] == end[i] ? false : pos[i]->contains(value);

    QnTimeSliderChunkPainter chunkPainter(this, painter);
    chunkPainter.start(value, this->sliderPosition(), m_msecsPerPixel, rect);

    while(value != maximumValue)
    {
        qint64 nextValue[Qn::TimePeriodContentCount] = {maximumValue, maximumValue};
        for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        {
            if (pos[i] == end[i])
                continue;

            if (!inside[i])
            {
                nextValue[i] = qMin(maximumValue, pos[i]->startTimeMs);
                continue;
            }

            if (!pos[i]->isInfinite())
                nextValue[i] = qMin(maximumValue, pos[i]->startTimeMs + pos[i]->durationMs);
        }

        qint64 bestValue = qMin(nextValue[Qn::RecordingContent], nextValue[Qn::MotionContent]);

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
    qreal leftPos = quickPositionFromValue(windowStart());
    qreal rightPos = quickPositionFromValue(windowEnd());
    qreal centralPos = quickPositionFromValue(sliderPosition());

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
    qint64 startPos = qMax(minimum() + 1, valueFromPosition(positionFromValue(m_windowStart) - overlap, false)) + m_localOffset;
    qint64 endPos = qMin(maximum() - 1, valueFromPosition(positionFromValue(m_windowEnd) + overlap, false)) + m_localOffset;

    /* Initialize next positions for tickmark steps. */
    for (int i = minStepIndex; i < stepCount; i++)
        m_nextTickmarkPos[i] = roundUp(startPos, m_steps[i]);

    /* Draw tickmarks. */
    for (int i = 0; i < m_tickmarkLines.size(); i++)
        m_tickmarkLines[i].clear();

    while(true)
    {
        qint64 pos = m_nextTickmarkPos[minStepIndex];
        if (pos > endPos)
            break;

        /* Find index of the step to use. */
        int index = minStepIndex;
        for (; index < stepCount; index++)
        {
            if (m_nextTickmarkPos[index] == pos)
                m_nextTickmarkPos[index] = add(pos, m_steps[index]);
            else
                break;
        }

        index--;
        int level = tickmarkLevel(index);

        qreal x = quickPositionFromValue(pos - m_localOffset, false);

        /* Draw label if needed. */
        qreal lineHeight = m_stepData[index].currentHeight;
        if (!qFuzzyIsNull(m_stepData[index].currentTextOpacity) && kTickmarkFontHeights[level])
        {
            QPixmap pixmap = m_pixmapCache->tickmarkTextPixmap(level, pos, kTickmarkFontHeights[level], m_steps[index]);
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
    const auto backgroundColor = [this](qint64 number) -> QColor
    {
        return m_colors.dateBarBackgrounds.empty() ? QColor(Qt::gray) :
            m_colors.dateBarBackgrounds[number % m_colors.dateBarBackgrounds.size()];
    };

    int stepCount = m_steps.size();

    /* Find index of the highlight time step. */
    int highlightIndex = 0;
    qreal highlightSpanPixels = qMax(size().width() * kMinDateSpanFraction, kMinDateSpanPixels);
    for (; highlightIndex < stepCount; highlightIndex++)
        if (!m_steps[highlightIndex].longFormat.isEmpty() && m_steps[highlightIndex].stepMSecs / m_msecsPerPixel >= highlightSpanPixels)
            break;

    highlightIndex = qMin(highlightIndex, stepCount - 1); // TODO: #Elric remove this line.
    const QnTimeStep& highlightStep = m_steps[highlightIndex];

    qreal topMargin = qFloor((rect.height() - kDateTextFontHeight) * 0.5);

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);

    /* Draw highlight. */
    qint64 pos1 = roundUp(m_windowStart + m_localOffset, highlightStep);
    qint64 pos0 = sub(pos1, highlightStep);
    qreal x0 = quickPositionFromValue(m_windowStart);
    qint64 number = absoluteNumber(pos1, highlightStep);
    while(true)
    {
        qreal x1 = quickPositionFromValue(pos1 - m_localOffset);

        painter->setPen(Qt::NoPen);
        painter->setBrush(backgroundColor(number));
        painter->drawRect(QRectF(x0, rect.top(), x1 - x0, rect.height()));

        QPixmap pixmap = m_pixmapCache->dateTextPixmap(pos0, kDateTextFontHeight, highlightStep);
        const auto pixmapSize = pixmap.size() / pixmap.devicePixelRatio();
        QRectF textRect((x0 + x1) / 2.0 - pixmapSize.width() / 2.0,
            rect.top() + topMargin, pixmapSize.width(), pixmapSize.height());
        if (textRect.left() < rect.left())
            textRect.moveRight(x1);
        if (textRect.right() > rect.right())
            textRect.moveLeft(x0);

        drawCroppedPixmap(painter, textRect, rect, pixmap, pixmap.rect());

        if (pos1 >= m_windowEnd + m_localOffset)
            break;

        pos0 = pos1;
        pos1 = add(pos1, highlightStep);
        number++;
        x0 = x1;
    }
}

void QnTimeSlider::drawThumbnails(QPainter* painter, const QRectF& rect)
{
    if (rect.height() < kThumbnailHeightForDrawing)
        return;

    m_thumbnailsPaintRect = rect;

    if (!thumbnailsLoader())
    {
        QSizeF labelSizeBound = rect.size();
        labelSizeBound.setHeight(m_noThumbnailsPixmap.height());

        QRect labelRect = Geometry::aligned(Geometry::expanded(
            Geometry::aspectRatio(m_noThumbnailsPixmap.size()), labelSizeBound,
            Qt::KeepAspectRatio), rect, Qt::AlignCenter).toRect();

        drawCroppedPixmap(painter, labelRect, rect, m_noThumbnailsPixmap, m_noThumbnailsPixmap.rect());
        return;
    }

    qint64 step = thumbnailsLoader()->timeStep();
    if (thumbnailsLoader()->timeStep() <= 0)
        return;

    if (thumbnailsLoader()->thumbnailSize().isEmpty())
        return;

    qreal aspectRatio = Geometry::aspectRatio(thumbnailsLoader()->thumbnailSize());
    qreal thumbnailWidth = rect.height() * aspectRatio;

    if (!m_oldThumbnailData.isEmpty() || m_thumbnailsUpdateTimer->isActive())
    {
        QRectF boundingRect = rect;
        for (int i = 0; i < m_oldThumbnailData.size(); i++)
        {
            const ThumbnailData& data = m_oldThumbnailData[i];
            if (data.thumbnail.isEmpty())
                continue;

            qreal x = rect.width() / 2 + data.pos * thumbnailWidth;;
            QSizeF targetSize(m_thumbnailsAspectRatio * rect.height(), rect.height());
            QRectF targetRect(x - targetSize.width() / 2, rect.top(), targetSize.width(), targetSize.height());

            drawThumbnail(painter, data, targetRect, boundingRect);

            boundingRect.setLeft(qMax(boundingRect.left(), targetRect.right()));
        }
    }
    else
    {
        QnScopedPainterPenRollback penRollback(painter);
        QnScopedPainterBrushRollback brushRollback(painter);

        qint64 startTime = qFloor(m_windowStart, step);
        qint64 endTime = qCeil(m_windowEnd, step);

        QRectF boundingRect = rect;
        for (qint64 time = startTime; time <= endTime; time += step)
        {
            QMap<qint64, ThumbnailData>::iterator pos = m_thumbnailData.find(time);
            if (pos == m_thumbnailData.end())
                continue;

            ThumbnailData& data = *pos;
            if (data.thumbnail.isEmpty())
                continue;

            qreal x = quickPositionFromValue(time, false);
            QSizeF targetSize(m_thumbnailsAspectRatio * rect.height(), rect.height());
            QRectF targetRect(x - targetSize.width() / 2, rect.top(), targetSize.width(), targetSize.height());

            drawThumbnail(painter, data, targetRect, boundingRect);

            boundingRect.setLeft(qMax(boundingRect.left(), targetRect.right()));
        }
    }
}

void QnTimeSlider::drawThumbnail(QPainter* painter, const ThumbnailData& data, const QRectF& targetRect, const QRectF& boundingRect)
{
    const QImage& image = data.thumbnail.image();

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * data.opacity);

    QRectF rect;
    drawCroppedImage(painter, targetRect, boundingRect, image, image.rect(),& rect);

    if (!rect.isEmpty())
    {
        qreal a = data.selection;
        qreal width = 1.0 + a * 2.0;
        QColor color = linearCombine(1.0 - a, QColor(255, 255, 255, 32), a, m_colors.selectionMarker); // TODO: #Elric customize
        rect = Geometry::eroded(rect, width / 2.0);

        painter->setPen(QPen(color, width));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect);

        qreal x = quickPositionFromValue(data.thumbnail.actualTime());
        if (x >= rect.left() && x <= rect.right())
        {
            painter->setPen(Qt::NoPen);
            painter->setBrush(withAlpha(color, (color.alpha() + 255) / 2));
            painter->drawEllipse(QPointF(x, rect.bottom()), width * 2, width * 2);
        }
    }
    painter->setOpacity(opacity);
}

// TODO: #GDM #Bookmarks check drawBookmarks() against m_localOffset
void QnTimeSlider::drawBookmarks(QPainter* painter, const QRectF& rect)
{
    if (!m_bookmarksVisible)
        return;

    QnTimelineBookmarkItemList bookmarks =
        m_bookmarksHelper ? m_bookmarksHelper->bookmarks(m_msecsPerPixel) : QnTimelineBookmarkItemList();
    if (bookmarks.isEmpty())
        return;

    QFontMetricsF fontMetrics(m_pixmapCache->defaultFont());

    qreal pos = quickPositionFromValue(sliderPosition());

    QBrush pastBrush(m_colors.pastBookmarkBound);
    QBrush futureBrush(m_colors.futureBookmarkBound);

    QFont font(m_pixmapCache->defaultFont());
    font.setWeight(kBookmarkFontWeight);

    qint64 hoverValueMs = valueFromPosition(m_hoverMousePos, false);

    int hoveredBookmarkItem = QnBookmarkMergeHelper::indexAtPosition(bookmarks,
        hoverValueMs,
        m_msecsPerPixel,
        QnBookmarkMergeHelper::OnlyTopmost | QnBookmarkMergeHelper::ExpandArea);

    /* Draw bookmarks: */
    for (int i = 0; i < bookmarks.size(); ++i)
    {
        const QnTimelineBookmarkItem& bookmarkItem = bookmarks[i];

        if (bookmarkItem.startTimeMs() >= m_windowEnd || bookmarkItem.endTimeMs() <= m_windowStart)
            continue;

        QRectF bookmarkRect = rect;
        bookmarkRect.setLeft(quickPositionFromValue(qMax(bookmarkItem.startTimeMs(), m_windowStart)));
        bookmarkRect.setRight(quickPositionFromValue(qMin(bookmarkItem.endTimeMs(), m_windowEnd)));

        bool hovered = i == hoveredBookmarkItem;
        const QColor& pastBg = hovered ? m_colors.pastBookmarkHover : m_colors.pastBookmark;
        const QColor& futureBg = hovered ? m_colors.futureBookmarkHover : m_colors.futureBookmark;

        QBrush leftBoundBrush;
        QBrush rightBoundBrush;
        if (pos > bookmarkRect.left() && pos < bookmarkRect.right())
        {
            painter->fillRect(bookmarkRect.x(), bookmarkRect.y(), pos - bookmarkRect.x(), bookmarkRect.height(), pastBg);
            painter->fillRect(pos, bookmarkRect.y(), bookmarkRect.right() - pos, bookmarkRect.height(), futureBg);
            leftBoundBrush = pastBrush;
            rightBoundBrush = futureBrush;
        }
        else if (pos >= bookmarkRect.right())
        {
            painter->fillRect(bookmarkRect, pastBg);
            leftBoundBrush = rightBoundBrush = pastBrush;
        }
        else
        {
            painter->fillRect(bookmarkRect, futureBg);
            leftBoundBrush = rightBoundBrush = futureBrush;
        }

        painter->fillRect(bookmarkRect.left(), bookmarkRect.top(), 1, bookmarkRect.height(), leftBoundBrush);
        painter->fillRect(bookmarkRect.right() - 1, bookmarkRect.top(), 1, bookmarkRect.height(), rightBoundBrush);

        if (bookmarkItem.isCluster())
            continue;

        const QnCameraBookmark& bookmark = bookmarkItem.bookmark();

        QRectF textRect = bookmarkRect;
        if (i < bookmarks.size() - 1)
            textRect.setRight(qMin(bookmarkRect.right(), quickPositionFromValue(bookmarks[i + 1].startTimeMs())));
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
        result.setHeight(m_rulerHeight);
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
        qint64 position = this->sliderPosition();
        if (position >= m_windowStart && position <= m_windowEnd)
        {
            qint64 windowRange = m_windowEnd - m_windowStart;

            qreal speed;
            if (position < m_windowStart + windowRange / 3)
                speed = -1.0 + static_cast<qreal>(position - m_windowStart) / (windowRange / 3);
            else if (position < m_windowEnd - windowRange / 3)
                speed = 0.0;
            else
                speed = 1.0 - static_cast<qreal>(m_windowEnd - position) / (windowRange / 3);

            speed = speed * qMax(0.5 * windowRange, 32 * 1000.0);

            qint64 delta = speed * deltaMs / 1000.0;
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
        qint64 untilStart = static_cast<qint64>((targetStart() - m_windowStart) * timeFactor);
        qint64 untilEnd = static_cast<qint64>((targetEnd() - m_windowEnd) * timeFactor);

        qint64 desiredWindowSize = targetEnd() - targetStart();
        qint64 maxRemainder = qMax(qAbs(untilStart), qAbs(untilEnd));

        if (maxRemainder < qMax(desiredWindowSize / 1000, 1000LL))
        {
            m_animatingSliderWindow = false;
            untilStart = untilEnd = 0;
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
            if (m_oldMinimum < 0 && m_oldMinimum == m_oldMaximum)
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

            qint64 windowStart = m_windowStart;
            qint64 windowEnd = m_windowEnd;

            bool wasAtMinimum = windowStart == m_oldMinimum;
            bool wasAtMaximum = windowEnd == m_oldMaximum;

            // TODO: #vkutin Enable this when thumbnail updates fully support it
            bool keepFullRange = false; // wasAtMinimum && wasAtMaximum;
            if (keepFullRange)
            {
                windowStart = minimum();
                windowEnd = maximum();
            }
            else
            {
                if (wasAtMaximum && m_options.testFlag(StickToMaximum))
                {
                    if (m_options.testFlag(PreserveWindowSize))
                        windowStart += maximum() - windowEnd;

                    windowEnd = maximum();
                }

                if (wasAtMinimum && m_options.testFlag(StickToMinimum))
                {
                    if (m_options.testFlag(PreserveWindowSize))
                        windowEnd += minimum() - windowStart;

                    windowStart = minimum();
                }
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
            updateToolTipVisibilityInternal(true);
            updateToolTipText();
            break;

        default:
            break;
    }
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
        m_zoomAnchor = valueFromPosition(event->pos());

        /* Snap zoom anchor to window sides. */
        if (m_options.testFlag(SnapZoomToSides))
        {
            qreal windowRange = m_windowEnd - m_windowStart;
            if ((m_zoomAnchor - m_windowStart) / windowRange < kZoomSideSnapDistance)
                m_zoomAnchor = m_windowStart;
            else if ((m_windowEnd - m_zoomAnchor) / windowRange < kZoomSideSnapDistance)
                m_zoomAnchor = m_windowEnd;
        }

        m_kineticZoomHandler->unhurry();
        m_kineticZoomHandler->kineticProcessor()->shift(degrees);
        m_kineticZoomHandler->kineticProcessor()->start();
    }

    event->accept();
}

void QnTimeSlider::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    base_type::resizeEvent(event);

    if (event->oldSize() != event->newSize())
    {
        updateMSecsPerPixel();
        updateMinimalWindow();
        updateThumbnailsStepSize(false);
        updateThumbnailsVisibility();
    }
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
                setSelection(sliderPosition(), sliderPosition());
                setSelectionValid(true);
            }
            else
            {
                if (event->key() == Qt::Key_BracketLeft)
                    setSelectionStart(sliderPosition());
                else
                    setSelectionEnd(sliderPosition());
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
    m_hoverMousePos = kInvalidHoverPos;

    unsetCursor();

    setThumbnailSelecting(m_lastHoverThumbnail, false);
    m_lastHoverThumbnail = -1;
}

void QnTimeSlider::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    base_type::hoverMoveEvent(event);

    m_hoverMousePos = event->pos();
    updateBookmarksViewerLocation();

    if (isWindowBeingDragged())
        return;

    if (thumbnailsRect().contains(event->pos()) && thumbnailsLoader() && thumbnailsLoader()->timeStep() != 0 && m_oldThumbnailData.isEmpty() && !m_thumbnailsUpdateTimer->isActive())
    {
        qint64 time = qRound(valueFromPosition(event->pos()), thumbnailsLoader()->timeStep());

        setThumbnailSelecting(m_lastHoverThumbnail, false);
        setThumbnailSelecting(time, true);
        m_lastHoverThumbnail = time;
    }
    else
    {
        setThumbnailSelecting(m_lastHoverThumbnail, false);
        m_lastHoverThumbnail = -1;
    }

    if (m_lastHoverThumbnail != -1)
    {
        setCursor(Qt::PointingHandCursor);
    }
    else
    {
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
}

void QnTimeSlider::updateBookmarksViewerLocation()
{
    if (lineBarRect().contains(m_hoverMousePos))
    {
        qint64 location = m_options.testFlag(StillBookmarksViewer) ?
            static_cast<qint64>(m_hoverMousePos.x()) :
            valueFromPosition(m_hoverMousePos);

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
        extendSelection(valueFromPosition(event->pos()));

    switch (event->button())
    {
        case Qt::LeftButton:
            m_dragMarker = markerFromPosition(event->pos(), kHoverEffectDistance);
            if (canSelect && m_dragMarker == NoMarker && !extendSelectionRequested)
                m_dragMarker = CreateSelectionMarker;
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

    bool thumbnailHasBeenClicked(false);
    if (thumbnailsLoader() && thumbnailsLoader()->timeStep() != 0 && thumbnailsRect().contains(event->pos()))
    {
        qint64 time = qRound(valueFromPosition(event->pos(), false), thumbnailsLoader()->timeStep());
        QMap<qint64, ThumbnailData>::const_iterator pos = m_thumbnailData.find(time);
        if (pos != m_thumbnailData.end())
        {
            if (event->button() == Qt::LeftButton)
            {
                emit thumbnailClicked();
                setSliderPosition(pos->thumbnail.actualTime(), true);
            }

            thumbnailHasBeenClicked = true;
        }
    }

    if (!thumbnailHasBeenClicked)
        dragProcessor()->mousePressEvent(this, event, immediateDrag);

    event->accept();
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
    {
        const auto pos = valueFromPosition(event->pos());

        const auto allowedOffset = m_msecsPerPixel * kAutoShiftAreaWidth;
        const auto targetOffset = m_msecsPerPixel * kAutoShiftOffsetWidth;
        const auto leftOffset = pos - m_windowStart;
        const auto rightOffset = m_windowEnd - pos;

        if (leftOffset <= allowedOffset)
        {
            // Shift window to the left (negative).
            shiftWindow(leftOffset - targetOffset, true);
        }
        else if (rightOffset <= allowedOffset)
        {
            // Shift window to the right (positive).
            shiftWindow(targetOffset - rightOffset, true);
        }
    }

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
        setSliderPosition(valueFromPosition(mapFromScene(info->mousePressScenePos())));
    }
}

void QnTimeSlider::startDrag(DragInfo* info)
{
    /* We have to use mapping because these events can be caused by the tooltip as well as by the slider itself. */
    QPointF mousePos = mapFromScene(info->mousePressScenePos());
    qint64 pos = valueFromPosition(mousePos + m_dragDelta);

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
            setSliderPosition(pos);
            break;

        case CreateSelectionMarker:
            setSelectionValid(true);
            setSelection(pos, pos);
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
    /* We have to use mapping because these events can be caused by the tooltip as well as by the slider itself.
     * We have to use scene coordinates because of possible redrag events when real tooltip coordinates differ from those stored in the DragInfo object. */
    QPointF mousePos = mapFromScene(info->mouseScenePos());

    if (m_dragMarker == NoMarker)
    {
        const auto delta = valueFromPosition(mapFromScene(info->mousePressScenePos())) - valueFromPosition(mousePos);
        const auto relativeDelta = m_dragWindowStart - m_windowStart + delta;
        const auto prevWindowStart = m_windowStart;
        shiftWindow(relativeDelta, false);

        m_kineticScrollHandler->unhurry();
        m_kineticScrollHandler->kineticProcessor()->shift(qreal(m_windowStart - prevWindowStart) / m_msecsPerPixel);
        return;
    }

    if (m_options.testFlag(DragScrollsWindow))
    {
        qreal left  = mousePos.x() - kWindowScrollPixelThreshold;
        qreal right = mousePos.x() + kWindowScrollPixelThreshold;

        if (m_dragDelta.x() < 0)
            left += m_dragDelta.x();
        else
            right += m_dragDelta.x();

        const auto redrag =
            [this]()
            {
                const auto redragCallback = [this]() { dragProcessor()->redrag(); };
                executeDelayedParented(redragCallback, kDefaultDelay, this);
            };

        if (left < 0)
        {
            ensureWindowContains(valueFromPosition(QPointF(left, 0), false));
            redrag();
        }
        else if (right > rect().right())
        {
            ensureWindowContains(valueFromPosition(QPointF(right, 0), false));
            redrag();
        }
    }

    setSliderPosition(valueFromPosition(mousePos + m_dragDelta));

    if (m_dragMarker == SelectionStartMarker || m_dragMarker == SelectionEndMarker)
    {
        qint64 selectionStart = m_selectionStart;
        qint64 selectionEnd = m_selectionEnd;
        Marker otherMarker = NoMarker;

        switch (m_dragMarker)
        {
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
        emit selectionReleased();
        m_selecting = false;
    }

    setSliderDown(false);
    m_dragDelta = QPointF();
}
