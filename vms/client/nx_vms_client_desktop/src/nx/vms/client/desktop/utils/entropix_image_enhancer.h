#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QRectF>

#include <core/resource/resource_fwd.h>

class QImage;

namespace nx::vms::client::desktop {

class EntropixImageEnhancer: public QObject
{
    Q_OBJECT

    using base_type = QObject;

public:
    EntropixImageEnhancer(
        const QnVirtualCameraResourcePtr& camera,
        QObject* parent = nullptr);
    virtual ~EntropixImageEnhancer() override;

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

} // namespace nx::vms::client::desktop
