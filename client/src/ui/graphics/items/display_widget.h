#ifndef QN_DISPLAY_WIDGET_H
#define QN_DISPLAY_WIDGET_H

#include <QWeakPointer>
#include <ui/widgets2/graphicswidget.h>
#include <ui/common/constrained_resizable.h>
#include "polygonal_shadow_item.h"

class QnDisplayWidgetRenderer;
class QnVideoResourceLayout;
class QnLayoutItemModel;
class QnResourceDisplay;
class QnPolygonalShadowItem;

class QnDisplayWidget: public GraphicsWidget, public QnPolygonalShapeProvider, public ConstrainedResizable {
    Q_OBJECT;
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor);
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth);
    Q_PROPERTY(QPointF shadowDisplacement READ shadowDisplacement WRITE setShadowDisplacement)

    typedef GraphicsWidget base_type;

public:
    QnDisplayWidget(QnLayoutItemModel *item, QGraphicsItem *parent = NULL);

    virtual ~QnDisplayWidget();

    /**
     * \returns                         Entity associated with this widget.
     */
    QnLayoutItemModel *item() const {
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

signals:
    void aspectRatioChanged(qreal oldAspectRatio, qreal newAspectRatio);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPointF &pos) const override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual bool windowFrameEvent(QEvent *event) override;

    virtual QPolygonF provideShape() override;

    virtual QSizeF constrainedSize(const QSizeF constraint) const override;

    void updateShadowZ();
    void updateShadowPos();
    void invalidateShadowShape();

private slots:
    void at_sourceSizeChanged(const QSize &size);

private:
    /**
     * \param channel                   Channel number.
     * \returns                         Rectangle in local coordinates where given channel is to be drawn.
     */
    QRectF channelRect(int channel) const;

private:
    /** Layout item. */
    QnLayoutItemModel *m_item;

    /** Display. */
    QnResourceDisplay *m_display;

    /** Resource layout of this display widget. */
    const QnVideoResourceLayout *m_resourceLayout;

    /** Number of media channels. */
    int m_channelCount;

    /** Associated renderer. */
    QnDisplayWidgetRenderer *m_renderer;

    /** Aspect ratio. Negative value means that aspect ratio is not enforced. */
    qreal m_aspectRatio;

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

};

#endif // QN_DISPLAY_WIDGET_H
