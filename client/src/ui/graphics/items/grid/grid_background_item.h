#ifndef GRID_BACKGROUND_ITEM_H
#define GRID_BACKGROUND_ITEM_H

#include <QObject>
#include <QGraphicsObject>

class QnWorkbenchGridMapper;
class RectAnimator;
class VariantAnimator;
class AnimationTimer;

class QnGridBackgroundItem : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)
    Q_PROPERTY(QRectF viewportRect READ viewportRect WRITE setViewportRect)

public:
    explicit QnGridBackgroundItem(QGraphicsItem *parent = NULL);
    virtual ~QnGridBackgroundItem();

    virtual QRectF boundingRect() const override;

    const QColor &color() const {
        return m_color;
    }

    void setColor(const QColor &color) {
        m_color = color;
    }

    const QRectF &viewportRect() const {
        return m_rect;
    }

    void setViewportRect(const QRectF &rect) {
        prepareGeometryChange();
        m_rect = rect;
    }

    const QRect &sceneRect() const {
        return m_sceneRect;
    }

    void setSceneRect(const QRect &rect);

    QnWorkbenchGridMapper *mapper() const {
        return m_mapper.data();
    }

    void setMapper(QnWorkbenchGridMapper *mapper);

    void setAnimationTimer(AnimationTimer *timer);

    void animatedShow();
    void animatedHide();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private slots:
    void updateGeometry();

    void at_opacityAnimator_finished();

private:
    QRect m_sceneRect;
    QRectF m_rect;
    QColor m_color;
    qreal m_targetOpacity;
    QWeakPointer<QnWorkbenchGridMapper> m_mapper;
    RectAnimator *m_geometryAnimator;
    VariantAnimator *m_opacityAnimator;
};


#endif // GRID_BACKGROUND_ITEM_H
