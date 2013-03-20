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
    Q_PROPERTY(QRectF viewportRect READ viewportRect WRITE setViewportRect)

public:
    explicit QnGridBackgroundItem(QGraphicsItem *parent = NULL);
    virtual ~QnGridBackgroundItem();

    virtual QRectF boundingRect() const override;

    const QRectF &viewportRect() const;
    void setViewportRect(const QRectF &rect);

    QnWorkbenchGridMapper *mapper() const;
    void setMapper(QnWorkbenchGridMapper *mapper);

    AnimationTimer* animationTimer() const;
    void setAnimationTimer(AnimationTimer *timer);

    QImage image() const;
    void setImage(const QImage &image);

    QSize imageSize() const;
    void setImageSize(const QSize &imageSize);

    QRect sceneBoundingRect() const;

    void animatedShow();
    void animatedHide();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private slots:
    void updateGeometry();

    void at_opacityAnimator_finished();

private:
    QRectF m_rect;
    QImage m_image;
    QSize m_imageSize;
    qreal m_targetOpacity;
    QRect m_sceneBoundingRect;
    QWeakPointer<QnWorkbenchGridMapper> m_mapper;
    RectAnimator *m_geometryAnimator;
    VariantAnimator *m_opacityAnimator;
};


#endif // GRID_BACKGROUND_ITEM_H
