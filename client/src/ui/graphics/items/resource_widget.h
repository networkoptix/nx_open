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
#include "polygonal_shadow_item.h"

class QGraphicsLinearLayout;

class QnResourceWidgetRenderer;
class QnVideoResourceLayout;
class QnWorkbenchItem;
class QnResourceDisplay;
class QnPolygonalShadowItem;

class QnResourceWidget: public GraphicsWidget, public QnPolygonalShapeProvider, public ConstrainedResizable, public FrameSectionQuearyable, protected QnSceneUtility {
    Q_OBJECT;
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor);
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth);
    Q_PROPERTY(QPointF shadowDisplacement READ shadowDisplacement WRITE setShadowDisplacement);
    Q_PROPERTY(QRectF enclosingGeometry READ enclosingGeometry WRITE setEnclosingGeometry);
    Q_PROPERTY(qreal enclosingAspectRatio READ enclosingAspectRatio WRITE setEnclosingAspectRatio);

    typedef GraphicsWidget base_type;

public:
    QnResourceWidget(QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

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
        return m_item;
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
     * \returns                         Frame color of this widget.
     */
    const QColor &frameColor() const {
        return m_frameColor;
    }

    /**
     * \param frameColor                New frame color for this widget.
     */
    void setFrameColor(const QColor &frameColor) {
        m_frameColor = frameColor;
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
     * Every widget is considered inscribed into an enclosing rectangle with a 
     * fixed aspect ratio. When aspect ratio of a widget changes, actual geometry
     * of its enclosing rectangle is recalculated, and this widget is re-inscribed
     * into it.
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

    using base_type::mapRectToScene;

signals:
    void aspectRatioChanged(qreal oldAspectRatio, qreal newAspectRatio);
    void aboutToBeDestroyed();

public slots:
    void showActivityDecorations();
    void hideActivityDecorations();

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
    void invalidateShadowShape();

    void ensureAboutToBeDestroyedEmitted();

private slots:
    void at_sourceSizeChanged(const QSize &size);

private:
    /**
     * \param channel                   Channel number.
     * \returns                         Rectangle in local coordinates where given channel is to be drawn.
     */
    QRectF channelRect(int channel) const;

    enum OverlayIcon {
        NO_ICON,
        PAUSED,
        LOADING
    };

    struct OverlayState {
        OverlayState(): icon(NO_ICON), changeTimeMSec(0), fadeInNeeded(false) {}

        OverlayIcon icon;
        qint64 changeTimeMSec;
        bool fadeInNeeded;
    };

    void setOverlayIcon(int channel, OverlayIcon icon);

    void drawOverlayIcon(int channel, const QRectF &rect);

    void drawCurrentTime(QPainter *painter, const QRectF& rect, qint64 time);

private:
    /** Layout item. */
    QnWorkbenchItem *m_item;

    /** Display. */
    QnResourceDisplay *m_display;

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

    /** Frame color. */
    QColor m_frameColor;

    /** Shadow displacement. */
    QPointF m_shadowDisplacement;

    /** Frame width. */
    qreal m_frameWidth;

    /** Layout for buttons. */
    QGraphicsLinearLayout *m_buttonsLayout;

    /** Whether aboutToBeDestroyed signal has already been emitted. */
    bool m_aboutToBeDestroyedEmitted;

    /** Whether activity decorations are visible. */
    bool m_activityDecorationsVisible;

    /** Time when the last new frame was rendered, in milliseconds. */
    qint64 m_lastNewFrameTimeMSec;

    /** Current per-channel overlay state. */
    QVector<OverlayState> m_overlayState;
};

#endif // QN_RESOURCE_WIDGET_H
