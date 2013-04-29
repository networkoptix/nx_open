#ifndef GRID_BACKGROUND_ITEM_H
#define GRID_BACKGROUND_ITEM_H

#include <memory>

#include <QObject>
#include <QGraphicsObject>

#include <camera/gl_renderer.h>

#include <ui/graphics/items/resource/decodedpicturetoopengluploader.h>

#include <utils/app_server_image_cache.h>

class QnWorkbenchGridMapper;

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

    QString imageFilename() const;
    void setImageFilename(const QString &imageFilename);

    QSize imageSize() const;
    void setImageSize(const QSize &imageSize);

    /** Image opacity value in range [0.0 .. 1.0] */
    qreal imageOpacity() const;
    void setImageOpacity(qreal value);

    QRect sceneBoundingRect() const;

    void updateDisplay();
protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private slots:
    void updateGeometry();

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
    qreal m_imageOpacity;
    QRect m_sceneBoundingRect;
    QWeakPointer<QnWorkbenchGridMapper> m_mapper;
    QnAppServerImageCache *m_cache;
    std::auto_ptr<DecodedPictureToOpenGLUploader> m_imgUploader;
    std::auto_ptr<QnGLRenderer> m_renderer;
    QSharedPointer<CLVideoDecoderOutput> m_imgAsFrame;
    bool m_imgUploaded;
    ImageStatus m_imageStatus;
    QHash<QString, QImage> m_imagesMemCache;
};


#endif // GRID_BACKGROUND_ITEM_H
