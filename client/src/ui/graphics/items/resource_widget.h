#ifndef QN_RESOURCE_WIDGET_H
#define QN_RESOURCE_WIDGET_H

#include <QStaticText>
#include <QWeakPointer>
#include <QVector>
#include <camera/render_status.h>
#include <ui/graphics/items/standard/graphicswidget.h>
#include <ui/common/constrained_resizable.h>
#include <ui/common/scene_utility.h>
#include <ui/common/frame_section_queryable.h>
#include <ui/workbench/workbench_context_aware.h>
#include <core/resource/resource_consumer.h>
#include <core/datapacket/mediadatapacket.h> /* For QnMetaDataV1Ptr. */
#include "polygonal_shadow_item.h"
#include "core/resource/resource_media_layout.h"
#include "core/resource/motion_window.h"

class QGraphicsLinearLayout;

class QnViewportBoundWidget;
class QnResourceWidgetRenderer;
class QnVideoResourceLayout;
class QnWorkbenchItem;
class QnResourceDisplay;
class QnPolygonalShadowItem;
class QnAbstractArchiveReader;

class QnLoadingProgressPainter;
class QnPausedPainter;
class QnImageButtonWidget;

class GraphicsLabel;
class Instrument;

/* Get rid of stupid win32 defines. */
#ifdef NO_DATA
#   undef NO_DATA
#endif

class QnResourceWidget: public GraphicsWidget, public QnWorkbenchContextAware, public QnPolygonalShapeProvider, public ConstrainedResizable, public FrameSectionQuearyable, protected SceneUtility {
    Q_OBJECT;
    Q_PROPERTY(qreal frameOpacity READ frameOpacity WRITE setFrameOpacity);
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth);
    Q_PROPERTY(QPointF shadowDisplacement READ shadowDisplacement WRITE setShadowDisplacement);
    Q_PROPERTY(QRectF enclosingGeometry READ enclosingGeometry WRITE setEnclosingGeometry);
    Q_PROPERTY(qreal enclosingAspectRatio READ enclosingAspectRatio WRITE setEnclosingAspectRatio);
    Q_FLAGS(DisplayFlags DisplayFlag);

    typedef GraphicsWidget base_type;

public:
    enum DisplayFlag {
        DISPLAY_ACTIVITY_OVERLAY   = 0x1, /**< Whether the paused overlay icon should be displayed. */
        DISPLAY_SELECTION_OVERLAY  = 0x2, /**< Whether selected / not selected state should be displayed. */
        DISPLAY_MOTION_GRID        = 0x4, /**< Whether a grid with motion detection is to be displayed. */
        DISPLAY_BUTTONS            = 0x8, /**< Whether item buttons are to be displayed. */
        DISPLAY_MOTION_SENSITIVITY = 0x10, /**< Whether a grid with motion region sensitivity is to be displayed. */
    };
    Q_DECLARE_FLAGS(DisplayFlags, DisplayFlag)

    /**
     * Constructor.
     *
     * \param item                      Workbench item that this resource widget will represent.
     * \param parent                    Parent item.
     */
    QnResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnResourceWidget();

    /**
     * \returns                         Resource associated with this widget.
     */
    const QnResourcePtr &resource() const;

    /**
     * \returns                         Associated renderer, if any.
     */
    QnResourceWidgetRenderer *renderer() const {
        return m_renderer;
    }

    /**
     * \returns                         Workbench item associated with this widget. Never returns NULL.
     */
    QnWorkbenchItem *item() const {
        return m_item.data();
    }

    /**
     * \returns                         Display associated with this widget.
     */
    QnResourceDisplay *display() const {
        return m_display;
    }

    /**
     * \returns                         Shadow item associated with this widget.
     */
    QnPolygonalShadowItem *shadow() const {
        return m_shadow.data();
    }

    /**
     * \returns                         Frame opacity of this widget.
     */
    qreal frameOpacity() const {
        return m_frameOpacity;
    }

    /**
     * \param frameColor                New frame opacity for this widget.
     */
    void setFrameOpacity(qreal frameOpacity) {
        m_frameOpacity = frameOpacity;
    }

    /**
     * \returns                         Frame width of this widget.
     */
    qreal frameWidth() const {
        return m_frameWidth;
    }

    /**
     * \param frameWidth                New frame width for this widget.
     */
    void setFrameWidth(qreal frameWidth);

    /**
     * \returns                         Shadow displacement of this widget.
     */
    const QPointF &shadowDisplacement() const {
        return m_shadowDisplacement;
    }

    /**
     * Shadow will be drawn with the given displacement in scene coordinates
     * relative to this widget.
     *
     * \param displacement              New shadow displacement for this widget.
     */
    void setShadowDisplacement(const QPointF &displacement);

    virtual void setGeometry(const QRectF &geometry) override;

    /**
     * \returns                         Aspect ratio of this widget.
     *                                  Negative value will be returned if this
     *                                  widget does not have aspect ratio.
     */
    qreal aspectRatio() const {
        return m_aspectRatio;
    }

    /**
     * \returns                         Whether this widget has an aspect ratio.
     */
    bool hasAspectRatio() const {
        return m_aspectRatio > 0.0;
    }

    /**
     * Every widget is considered to be inscribed into an enclosing rectangle with a
     * fixed aspect ratio. When aspect ratio of the widget itself changes, it is
     * re-inscribed into its enclosed rectangle.
     *
     * \returns                         Aspect ratio of the enclosing rectangle for this widget.
     */
    qreal enclosingAspectRatio() const {
        return m_enclosingAspectRatio;
    }

    /**
     * \param enclosingAspectRatio      New enclosing aspect ratio.
     */
    void setEnclosingAspectRatio(qreal enclosingAspectRatio);

    /**
     * \returns                         Geometry of the enclosing rectangle for this widget.
     */
    QRectF enclosingGeometry() const;

    /**
     * \param enclosingGeometry         Geometry of the enclosing rectangle for this widget.
     */
    void setEnclosingGeometry(const QRectF &enclosingGeometry);

    /**
     * \returns                         Display flags for this widget.
     */
    DisplayFlags displayFlags() const {
        return m_displayFlags;
    }

    /**
     * \param flag                      Affected flag.
     * \param value                     New value for the affected flag.
     */
    void setDisplayFlag(DisplayFlag flag, bool value = true) {
        setDisplayFlags(value ? m_displayFlags | flag : m_displayFlags & ~flag);
    }

    /**
     * \param flags                     New display flags for this widget.
     */
    void setDisplayFlags(DisplayFlags flags);

    /**
     * \returns                         Whether a grid with motion detection is
     *                                  displayed over a video.
     */
    bool isMotionGridDisplayed() const {
        return m_displayFlags & DISPLAY_MOTION_GRID;
    }

    /**
     * \param itemPos                   Point in item coordinates to map to grid coordinates.
     * \returns                         Coordinates of the motion cell that the given point belongs to.
     *                                  Note that motion grid is finite, so even if the
     *                                  passed coordinate lies outside the item boundary,
     *                                  returned joint will lie inside it.
     */
    QPoint mapToMotionGrid(const QPointF &itemPos);

    /**
     * \param gridPos                   Coordinate of the motion grid cell.
     * \returns                         Position in scene coordinates of the top left corner of the grid cell.
     */
    QPointF mapFromMotionGrid(const QPoint &gridPos);

    /**
     * \param gridRect                  Rectangle in grid coordinates to add to
     *                                  selected motion region of this widget.
     */
    void addToMotionSelection(const QRect &gridRect);

    void addToMotionRegion(int sens, const QRect& rect, int channel);

    void clearMotionRegions();

    using base_type::mapRectToScene;

    Qn::RenderStatus currentRenderStatus() const {
        return m_renderStatus;
    }

    enum MotionDrawType {
        DrawMaskOnly,
        DrawAllMotionInfo
    };
    QList<QnMotionRegion>& getMotionRegionList();

public slots:
    void showActivityDecorations();
    void hideActivityDecorations();
    void fadeOutOverlay();
    void fadeInOverlay();
    void fadeOutInfo();
    void fadeInInfo();
    void fadeInfo(bool fadeIn);

    /**
     * Clears this widget's motion selection region.
     */
    void clearMotionSelection();
    void setDrawMotionWindows(MotionDrawType value);
signals:
    void aspectRatioChanged(qreal oldAspectRatio, qreal newAspectRatio);
    void aboutToBeDestroyed();
    void motionRegionSelected(QnResourcePtr resource, QnAbstractArchiveReader* reader, QList<QRegion> region);
    void displayFlagsChanged();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPointF &pos) const override;
    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF &region) const override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual bool windowFrameEvent(QEvent *event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    virtual QPolygonF provideShape() override;

    virtual QSizeF constrainedSize(const QSizeF constraint) const override;
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

    void updateShadowZ();
    void updateShadowPos();
    void updateShadowOpacity();
    void updateShadowVisibility();
    void invalidateShadowShape();

    void ensureAboutToBeDestroyedEmitted();

    void ensureMotionMask();
    void invalidateMotionMask();
    void ensureMotionMaskBinData();
    void invalidateMotionMaskBinData();

    int motionGridWidth() const;
    int motionGridHeight() const;
    void drawMotionSensitivity(QPainter *painter, const QRectF &rect, const QnMotionRegion& region, int channel);
signals:
    void updateOverlayTextLater();

private slots:
    void updateOverlayText();

    void at_sourceSizeChanged(const QSize &size);
    void at_resource_resourceChanged();
    void at_resource_nameChanged();
    void at_camDisplay_stillImageChanged();

private:
    /**
     * \param channel                   Channel number.
     * \returns                         Rectangle in local coordinates where given channel is to be drawn.
     */
    QRectF channelRect(int channel) const;

    enum OverlayIcon {
        NO_ICON,
        PAUSED,
        LOADING,
        NO_DATA,
        OFFLINE,
        UNAUTHORIZED
    };

    struct ChannelState {
        ChannelState(): icon(NO_ICON), iconChangeTimeMSec(0), iconFadeInNeeded(false), lastNewFrameTimeMSec(0) {}

        /** Current overlay icon. */
        OverlayIcon icon;

        /** Time when the last icon change has occurred, in milliseconds since epoch. */
        qint64 iconChangeTimeMSec;

        /** Whether the icon should fade in on change. */
        bool iconFadeInNeeded;

        /** Last time when new frame was rendered, in milliseconds since epoch. */
        qint64 lastNewFrameTimeMSec;

        /** Selected region for search-by-motion, in parrots. */
        QRegion motionSelection;
    };

    void setOverlayIcon(int channel, OverlayIcon icon);

    void drawSelection(const QRectF &rect);

    void drawOverlayIcon(int channel, const QRectF &rect);

    void drawMotionGrid(QPainter *painter, const QRectF &rect, const QnMetaDataV1Ptr &motion, int channel);

    void drawMotionMask(QPainter *painter, const QRectF& rect, int channel);

    void drawFilledRegion(QPainter *painter, const QRectF &rect, const QRegion &selection, const QColor& color, const QColor& penColor);

    void drawFlashingText(QPainter *painter, const QStaticText &text, qreal textSize, const QPointF &offset = QPointF());

private:
    /** Layout item. */
    QWeakPointer<QnWorkbenchItem> m_item;

    /** Resource associated with this widget. */
    QnResourcePtr m_resource;

    /** Display. */
    QnResourceDisplay *m_display;

    /* Display flags. */
    DisplayFlags m_displayFlags;

    /** Resource layout of this display widget. */
    const QnVideoResourceLayout *m_videoLayout;

    /** Number of media channels. */
    int m_channelCount;

    /** Associated renderer. */
    QnResourceWidgetRenderer *m_renderer;

    /** Paused painter. */
    QSharedPointer<QnPausedPainter> m_pausedPainter;

    /** Loading progress painter. */
    QSharedPointer<QnLoadingProgressPainter> m_loadingProgressPainter;

    /** Aspect ratio. Negative value means that aspect ratio is not enforced. */
    qreal m_aspectRatio;

    /** Aspect ratio of the virtual enclosing rectangle. */
    qreal m_enclosingAspectRatio;

    /** Cached size of a single media channel, in screen coordinates. */
    QSize m_channelScreenSize;

    /** Shadow item. */
    QWeakPointer<QnPolygonalShadowItem> m_shadow;

    /** Frame opacity. */
    qreal m_frameOpacity;

    /** Shadow displacement. */
    QPointF m_shadowDisplacement;

    /** Frame width. */
    qreal m_frameWidth;

    /* Widgets for overlaid stuff. */
    QnViewportBoundWidget *m_headerOverlayWidget;

    QnViewportBoundWidget *m_footerOverlayWidget;

    QGraphicsWidget *m_footerWidget;

    GraphicsLabel *m_headerTitleLabel;

    GraphicsLabel *m_footerStatusLabel;

    GraphicsLabel *m_footerTimeLabel;

    QnImageButtonWidget *m_infoButton;

    /** Whether aboutToBeDestroyed signal has already been emitted. */
    bool m_aboutToBeDestroyedEmitted;

    /** Additional per-channel state. */
    QVector<ChannelState> m_channelState;

    /** Image region where motion is currently present, in parrots. */
    QList<QnMotionRegion> m_motionRegionList;

    /** Whether the motion mask is valid. */
    bool m_motionMaskValid;

    /** Binary mask for the current motion region. */
    __m128i *m_motionMaskBinData[CL_MAX_CHANNELS];

    /** Whether motion mask binary data is valid. */
    bool m_motionMaskBinDataValid;

    /** Status of the last painting operation. */
    Qn::RenderStatus m_renderStatus;

    QStaticText m_noDataStaticText;
    QStaticText m_offlineStaticText;
    QStaticText m_unauthorizedStaticText;
    QStaticText m_unauthorizedStaticText2;
    MotionDrawType m_motionDrawType;
    QStaticText m_sensStaticText[10];
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceWidget::DisplayFlags);

#endif // QN_RESOURCE_WIDGET_H
