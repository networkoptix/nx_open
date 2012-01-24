#ifndef QN_RESOURCE_WIDGET_H
#define QN_RESOURCE_WIDGET_H

#include <QWeakPointer>
#include <QVector>
#include <camera/render_status.h>
#include <ui/widgets2/graphicswidget.h>
#include <ui/common/constrained_resizable.h>
#include <ui/common/scene_utility.h>
#include <ui/common/frame_section_queryable.h>
#include <core/resource/resource_consumer.h>
#include <core/datapacket/mediadatapacket.h> /* For QnMetaDataV1Ptr. */
#include "polygonal_shadow_item.h"

class QGraphicsLinearLayout;

class QnResourceWidgetRenderer;
class QnVideoResourceLayout;
class QnWorkbenchItem;
class QnResourceDisplay;
class QnPolygonalShadowItem;
class QnAbstractArchiveReader;

class QnResourceWidget: public GraphicsWidget, public QnPolygonalShapeProvider, public ConstrainedResizable, public FrameSectionQuearyable, protected SceneUtility {
    Q_OBJECT;
    Q_PROPERTY(qreal frameOpacity READ frameOpacity WRITE setFrameOpacity);
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth);
    Q_PROPERTY(QPointF shadowDisplacement READ shadowDisplacement WRITE setShadowDisplacement);
    Q_PROPERTY(QRectF enclosingGeometry READ enclosingGeometry WRITE setEnclosingGeometry);
    Q_PROPERTY(qreal enclosingAspectRatio READ enclosingAspectRatio WRITE setEnclosingAspectRatio);

    typedef GraphicsWidget base_type;

public:
    enum DisplayFlag {
        DISPLAY_ACTIVITY_OVERLAY,  /**< Whether the paused overlay icon should be displayed. */
        DISPLAY_SELECTION_OVERLAY, /**< Whether selected / not selected state should be displayed. */
        DISPLAY_MOTION_GRID,       /**< Whether a grid with motion detection is to be displayed. */
    };
    Q_DECLARE_FLAGS(DisplayFlags, DisplayFlag)

    /**
     * Constructor.
     *
     * \param item                      Workbench item that this resource widget will represent.
     * \param parent                    Parent item.
     */
    QnResourceWidget(QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

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
     * Adds a button to this widget.
     *
     * Widget decides where to place buttons and creates an appropriate layout for them.
     * Deciding which buttons to add, assigning actions to them, and actually adding them is
     * up to the user.
     *
     * \param button                    Button to add. Ownership of the button is
     *                                  transferred to this widget.
     */
    void addButton(QGraphicsLayoutItem *button);

    /**
     * Removes a button from this widget.
     *
     * \param button                    Button to add. Ownership of the button is
     *                                  transferred to the caller.
     */
    void removeButton(QGraphicsLayoutItem *button);

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

    using base_type::mapRectToScene;

public Q_SLOTS:
    void showActivityDecorations();
    void hideActivityDecorations();

    /**
     * Clears this widget's motion selection region.
     */
    void clearMotionSelection();

Q_SIGNALS:
    void aspectRatioChanged(qreal oldAspectRatio, qreal newAspectRatio);
    void aboutToBeDestroyed();
    void motionRegionSelected(QnResourcePtr resource, QnAbstractArchiveReader* reader, QRegion region);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPointF &pos) const override;
    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF &region) const override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual bool windowFrameEvent(QEvent *event) override;

    virtual QPolygonF provideShape() override;

    virtual QSizeF constrainedSize(const QSizeF constraint) const override;
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

    void updateShadowZ();
    void updateShadowPos();
    void updateShadowOpacity();
    void invalidateShadowShape();

    void ensureAboutToBeDestroyedEmitted();

private Q_SLOTS:
    void at_sourceSizeChanged(const QSize &size);
    void at_display_resourceUpdated();

private:
    /**
     * \param channel                   Channel number.
     * \returns                         Rectangle in local coordinates where given channel is to be drawn.
     */
    QRectF channelRect(int channel) const;

    void ensureMotionMask();

    void invalidateMotionMask();

    enum OverlayIcon {
        NO_ICON,
        PAUSED,
        LOADING
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

    void drawCurrentTime(QPainter *painter, const QRectF &rect, qint64 time);

    void drawMotionGrid(QPainter *painter, const QRectF &rect, const QnMetaDataV1Ptr &motion);

    void drawMotionMask(QPainter *painter, const QRectF& rect);

    void drawFilledRegion(QPainter *painter, const QRectF &rect, const QRegion &selection, const QColor& color);

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

    /** Layout for buttons. */
    QGraphicsLinearLayout *m_buttonsLayout;

    /** Whether aboutToBeDestroyed signal has already been emitted. */
    bool m_aboutToBeDestroyedEmitted;

    /** Additional per-channel state. */
    QVector<ChannelState> m_channelState;

    /** Image region where motion is currently present, in parrots. */
    QRegion m_motionMask;

    /** Binary mask for the current motion region. */
    __m128i *m_motionMaskBinData;

    /** Whether the motion mask is valid. */
    bool m_motionMaskValid;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceWidget::DisplayFlags);

#endif // QN_RESOURCE_WIDGET_H
