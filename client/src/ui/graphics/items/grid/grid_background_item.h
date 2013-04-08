#ifndef GRID_BACKGROUND_ITEM_H
#define GRID_BACKGROUND_ITEM_H

#include <memory>

#include <QObject>
#include <QGraphicsObject>

#include <utils/app_server_file_cache.h>

#include "../resource/decodedpicturetoopengluploader.h"
#include "../../../../camera/gl_renderer.h"


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

    QString imageFilename() const;
    void setImageFilename(const QString &imageFilename);

    QSize imageSize() const;
    void setImageSize(const QSize &imageSize);

    int imageOpacity() const;
    void setImageOpacity(int percent);

    QRect sceneBoundingRect() const;

    void updateDisplay();
    void animatedHide();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private slots:
    void animatedShow();
    void updateGeometry();

    void at_opacityAnimator_finished();
    void at_imageLoaded(const QString& filename, bool ok);
    void setImage(const QImage& image);

private:
    enum ImageStatus {
        None,
        Loading,
        Loaded
    };

    QRectF m_rect;
    QImage m_image;
    QString m_imageFilename;
    QSize m_imageSize;
    int m_imageOpacity;
    qreal m_targetOpacity;
    QRect m_sceneBoundingRect;
    QWeakPointer<QnWorkbenchGridMapper> m_mapper;
    RectAnimator *m_geometryAnimator;
    VariantAnimator *m_opacityAnimator;
    QnAppServerFileCache *m_cache;
    std::auto_ptr<DecodedPictureToOpenGLUploader> m_imgUploader;
    std::auto_ptr<QnGLRenderer> m_renderer;
    QSharedPointer<CLVideoDecoderOutput> m_imgAsFrame;
    bool m_imgUploaded;
    ImageStatus m_imageStatus;
};


#endif // GRID_BACKGROUND_ITEM_H
