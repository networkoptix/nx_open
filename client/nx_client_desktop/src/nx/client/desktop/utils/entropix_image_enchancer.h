#pragma once

#include <QtCore/QObject>
#include <QtCore/QRectF>

#include <core/resource/resource_fwd.h>

class QImage;

namespace nx {
namespace client {
namespace desktop {

class EntropixImageEnchancer: public QObject
{
    Q_OBJECT

    using base_type = QObject;

public:
    EntropixImageEnchancer(
        const QnVirtualCameraResourcePtr& camera,
        QObject* parent = nullptr);
    ~EntropixImageEnchancer();

    void requestScreenshot(qint64 timestamp, const QRectF& zoomRect = QRectF());
    void cancelRequest();

signals:
    void cameraScreenshotReady(const QImage& image);

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace desktop
} // namespace client
} // namespace nx
