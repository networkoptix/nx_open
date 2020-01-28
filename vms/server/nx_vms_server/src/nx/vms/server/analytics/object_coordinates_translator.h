#pragma once

#include <atomic>

#include <QtCore/QRectF>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

namespace nx::vms::server::analytics {

/**
 * Translates coordinates of objects detected by an analytics plugin on a rotated frame to the
 * coordinate system bound to the corresponding unrotated frame.
 */
class ObjectCoordinatesTranslator: public Connective<QObject>
{
    Q_OBJECT
public:
    ObjectCoordinatesTranslator(QnVirtualCameraResourcePtr device);

    QRectF translate(const QRectF& originalBoundingBox) const;

private:
    void at_propertyChanged(const QnResourcePtr& device, const QString& propertyName);

private:
    QnVirtualCameraResourcePtr m_device;
    std::atomic<int> m_rotationAngleDegrees{0};
};

} // namespace nx::vms::server::analytics
