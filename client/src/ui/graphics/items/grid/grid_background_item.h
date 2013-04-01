#ifndef GRID_BACKGROUND_ITEM_H
#define GRID_BACKGROUND_ITEM_H

#include <QObject>
#include <QGraphicsObject>

#include <utils/app_server_file_cache.h>

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

    int imageId() const;
    void setImageId(int imageId);

    QSize imageSize() const;
    void setImageSize(const QSize &imageSize);

    QRect sceneBoundingRect() const;

    void showWhenReady();
    void animatedHide();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private slots:
    void animatedShow();
    void updateGeometry();

    void at_opacityAnimator_finished();
    void at_image_loaded(int id, const QImage& image);

private:
    QRectF m_rect;
    QImage m_image;
    int m_imageId;
    QSize m_imageSize;
    qreal m_targetOpacity;
    QRect m_sceneBoundingRect;
    QWeakPointer<QnWorkbenchGridMapper> m_mapper;
    RectAnimator *m_geometryAnimator;
    VariantAnimator *m_opacityAnimator;
    QnAppServerFileCache *m_cache;
};


#endif // GRID_BACKGROUND_ITEM_H
