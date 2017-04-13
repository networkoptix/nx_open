#ifndef GRID_BACKGROUND_ITEM_H
#define GRID_BACKGROUND_ITEM_H

#include <memory>

#include <QtCore/QObject>
#include <QtWidgets/QGraphicsObject>

#include <camera/gl_renderer.h>
#include <core/resource/resource_fwd.h>

#include <ui/graphics/items/resource/decodedpicturetoopengluploader.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchGridMapper;
class QnGridBackgroundItemPrivate;
class ServerImageCache;

class QnGridBackgroundItem: public QGraphicsObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QGraphicsObject;

    Q_PROPERTY(QRectF viewportRect READ viewportRect WRITE setViewportRect)

public:
    explicit QnGridBackgroundItem(
        QGraphicsItem* parent = nullptr,
        QnWorkbenchContext* context = nullptr);

    virtual ~QnGridBackgroundItem();

    virtual QRectF boundingRect() const override;

    const QRectF& viewportRect() const;
    void setViewportRect(const QRectF& rect);

    QnWorkbenchGridMapper* mapper() const;
    void setMapper(QnWorkbenchGridMapper* mapper);

    void update(const QnLayoutResourcePtr& layout);

    QRect sceneBoundingRect() const;

protected:
    virtual void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

    QScopedPointer<QnGridBackgroundItemPrivate> const d_ptr;

private slots:
    void updateGeometry();
    void updateDisplay();
    void updateDefaultBackground();

    void at_imageLoaded(const QString& filename, bool ok);
    void setImage(const QImage& image);

private:
    ServerImageCache* cache();
    void updateConnectedState();

    Q_DECLARE_PRIVATE(QnGridBackgroundItem)
    QPointer<QnWorkbenchGridMapper> m_mapper;
    QScopedPointer<DecodedPictureToOpenGLUploader> m_imgUploader;
    QScopedPointer<QnGLRenderer> m_renderer;
    QSharedPointer<CLVideoDecoderOutput> m_imgAsFrame;
};


#endif // GRID_BACKGROUND_ITEM_H
