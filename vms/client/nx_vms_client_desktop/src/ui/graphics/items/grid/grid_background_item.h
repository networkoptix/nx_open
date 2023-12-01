// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtWidgets/QGraphicsObject>

#include <camera/gl_renderer.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <ui/graphics/items/resource/decodedpicturetoopengluploader.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchGridMapper;
class QnGridBackgroundItemPrivate;

namespace nx::vms::client::desktop {

class ServerImageCache;

} // namespace nx::vms::client::desktop


class QnGridBackgroundItem: public QGraphicsObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QGraphicsObject;

    Q_PROPERTY(QRectF viewportRect READ viewportRect WRITE setViewportRect)

public:
    explicit QnGridBackgroundItem(
        QGraphicsScene* scene,
        QnWorkbenchContext* context = nullptr);

    virtual ~QnGridBackgroundItem();

    virtual QRectF boundingRect() const override;

    const QRectF& viewportRect() const;
    void setViewportRect(const QRectF& rect);

    QnWorkbenchGridMapper* mapper() const;
    void setMapper(QnWorkbenchGridMapper* mapper);

    void update(const nx::vms::client::desktop::LayoutResourcePtr& layout);

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
    void setImage(const QImage& image, const QString& filename = QString());

private:
    nx::vms::client::desktop::ServerImageCache* cache();
    void updateConnectedState();

    Q_DECLARE_PRIVATE(QnGridBackgroundItem)
    QPointer<QnWorkbenchGridMapper> m_mapper;
    QScopedPointer<DecodedPictureToOpenGLUploader> m_imgUploader;
    QScopedPointer<QnGLRenderer> m_renderer;
    QSharedPointer<CLVideoDecoderOutput> m_imgAsFrame;
    QColor m_panelColor;
};
