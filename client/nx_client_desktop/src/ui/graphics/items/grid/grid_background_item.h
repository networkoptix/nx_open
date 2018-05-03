#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtWidgets/QGraphicsObject>

#include <camera/gl_renderer.h>
#include <core/resource/resource_fwd.h>

#include <ui/customization/customized.h>
#include <ui/graphics/items/resource/decodedpicturetoopengluploader.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchGridMapper;
class QnGridBackgroundItemPrivate;

namespace nx {
namespace client {
namespace desktop {

class ServerImageCache;

} // namespace desktop
} // namespace client
} // namespace nx


class QnGridBackgroundItem: public Customized<QGraphicsObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Customized<QGraphicsObject>;

    Q_PROPERTY(QRectF viewportRect READ viewportRect WRITE setViewportRect)
    Q_PROPERTY(QColor panelColor READ panelColor WRITE setPanelColor)

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

    QColor panelColor() const;
    void setPanelColor(const QColor& color);

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
    void setImage(const QImage& image, const QString& filename = QString());

private:
    nx::client::desktop::ServerImageCache* cache();
    void updateConnectedState();

    Q_DECLARE_PRIVATE(QnGridBackgroundItem)
    QPointer<QnWorkbenchGridMapper> m_mapper;
    QScopedPointer<DecodedPictureToOpenGLUploader> m_imgUploader;
    QScopedPointer<QnGLRenderer> m_renderer;
    QSharedPointer<CLVideoDecoderOutput> m_imgAsFrame;
    QColor m_panelColor;
};
