#pragma once

#include <QtCore/QLocale>
#include <QtCore/QScopedPointer>

#include <utils/math/functors.h>

#include <client/client_color_types.h>

#include <recording/time_period_storage.h>

#include <camera/thumbnail.h>

#include <core/resource/camera_bookmark_fwd.h>

#include <ui/graphics/items/generic/tool_tip_slider.h>
#include <ui/processors/kinetic_process_handler.h>
#include <ui/processors/drag_process_handler.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/animation/animated.h>
#include <ui/common/help_topic_queryable.h>

#include "time_step.h"

class QTimer;

class GraphicsLabel;
class QnThumbnailsLoader;
class QnTimeSliderPixmapCache;
class QnTimeSliderChunkPainter;
class QnTimePeriodList;
class QnBookmarksViewer;
class QnBookmarkMergeHelper;

class QnTimeSlider: public Animated<QnToolTipSlider>, public HelpTopicQueryable,
    protected DragProcessHandler, protected AnimationTimerListener
{
    Q_OBJECT
    Q_PROPERTY(qint64 windowStart READ windowStart WRITE setWindowStart)
    Q_PROPERTY(qint64 windowEnd READ windowEnd WRITE setWindowEnd)
    Q_PROPERTY(QnTimeSliderColors colors READ colors WRITE setColors)

    typedef Animated<QnToolTipSlider> base_type;

public:
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
        * Whether bookmarks viewer should be anchored to timeline pixel location (otherwise - to a timestamp).
        */
        StillBookmarksViewer = 0x2000,

        /**
        * Whether single click with mouse button immediately clears existing selection.
        */
        ClearSelectionOnClick = 0x4000
    };
    Q_DECLARE_FLAGS(Options, Option);

    explicit QnTimeSlider(QGraphicsItem* parent = nullptr, QGraphicsItem* tooltipParent = nullptr);

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

    qint64 windowStart() const;
    void setWindowStart(qint64 windowStart);

    qint64 windowEnd() const;
    void setWindowEnd(qint64 windowEnd);

    void setWindow(qint64 start, qint64 end, bool animate = false);
    void shiftWindow(qint64 delta, bool animate = false);

    bool windowContains(qint64 position);
    void ensureWindowContains(qint64 position);

    using base_type::setSliderPosition;
    void setSliderPosition(qint64 position, bool keepInWindow);

    using base_type::setValue;
    void setValue(qint64 value, bool keepInWindow);

    qint64 selectionStart() const;
    void setSelectionStart(qint64 selectionStart);

    qint64 selectionEnd() const;
    void setSelectionEnd(qint64 selectionEnd);

    void setSelection(qint64 start, qint64 end);

    bool isSelectionValid() const;
    void setSelectionValid(bool valid);

    bool isLiveSupported() const;
    void setLiveSupported(bool value);

    bool isLive() const;

    Q_SLOT void finishAnimations();
    Q_SLOT void hurryKineticAnimations();

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    virtual bool eventFilter(QObject* target, QEvent* event) override;

    virtual QPointF positionFromValue(qint64 logicalValue, bool bound = true) const override;
    virtual qint64 valueFromPosition(const QPointF& position, bool bound = true) const override;

    bool isThumbnailsVisible() const;
    qreal thumbnailsHeight() const;
    qreal rulerHeight() const;

    QnThumbnailsLoader* thumbnailsLoader() const;
    void setThumbnailsLoader(QnThumbnailsLoader* value, qreal aspectRatio); // TODO: #Elric remove aspectRatio

    const QVector<qint64>& indicators() const;
    void setIndicators(const QVector<qint64>& indicators);

    qint64 localOffset() const;
    void setLocalOffset(qint64 utcOffset);

    const QnTimeSliderColors& colors() const;
    void setColors(const QnTimeSliderColors& colors);

    bool isTooltipVisible() const;
    void setTooltipVisible(bool visible);

    void setLastMinuteIndicatorVisible(int line, bool visible);
    bool isLastMinuteIndicatorVisible(int line) const;

    virtual int helpTopicAt(const QPointF& pos) const override;

    QnBookmarksViewer* bookmarksViewer();

    typedef QSharedPointer<QnBookmarkMergeHelper> QnBookmarkMergeHelperPtr;
    void setBookmarksHelper(const QnBookmarkMergeHelperPtr& helper);
    bool isBookmarksVisible() const;
    void setBookmarksVisible(bool bookmarksVisible);

    qreal msecsPerPixel() const;

    bool positionMarkerVisible() const;

signals:
    void windowMoved();
    void windowChanged(qint64 windowStart, qint64 windowEnd);
    void selectionChanged(const QnTimePeriod& selection);
    void customContextMenuRequested(const QPointF& pos, const QPoint& screenPos);
    void selectionPressed();
    void selectionReleased();
    void thumbnailsVisibilityChanged();
    void thumbnailClicked();
    void msecsPerPixelChanged();
    void lineCommentChanged(int line, const QString& comment);

protected:
    virtual void sliderChange(SliderChange change) override;
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

    virtual void tick(int deltaMSecs) override;

    virtual void startDragProcess(DragInfo* info) override;
    virtual void startDrag(DragInfo* info) override;
    virtual void dragMove(DragInfo* info) override;
    virtual void finishDragProcess(DragInfo* info) override;

    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF& constraint) const override;

    static void createSteps(QVector<QnTimeStep>* absoluteSteps, QVector<QnTimeStep>* relativeSteps);
    static void enumerateSteps(QVector<QnTimeStep>& steps);

    virtual void setupShowAnimator(VariantAnimator* animator) const override;
    virtual void setupHideAnimator(VariantAnimator* animator) const override;

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
        LineData(): visible(true), stretch(1.0) {}

        QnTimePeriodStorage timeStorage;
        QString comment;
        QPixmap commentPixmap;
        bool visible;
        qreal stretch;
    };

    struct ThumbnailData
    {
        ThumbnailData(): pos(0.0), opacity(0.0), selection(0.0), hiding(false), selecting(false) {}
        ThumbnailData(const QnThumbnail& thumbnail): thumbnail(thumbnail), pos(0.0), opacity(0.0), selection(0.0), hiding(false), selecting(false) {}

        QnThumbnail thumbnail;
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
    Marker markerFromPosition(const QPointF& pos, qreal maxDistance = 1.0) const;
    QPointF positionFromMarker(Marker marker) const;

    qreal quickPositionFromValue(qint64 logicalValue, bool bound = true) const;

    QRectF thumbnailsRect() const;
    QRectF rulerRect() const;
    QRectF dateBarRect() const;
    QRectF tickmarkBarRect() const;
    QRectF lineBarRect() const;

    qreal effectiveLineStretch(int line) const;

    /* Returns tickmark level clamped in range [0...maxTickmarkLevels] (warning! not [0...maxTickmarkLevels-1]) */
    int tickmarkLevel(int stepIndex) const;

    QColor tickmarkLineColor(int level) const;
    QColor tickmarkTextColor(int level) const;

    void setMarkerSliderPosition(Marker marker, qint64 position);

    void extendSelection(qint64 position);

    bool isAnimatingWindow() const;

    bool scaleWindow(qreal factor, qint64 anchor);

    void drawPeriodsBar(QPainter* painter, const QnTimePeriodList& recorded, const QnTimePeriodList& motion, const QRectF& rect);
    void drawTickmarks(QPainter* painter, const QRectF& rect);
    void drawSolidBackground(QPainter* painter, const QRectF& rect);
    void drawMarker(QPainter* painter, qint64 pos, const QColor& color, qreal width = 1.0);
    void drawSelection(QPainter* painter);
    void drawSeparator(QPainter* painter, const QRectF& rect);
    void drawLastMinute(QPainter* painter, const QRectF& rect);
    void drawDates(QPainter* painter, const QRectF& rect);
    void drawThumbnails(QPainter* painter, const QRectF& rect);
    void drawThumbnail(QPainter* painter, const ThumbnailData& data, const QRectF& targetRect, const QRectF& boundingRect);
    void drawBookmarks(QPainter* painter, const QRectF& rect);

    int findTopLevelStepIndex() const;

    void updatePixmapCache();
    void updateToolTipVisibilityInternal(bool animated);
    void updateToolTipText();
    void updateSteps();
    void updateTickmarkTextSteps();
    void updateMSecsPerPixel();
    void updateMinimalWindow();
    void updateStepAnimationTargets();
    void updateLineCommentPixmap(int line);
    void updateLineCommentPixmaps();
    void updateAggregationValue();
    void updateTotalLineStretch();
    void updateThumbnailsStepSize(bool instant, bool forced = false);
    void updateThumbnailsPeriod();
    void updateThumbnailsStepSizeLater();
    void updateThumbnailsVisibility();
    void updateKineticProcessor();
    Q_SLOT void updateThumbnailsStepSizeTimer();
    Q_SLOT void updateThumbnailsStepSizeForced();

    Q_SLOT void addThumbnail(const QnThumbnail& thumbnail);
    Q_SLOT void clearThumbnails();

    void animateStepValues(int deltaMs);
    void animateThumbnails(int deltaMs);
    bool animateThumbnail(qreal dt, ThumbnailData& data);
    void freezeThumbnails();
    void animateLastMinute(int deltaMSecs);

    void setThumbnailSelecting(qint64 time, bool selecting);

    void setTargetStart(qint64 start);
    void setTargetEnd(qint64 end);
    qint64 targetStart();
    qint64 targetEnd();

    void generateProgressPatterns();

    void updateBookmarksViewerLocation();

    QnBookmarksViewer* createBookmarksViewer();

    bool isWindowBeingDragged() const;

private:
    Q_DECLARE_PRIVATE(GraphicsSlider);

    friend class QnTimeSliderChunkPainter;
    friend class QnTimeSliderStepStorage;

    QnTimeSliderColors m_colors;

    qint64 m_windowStart, m_windowEnd;
    qint64 m_minimalWindow;

    qint64 m_selectionStart, m_selectionEnd;
    bool m_selectionValid;

    qint64 m_oldMinimum, m_oldMaximum;
    Options m_options;

    QnLinearFunction m_unboundMapper;
    QnBoundedLinearFunction m_boundMapper;

    qint64 m_zoomAnchor;
    bool m_animatingSliderWindow;
    bool m_kineticsHurried;
    qint64 m_targetStart, m_targetEnd;
    Marker m_dragMarker;
    QPointF m_dragDelta;
    bool m_dragIsClick;
    bool m_selecting;

    int m_lineCount;
    qreal m_totalLineStretch;
    QVector<LineData> m_lineData;

    QVector<QnTimeStep> m_steps;
    QVector<TimeStepData> m_stepData;
    int m_maxStepIndex;
    qreal m_msecsPerPixel;
    qreal m_animationUpdateMSecsPerPixel;
    QVector<qint64> m_nextTickmarkPos;
    QVector<QVector<QPointF> > m_tickmarkLines;

    QPointer<QnThumbnailsLoader> m_thumbnailsLoader;
    qreal m_thumbnailsAspectRatio;
    QTimer* m_thumbnailsUpdateTimer;
    qint64 m_lastThumbnailsUpdateTime;
    QPixmap m_noThumbnailsPixmap;
    QMap<qint64, ThumbnailData> m_thumbnailData;
    QList<ThumbnailData> m_oldThumbnailData;
    QRectF m_thumbnailsPaintRect;
    qint64 m_lastHoverThumbnail;
    bool m_thumbnailsVisible;
    bool m_tooltipVisible;

    qreal m_rulerHeight;

    int m_lastMinuteAnimationDelta;
    QPixmap m_progressPastPattern;
    QPixmap m_progressFuturePattern;
    QVector<bool> m_lastMinuteIndicatorVisible;

    QnTimeSliderPixmapCache* m_pixmapCache;

    QVector<qint64> m_indicators;

    qint64 m_localOffset;

    QLocale m_locale;

    QPointF m_hoverMousePos;
    qreal m_lastLineBarValue;

    QnBookmarksViewer* m_bookmarksViewer;
    bool m_bookmarksVisible;
    QnBookmarkMergeHelperPtr m_bookmarksHelper;

    bool m_liveSupported;
    bool m_selectionInitiated;

    GraphicsLabel* m_tooltipLine1;
    GraphicsLabel* m_tooltipLine2;

    bool m_updatingValue;

    qint64 m_dragWindowStart = 0;

    const QScopedPointer<KineticZoomHandler> m_kineticZoomHandler;
    const QScopedPointer<KineticScrollHandler> m_kineticScrollHandler;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnTimeSlider::Options);
