#ifndef QN_DISPLAY_WIDGET_H
#define QN_DISPLAY_WIDGET_H

#include <QWeakPointer>
#include <ui/widgets2/graphicswidget.h>
#include "polygonal_shadow_item.h"

class QnDisplayWidgetRenderer;
class QnVideoResourceLayout;
class QnUiLayoutItem;
class QnPolygonalShadowItem;

class QnDisplayWidget: public GraphicsWidget, public QnPolygonalShapeProvider {
    Q_OBJECT;
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor);
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth);
    Q_PROPERTY(QPointF shadowDisplacement READ shadowDisplacement WRITE setShadowDisplacement)

    typedef GraphicsWidget base_type;

public:
    QnDisplayWidget(QnUiLayoutItem *entity, QGraphicsItem *parent = NULL);

    virtual ~QnDisplayWidget();

    /**
     * \returns                         Entity associated with this widget.
     */
    QnUiLayoutItem *entity() const {
        return m_entity;
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
     */
    qreal aspectRatio() const {
        return m_aspectRatio;
    }

signals:
    void aspectRatioChanged(qreal oldAspectRatio, qreal newAspectRatio);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPointF &pos) const override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;

    virtual QPolygonF provideShape() override;

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
    /** Display entity. */
    QnUiLayoutItem *m_entity;

    /** Resource layout of this display widget. */
    const QnVideoResourceLayout *m_resourceLayout;

    /** Number of media channels. */
    int m_channelCount;

    /** Associated renderer. */
    QnDisplayWidgetRenderer *m_renderer;

    /** Aspect ratio, free if zero. */
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
