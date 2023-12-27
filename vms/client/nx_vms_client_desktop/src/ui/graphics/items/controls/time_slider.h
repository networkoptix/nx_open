// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <QtCore/QLocale>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

#include <qt_graphics_items/graphics_slider.h>

#include <core/resource/camera_bookmark_fwd.h>
#include <nx/vms/client/core/media/time_period_storage.h>
#include <nx/vms/client/desktop/workbench/timeline/thumbnail.h>
#include <nx/vms/client/desktop/workbench/timeline/time_marker.h>
#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/common/help_topic_queryable.h>
#include <ui/processors/drag_process_handler.h>
#include <ui/processors/kinetic_process_handler.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/math/functors.h>

#include "time_step.h"

class QTimer;

class GraphicsLabel;
class QGraphicsView;
class QnTimeSliderPixmapCache;
class QnTimeSliderChunkPainter;
class QnTimePeriodList;
class QnBookmarksViewer;
class QnBookmarkMergeHelper;
class QnWorkbenchContext;

namespace nx::vms::client::desktop::workbench::timeline {

class LivePreview;
class ThumbnailLoadingManager;
class ThumbnailPanel;

} // nx::vms::client::desktop::workbench::timeline

class QnTimeSlider:
    public Animated<GraphicsSlider>,
    public HelpTopicQueryable,
    public QnWorkbenchContextAware, //< Required for live preview.
    protected DragProcessHandler
{
    Q_OBJECT
    Q_PROPERTY(std::chrono::milliseconds windowStart READ windowStart WRITE setWindowStart)
    Q_PROPERTY(std::chrono::milliseconds windowEnd READ windowEnd WRITE setWindowEnd)

    using base_type = Animated<GraphicsSlider>;
    using LivePreview = nx::vms::client::desktop::workbench::timeline::LivePreview;
    using TimeMarker = nx::vms::client::desktop::workbench::timeline::TimeMarker;
    using ThumbnailPtr = nx::vms::client::desktop::workbench::timeline::ThumbnailPtr;
    using ThumbnailLoadingManager =
        nx::vms::client::desktop::workbench::timeline::ThumbnailLoadingManager;
    using ThumbnailPanel = nx::vms::client::desktop::workbench::timeline::ThumbnailPanel;

public:
    using milliseconds = std::chrono::milliseconds;

    enum Option {
        /**
         * Whether window start should stick to slider's minimum value.
         * If this flag is set and window starts at slider's minimum,
         * window start will change when minimum is changed.
         */
        StickToMinimum = 0x1,

        /**
         * Whether window end should stick to slider's maximum value.
         */
        StickToMaximum = 0x2,

        /**
         * Whether window size is to be preserved when auto-adjusting window sides.
         */
        PreserveWindowSize = 0x4,

        /**
         * Whether slider's tooltip is to be autoupdated using the provided
         * tool tip format.
         */
        UpdateToolTip = 0x8,

        /**
         * Whether slider's value is considered to be a number of milliseconds that
         * have passed since 1970-01-01 00:00:00.000, Coordinated Universal Time.
         *
         * If this flag is not set, slider's value is simply a number of
         * milliseconds, with no connection to real dates.
         */
        UseUTC = 0x10,

        /**
         * Whether the user can edit current selection with '[' and ']' buttons,
         * and with mouse.
         */
        SelectionEditable = 0x20,

        /**
         * Whether the window should be auto-adjusted to contain the current
         * position.
         */
        AdjustWindowToPosition = 0x40,

        /**
         * Whether zooming with the mouse wheel close to the window's side
         * should zoom into the side, not the mouse pointer position.
         */
        SnapZoomToSides = 0x80,

        /**
         * Whether double clicking the time slider should start animated unzoom.
         */
        UnzoomOnDoubleClick = 0x100,

        /**
        * Whether slider should adjust the window to keep current position marker at the same pixel position.
        */
        StillPosition = 0x200,

        /**
        * Whether slider should hide position marker and its tooltip in live mode.
        */
        HideLivePosition = 0x400,

        /**
        * Whether drag-selection should be performed by left mouse button (otherwise right).
        */
        LeftButtonSelection = 0x800,

        /**
        * Whether drag operations at window sides should scroll the window.
        */
        DragScrollsWindow = 0x1000,

        /**
        * Whether single click with mouse button immediately clears existing selection.
        */
        ClearSelectionOnClick = 0x4000
    };
    Q_DECLARE_FLAGS(Options, Option);

    explicit QnTimeSlider(
        QnWorkbenchContext* context,
        QGraphicsItem* parent = nullptr,
        QGraphicsItem* tooltipParent = nullptr);

    virtual ~QnTimeSlider();

    void invalidateWindow();

    int lineCount() const;
    void setLineCount(int lineCount);

    void setLineVisible(int line, bool visible);
    bool isLineVisible(int line) const;

    void setLineStretch(int line, qreal stretch);
    qreal lineStretch(int line) const;

    void setLineComment(int line, const QString& comment);
    QString lineComment(int line);

    QnTimePeriodList timePeriods(int line, Qn::TimePeriodContent type) const;
    void setTimePeriods(int line, Qn::TimePeriodContent type, const QnTimePeriodList& timePeriods);

    Options options() const;
    void setOptions(Options options);
    void setOption(Option option, bool value);

    void setTimeRange(milliseconds min, milliseconds max);
    milliseconds minimum() const;
    milliseconds maximum() const;

    milliseconds windowStart() const;
    void setWindowStart(milliseconds windowStart);

    milliseconds windowEnd() const;
    void setWindowEnd(milliseconds windowEnd);

    void setWindow(milliseconds start, milliseconds end, bool animate = false, bool forceResize = false);
    void shiftWindow(milliseconds delta, bool animate = false);

    bool isAnimatingWindow() const;
    bool isAnimatingWindowToCertainPosition() const;
    milliseconds windowTargetStart();
    milliseconds windowTargetEnd();

    bool windowContains(milliseconds position);
    void ensureWindowContains(milliseconds position);

    milliseconds sliderTimePosition() const;
    void setSliderPosition(milliseconds position, bool keepInWindow);

    milliseconds value() const;

    // Methods setValue(value, true) and navigateTo(value) do similar things but with different
    // visual behavior: setValue scrolls the timeline to move its cursor to the position it was at
    // before value change, navigateTo scrolls the timeline to move its cursor into view with side
    // paddings subtracted, i.e. acts as if user clicked timeline at specified position.
    void setValue(milliseconds value, bool keepInWindow);
    void navigateTo(milliseconds value);

    QnTimePeriod selection() const;

    milliseconds selectionStart() const;
    void setSelectionStart(milliseconds selectionStart);

    milliseconds selectionEnd() const;
    void setSelectionEnd(milliseconds selectionEnd);

    void setSelection(milliseconds start, milliseconds end);

    bool isSelectionValid() const;
    void setSelectionValid(bool valid);

    bool isLiveSupported() const;
    void setLiveSupported(bool value);

    bool isLive() const;
    void updateLive();

    Q_SLOT void finishAnimations();
    Q_SLOT void hurryKineticAnimations();

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    virtual bool eventFilter(QObject* target, QEvent* event) override;

    QPointF positionFromTime(milliseconds logicalValue, bool bound = true) const;
    milliseconds timeFromPosition(const QPointF& position, bool bound = true) const;

    bool isThumbnailsVisible() const;
    qreal thumbnailsHeight() const;

    /**
     * @return Full potential height of Time Slider regardless of its current state.
     */
    static qreal expectedHeight();

    ThumbnailLoadingManager* thumbnailLoadingManager() const;
    void setThumbnailLoadingManager(std::unique_ptr<ThumbnailLoadingManager> manager);
    void setThumbnailPanel(ThumbnailPanel* thumbnailPanel);

    const QVector<milliseconds>& indicators() const;
    void setIndicators(const QVector<milliseconds>& indicators);

    milliseconds localOffset() const;
    void setLocalOffset(milliseconds utcOffset);

    TimeMarker* tooltip() const;
    bool isTooltipVisible() const;
    void setTooltipVisible(bool visible);

    /**
     * Place the widget under Time Marker tooltip in their parent widget's stack.
     * @param widget Must be child of tooltipParentWidget provided to constructor.
     */
    void stackWidgetUnderTooltip(QWidget* widget) const;

    void setLastMinuteIndicatorVisible(int line, bool visible);
    bool isLastMinuteIndicatorVisible(int line) const;

    virtual int helpTopicAt(const QPointF& pos) const override;

    QnBookmarksViewer* bookmarksViewer();

    typedef QSharedPointer<QnBookmarkMergeHelper> QnBookmarkMergeHelperPtr;
    void setBookmarksHelper(const QnBookmarkMergeHelperPtr& helper);
    bool isBookmarksVisible() const;
    void setBookmarksVisible(bool bookmarksVisible);

    Qn::TimePeriodContent selectedExtraContent() const; //< Qn::RecordingContent if none.
    void setSelectedExtraContent(Qn::TimePeriodContent value);

    std::chrono::milliseconds msecsPerPixel() const;

    bool positionMarkerVisible() const;

    // Deduces live preview tooltip visibility and position quering mouse coordinates. Lazy.
    void updateLivePreview();

    /**
     * @param shaded If true, Thumbnail Panel will be painted partially transparent which is
     *     required by Live Preview.
     */
    void setThumbnailsFaded(bool faded);

    /**
     * Draw Thumbnail Panel thumbnails. Calling for QPainter created for another class is OK.
     * @param painter QPainter to paint by.
     * @param rect Limiting are to paint within using rect coordinate system.
     * @param timePeriod Archive time period specifying thumbnails subset to be painted.
     * @param unboundedPositionFromTimeFunc Function to convert time to coordinate within
     *     rect coordinate system. No bounding is applied to resulting coordinate.
     * @param boundedPositionFromTimeFunc Function to convert time to coordinate within
     *     rect coordinate system. Also specifies bounding for resulting coordinate.
     */
    void drawThumbnails(
        QPainter* painter,
        const QRectF& rect,
        const QnTimePeriod& timePeriod,
        const QnLinearFunction& unboundedPositionFromTimeFunc,
        const QnBoundedLinearFunction& boundedPositionFromTimeFunc);

    /**
     * Methods to handle events initially received by Thumbnail Panel.
     */
    void handleThumbnailsWheel(QGraphicsSceneWheelEvent* event);
    void handleThumbnailsResize();
    void handleThumbnailsHoverMove(milliseconds hoveredTime);
    void handleThumbnailsMousePress(milliseconds pressedTime, Qt::MouseButton button);

    void setLivePreviewAllowed(bool allowed);

signals:
    void timeChanged(milliseconds value);
    void timeRangeChanged(milliseconds min, milliseconds max);
    void windowMoved();
    void windowChanged(milliseconds windowStart, milliseconds windowEnd);
    void selectionChanged(const QnTimePeriod& selection);
    void customContextMenuRequested(const QPointF& pos, const QPoint& screenPos);
    void selectionPressed();
    void selectionReleased();
    void thumbnailsVisibilityChanged();
    void thumbnailsHeightChanged();
    void thumbnailClicked();
    void msecsPerPixelChanged();
    void lineCommentChanged(int line, const QString& comment);

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    virtual void wheelEvent(QGraphicsSceneWheelEvent* event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

    virtual void startDragProcess(DragInfo* info) override;
    virtual void startDrag(DragInfo* info) override;
    virtual void dragMove(DragInfo* info) override;
    virtual void finishDragProcess(DragInfo* info) override;

    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint) const override;

    static void createSteps(QVector<QnTimeStep>* absoluteSteps, QVector<QnTimeStep>* relativeSteps);
    static void enumerateSteps(QVector<QnTimeStep>& steps);

private:
    enum Marker
    {
        NoMarker,
        SelectionStartMarker,
        SelectionEndMarker,
        CreateSelectionMarker,
        DragMarker
    };

    struct TimeStepData
    {
        TimeStepData(): textWidthToHeight(0.0),
            currentHeight(0.0), targetHeight(0.0), heightSpeed(0.0),
            currentLineOpacity(0.0), targetLineOpacity(0.0), lineOpacitySpeed(0.0),
            currentTextOpacity(0.0), targetTextOpacity(0.0), textOpacitySpeed(0.0) {}

        qreal textWidthToHeight;

        qreal currentHeight;
        qreal targetHeight;
        qreal heightSpeed;

        qreal currentLineOpacity;
        qreal targetLineOpacity;
        qreal lineOpacitySpeed;

        qreal currentTextOpacity;
        qreal targetTextOpacity;
        qreal textOpacitySpeed;
    };

    struct LineData
    {
        std::shared_ptr<nx::vms::client::core::TimePeriodStorage> periodStorage{
            new nx::vms::client::core::TimePeriodStorage()};
        QString comment;
        QPixmap commentPixmap;
        bool visible = true;
        qreal stretch = 1.0;
    };

    struct ThumbnailData
    {
        ThumbnailData(): pos(0.0), opacity(0.0), selection(0.0), hiding(false), selecting(false) {}
        ThumbnailData(const ThumbnailPtr& thumbnail):
            thumbnail(thumbnail), pos(0.0), opacity(0.0), selection(0.0), hiding(false),
            selecting(false)
        {
        }

        ThumbnailPtr thumbnail;
        qreal pos;
        qreal opacity;
        qreal selection;
        bool hiding;
        bool selecting;
    };

    class KineticHandlerBase;
    class KineticZoomHandler;
    class KineticScrollHandler;

private:
    void tick(int deltaMSecs);

    // These two functions are crucial for AbstractLinearGraphicsSlider to work correctly.
    virtual QPointF positionFromValue(qint64 logicalValue, bool bound = true) const override;
    virtual qint64 valueFromPosition(const QPointF& position, bool bound = true) const override;

    Marker markerFromPosition(const QPointF& pos, qreal maxDistance = 1.0) const;
    QPointF positionFromMarker(Marker marker) const;

    QPointF tooltipPointedTo() const;

    qreal quickPositionFromTime(milliseconds logicalValue, bool bound = true) const;

    QPointF mapFromScreen(const QPoint& screenPos) const;
    bool selectionEnabledForPos(const QPointF& pos) const;

    QRectF dateBarRect() const;
    QRectF tickmarkBarRect() const;
    QRectF lineBarRect() const;

    qreal effectiveLineStretch(int line) const;

    /* Returns tickmark level clamped in range [0...maxTickmarkLevels] (warning! not [0...maxTickmarkLevels-1]) */
    int tickmarkLevel(int stepIndex) const;

    QColor tickmarkLineColor(int level) const;
    QColor tickmarkTextColor(int level) const;

    void setMarkerSliderPosition(Marker marker, milliseconds position);

    void extendSelection(milliseconds position);

    bool scaleWindow(qreal factor, milliseconds anchor);

    void drawPeriodsBar(QPainter* painter, const QnTimePeriodList& recorded, const QnTimePeriodList& extra, const QRectF& rect);
    void drawTickmarks(QPainter* painter, const QRectF& rect);
    void drawSolidBackground(QPainter* painter, const QRectF& rect);
    void drawMarker(QPainter* painter,
        milliseconds pos,
        qreal topY,
        const QColor& color,
        qreal width = 1.0);
    void drawSelection(QPainter* painter);
    void drawSeparator(QPainter* painter, const QRectF& rect);
    void drawLastMinute(QPainter* painter, const QRectF& rect);
    void drawDates(QPainter* painter, const QRectF& rect);
    void drawThumbnail(
        QPainter* painter,
        const ThumbnailData& data,
        const QRectF& targetRect,
        const QRectF& boundingRect,
        const QnBoundedLinearFunction& boundedPositionFromTimeFunc);
    void drawBookmarks(QPainter* painter, const QRectF& rect);

    int findTopLevelStepIndex() const;

    void updatePixmapCache();
    void updateToolTipVisibilityInternal();
    void updateToolTipPosition();

    /** Queue tooltip position update after paint event to reduce lags. */
    void queueTooltipPositionUpdate();
    void updateToolTipText();
    TimeMarker::TimeContent tooltipTimeContent(milliseconds pos) const;
    void updateSteps();
    void updateTickmarkTextSteps();
    bool updateMSecsPerPixel();
    void updateMinimalWindow();
    void updateStepAnimationTargets();
    void updateLineCommentPixmap(int line);
    void updateLineCommentPixmaps();
    void updateAggregationValue();
    void updateTotalLineStretch();
    void updateThumbnailsStepSize(bool instant);
    void updateThumbnailsPeriod();
    void updateThumbnailsVisibility();
    void updateKineticProcessor();

    Q_SLOT void addThumbnail(ThumbnailPtr thumbnail);
    Q_SLOT void clearThumbnails();
    void addAllThumbnailsFromManager();

    void animateStepValues(int deltaMs);
    void animateThumbnails(int deltaMs);
    bool animateThumbnail(qreal dt, ThumbnailData& data);
    void freezeThumbnails();
    void animateLastMinute(int deltaMSecs);

    void setThumbnailSelecting(milliseconds time, bool selecting);

    void setWindowTargetStart(milliseconds start);
    void setWindowTargetEnd(milliseconds end);

    // Similar to ensureWindowContains, but honors side paddings.
    void scrollIntoWindow(milliseconds position, bool animated);

    void generateProgressPatterns();

    void updateBookmarksViewerLocation();

    QnBookmarksViewer* createBookmarksViewer(QWidget* tooltipParentWidget);

    bool isWindowBeingDragged() const;

    // Remove from public everything qint64-position-based from parent classes.
    using base_type::setTickInterval;
    using base_type::tickInterval;
    using base_type::setMinimum;
    using base_type::setMaximum;
    using base_type::setRange;
    using base_type::singleStep;
    using base_type::setSingleStep;
    using base_type::pageStep;
    using base_type::setPageStep;
    using base_type::sliderPosition;
    using base_type::setSliderPosition;
    using base_type::setValue;
    using base_type::valueChanged;
    using base_type::pageStepChanged;
    using base_type::singleStepChanged;
    using base_type::sliderMoved;
    using base_type::rangeChanged;

private:
    Q_DECLARE_PRIVATE(GraphicsSlider);

    friend class QnTimeSliderChunkPainter;
    friend class QnTimeSliderStepStorage;

    QPointer<QGraphicsView> m_view;
    const QGraphicsItem* m_tooltipParent;
    QPointer<TimeMarker> m_tooltip;
    bool m_tooltipPositionUpdateQueued = false;

    milliseconds m_windowStart, m_windowEnd;
    milliseconds m_minimalWindow;

    milliseconds m_selectionStart, m_selectionEnd;
    bool m_selectionValid;

    milliseconds m_oldMinimum, m_oldMaximum;
    Options m_options;

    QnLinearFunction m_unboundMapper;
    QnLinearFunction m_thumbnailsUnboundMapper;
    QnBoundedLinearFunction m_boundMapper;

    milliseconds m_zoomAnchor;
    bool m_animatingSliderWindow;
    bool m_kineticsHurried;
    milliseconds m_targetStart, m_targetEnd;
    Marker m_dragMarker;
    QPointF m_dragDelta;
    bool m_dragIsClick;
    bool m_selecting;

    int m_lineCount;
    qreal m_totalLineStretch;
    std::vector<LineData> m_lineData;

    const QVector<QnTimeStep>* m_steps;
    std::vector<TimeStepData> m_stepData;
    int m_maxStepIndex;
    int m_textMinStepIndex = 0;
    qreal m_msecsPerPixel;
    qreal m_animationUpdateMSecsPerPixel;
    std::vector<milliseconds> m_nextTickmarkPos;
    std::vector<QList<QPointF> > m_tickmarkLines;

    std::unique_ptr<ThumbnailLoadingManager> m_thumbnailManager;
    ThumbnailPanel* m_thumbnailPanel = nullptr;
    qreal m_thumbnailsAspectRatio;
    QTimer* m_thumbnailsUpdateTimer;
    milliseconds m_lastThumbnailsUpdateTime;
    QMap<milliseconds, ThumbnailData> m_thumbnailData;
    QList<ThumbnailData> m_oldThumbnailData;
    QRectF m_thumbnailsPaintRect;
    milliseconds m_lastHoverThumbnail;
    bool m_thumbnailsVisible;
    qreal m_thumbnailsOpacity = 1.0;
    int m_thumbnailsOpacityIncrementSign = 0;
    bool m_tooltipVisible;

    int m_lastMinuteAnimationDelta;
    QPixmap m_progressPastPattern;
    QPixmap m_progressFuturePattern;
    QVector<bool> m_lastMinuteIndicatorVisible;

    QnTimeSliderPixmapCache* m_pixmapCache;

    QVector<milliseconds> m_indicators;

    milliseconds m_localOffset;

    QLocale m_locale;

    std::optional<QPointF> m_hoverMousePos;
    qreal m_lastLineBarValue;

    QnBookmarksViewer* m_bookmarksViewer;
    bool m_bookmarksVisible;
    QnBookmarkMergeHelperPtr m_bookmarksHelper;

    bool m_liveSupported;
    bool m_selectionInitiated;

    std::unique_ptr<LivePreview> m_livePreview;
    bool m_useLivePreview = false;
    bool m_livePreviewAllowed = true;

    bool m_updatingValue;

    milliseconds m_dragWindowStart = milliseconds(0);

    Qn::TimePeriodContent m_selectedExtraContent = Qn::RecordingContent;

    const QScopedPointer<KineticZoomHandler> m_kineticZoomHandler;
    const QScopedPointer<KineticScrollHandler> m_kineticScrollHandler;

    bool m_isLive;

    QPointer<QWidget> m_lastFocusedWidget;

    AnimationTimerListenerPtr m_animationTimerListener = AnimationTimerListener::create();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnTimeSlider::Options);
