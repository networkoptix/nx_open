#pragma once

#include <QtCore/QObject>
#include <QtCore/QRectF>

#include <core/resource/resource_fwd.h>

class QImage;

namespace nx {
namespace client {
namespace desktop {

class EntropixImageEnhancer: public QObject
{
    Q_OBJECT

    using base_type = QObject;

public:
    EntropixImageEnhancer(
        const QnVirtualCameraResourcePtr& camera,
        QObject* parent = nullptr);
    ~EntropixImageEnhancer();

    void requestScreenshot(qint64 timestamp, const QRectF& zoomRect = QRectF());
    void cancelRequest();
    int progress() const;

signals:
    void cameraScreenshotReady(const QImage& image);
    void progressChanged(int progress);

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace desktop
} // namespace client
} // namespace nx
