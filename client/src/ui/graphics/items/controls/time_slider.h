#ifndef QN_TIME_SLIDER_H
#define QN_TIME_SLIDER_H

#include <recording/time_period_list.h>
#include <recording/time_period_storage.h>

#include <ui/common/functors.h>
#include <ui/graphics/items/generic/tool_tip_slider.h>
#include <ui/processors/kinetic_process_handler.h>
#include <ui/processors/drag_process_handler.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/animation/animated.h>

#include "time_step.h"
#include "camera/thumbnail.h"

class QTimer;

class QnThumbnailsLoader;
class QnTimeSliderPixmapCache;

class QnTimeSlider: public Animated<QnToolTipSlider>, protected KineticProcessHandler, protected DragProcessHandler, protected AnimationTimerListener {
    Q_OBJECT;
    Q_PROPERTY(qint64 windowStart READ windowStart WRITE setWindowStart);
    Q_PROPERTY(qint64 windowEnd READ windowEnd WRITE setWindowEnd);

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
    };
    Q_DECLARE_FLAGS(Options, Option);

    explicit QnTimeSlider(QGraphicsItem *parent = NULL);
    virtual ~QnTimeSlider();

    int lineCount() const;
    void setLineCount(int lineCount);

    void setLineVisible(int line, bool visible);
    bool isLineVisible(int line) const;

    void setLineStretch(int line, qreal stretch);
    qreal lineStretch(int line) const;

    void setLineComment(int line, const QString &comment);
    QString lineComment(int line);

    QnTimePeriodList timePeriods(int line, Qn::TimePeriodRole type) const;
    void setTimePeriods(int line, Qn::TimePeriodRole type, const QnTimePeriodList &timePeriods);

    Options options() const;
    void setOptions(Options options);
    void setOption(Option option, bool value);

    qint64 windowStart() const;
    void setWindowStart(qint64 windowStart);

    qint64 windowEnd() const;
    void setWindowEnd(qint64 windowEnd);

    void setWindow(qint64 start, qint64 end);

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

    const QString &toolTipFormat() const;
    void setToolTipFormat(const QString &format);

    Q_SLOT void finishAnimations();
    Q_SLOT void animatedUnzoom();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    virtual QPointF positionFromValue(qint64 logicalValue, bool bound = true) const override;
    virtual qint64 valueFromPosition(const QPointF &position, bool bound = true) const override;

    bool isThumbnailsVisible() const;
    qreal thumbnailsHeight() const;
    qreal rulerHeight() const;
    void setRulerHeight(qreal rulerHeight);

    QnThumbnailsLoader *thumbnailsLoader() const;
    void setThumbnailsLoader(QnThumbnailsLoader *value);

signals:
    void windowChanged(qint64 windowStart, qint64 windowEnd);
    void selectionChanged(qint64 selectionStart, qint64 selectionEnd);
    void customContextMenuRequested(const QPointF &pos, const QPoint &screenPos);
    void selectionPressed();
    void selectionReleased();
    void thumbnailsVisibilityChanged();

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    virtual void tick(int deltaMSecs) override;

    virtual void kineticMove(const QVariant &degrees) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;

    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

    static QVector<QnTimeStep> createRelativeSteps();
    static QVector<QnTimeStep> createAbsoluteSteps();
    static QVector<QnTimeStep> enumerateSteps(const QVector<QnTimeStep> &steps);

private:
    enum Marker {
        NoMarker,
        SelectionStartMarker,
        SelectionEndMarker,
        CreateSelectionMarker
    };

    struct TimeStepData {
        TimeStepData(): currentHeight(0.0), targetHeight(0.0), currentLineOpacity(0.0), targetLineOpacity(0.0), currentTextOpacity(0.0), targetTextOpacity(0.0) {}

        qreal currentHeight;
        qreal targetHeight;
        qreal currentLineOpacity;
        qreal targetLineOpacity;
        qreal currentTextOpacity;
        qreal targetTextOpacity;

        int currentTextHeight;
        qreal currentLineHeight;
    };

    struct LineData {
        LineData(): visible(true), stretch(1.0) {}

        QnTimePeriodStorage timeStorage;
        QString comment;
        QPixmap commentPixmap;
        bool visible;
        qreal stretch;
    };

    struct ThumbnailData {
        ThumbnailData(): pos(0.0), opacity(0.0), hiding(false), selection(0.0), selecting(false) {}
        ThumbnailData(const QnThumbnail &thumbnail): thumbnail(thumbnail), pos(0.0), opacity(0.0), hiding(false), selection(0.0), selecting(false) {}

        QnThumbnail thumbnail;
        qreal pos;
        qreal opacity;
        qreal selection;
        bool hiding;
        bool selecting;
    };

private:
    Marker markerFromPosition(const QPointF &pos, qreal maxDistance = 1.0) const;
    QPointF positionFromMarker(Marker marker) const;

    qreal quickPositionFromValue(qint64 logicalValue, bool bound = true) const;

    QRectF thumbnailsRect() const;
    QRectF rulerRect() const;
    qreal effectiveLineStretch(int line) const;

    void setMarkerSliderPosition(Marker marker, qint64 position);

    bool isAnimatingWindow() const;

    bool scaleWindow(qreal factor, qint64 anchor);

    void drawPeriodsBar(QPainter *painter, QnTimePeriodList &recorded, QnTimePeriodList &motion, const QRectF &rect);
    void drawTickmarks(QPainter *painter, const QRectF &rect);
    void drawSolidBackground(QPainter *painter, const QRectF &rect);
    void drawMarker(QPainter *painter, qint64 pos, const QColor &color);
    void drawSelection(QPainter *painter);
    void drawSeparator(QPainter *painter, const QRectF &rect);
    void drawDates(QPainter *painter, const QRectF &rect);
    void drawThumbnails(QPainter *painter, const QRectF &rect);
    void drawThumbnail(QPainter *painter, const ThumbnailData &data, const QRectF &targetRect, const QRectF &boundingRect);

    void updateVisibleLineCount();
    void updateToolTipVisibility();
    void updateToolTipText();
    void updateSteps();
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
    Q_SLOT void updateThumbnailsStepSizeTimer();
    Q_SLOT void updateThumbnailsStepSizeForced();

    Q_SLOT void addThumbnail(const QnThumbnail &thumbnail);
    Q_SLOT void clearThumbnails();

    void animateStepValues(int deltaMSecs);
    void animateThumbnails(int deltaMSecs);
    bool animateThumbnail(qreal dt, ThumbnailData &data);
    void freezeThumbnails();

    void setThumbnailSelecting(qint64 time, bool selecting);

private:
    Q_DECLARE_PRIVATE(GraphicsSlider);

    qint64 m_windowStart, m_windowEnd;
    qint64 m_minimalWindow;

    qint64 m_selectionStart, m_selectionEnd;
    bool m_selectionValid;

    qint64 m_oldMinimum, m_oldMaximum;
    Options m_options;
    QString m_toolTipFormat;

    QnLinearFunction m_unboundMapper;
    QnBoundedLinearFunction m_boundMapper;

    qint64 m_zoomAnchor;
    bool m_unzooming;
    Marker m_dragMarker;
    QPointF m_dragDelta;
    bool m_dragIsClick;
    bool m_selecting;

    int m_lineCount;
    qreal m_totalLineStretch;
    QVector<LineData> m_lineData;

    QVector<QnTimeStep> m_steps;
    QVector<TimeStepData> m_stepData;
    qreal m_msecsPerPixel;
    qreal m_animationUpdateMSecsPerPixel;
    QVector<qint64> m_nextTickmarkPos;
    QVector<QVector<QPointF> > m_tickmarkLines;

    QWeakPointer<QnThumbnailsLoader> m_thumbnailsLoader;
    QTimer *m_thumbnailsUpdateTimer;
    qint64 m_lastThumbnailsUpdateTime;
    QPixmap m_noThumbnailsPixmap;
    QMap<qint64, ThumbnailData> m_thumbnailData;
    QList<ThumbnailData> m_oldThumbnailData;
    QRectF m_thumbnailsPaintRect;
    qint64 m_lastHoverThumbnail;
    bool m_thumbnailsVisible;

    qreal m_rulerHeight;
    qreal m_prefferedHeight;

    QnTimeSliderPixmapCache *m_pixmapCache;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnTimeSlider::Options);

#endif // QN_TIME_SLIDER_H
