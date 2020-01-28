#include "object_coordinates_translator.h"

#include <QtCore/QObject>

#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>

namespace nx::vms::server::analytics {

static int getRotation(const QnVirtualCameraResourcePtr& device)
{
    return device->getProperty(QnMediaResource::rotationKey()).toInt();
}

ObjectCoordinatesTranslator::ObjectCoordinatesTranslator(QnVirtualCameraResourcePtr device):
    m_device(std::move(device))
{
    connect(
        m_device.get(), &QnResource::propertyChanged,
        this, &ObjectCoordinatesTranslator::at_propertyChanged);

    m_rotationAngleDegrees.store(getRotation(m_device));
}

QRectF ObjectCoordinatesTranslator::translate(const QRectF& originalBoundingBox) const
{
    NX_VERBOSE(this,
        "Translating bounding box coordinates, rotation angle: %1 degrees, "
        "original bounding box: %2, device %3 (%4)",
        m_rotationAngleDegrees.load(), originalBoundingBox,
        m_device->getUserDefinedName(), m_device->getId());

    switch (m_rotationAngleDegrees.load())
    {
        case 0:
        {
            return originalBoundingBox;
        }
        case 90:
        {
            const QPointF pointToTranslate = originalBoundingBox.topRight();
            return QRectF(
                QPointF(pointToTranslate.y(), 1 - pointToTranslate.x()),
                QSizeF(originalBoundingBox.height(), originalBoundingBox.width()));
        }
        case 180:
        {
            const QPointF pointToTranslate = originalBoundingBox.bottomRight();
            return QRectF(
                QPointF(1 - pointToTranslate.x(), 1 - pointToTranslate.y()),
                QSizeF(originalBoundingBox.width(), originalBoundingBox.height()));
        }
        case 270:
        {
            const QPointF pointToTranslate = originalBoundingBox.bottomLeft();
            return QRectF(
                QPointF(1 - pointToTranslate.y(), pointToTranslate.x()),
                QSizeF(originalBoundingBox.height(), originalBoundingBox.width()));
        }
        default:
        {
            NX_WARNING(this,
                "Got invalid rotation angle: %1 degrees (allowed rotation angles are 0, 90, 180 "
                "and 270 degrees), device %2 (%3)",
                m_rotationAngleDegrees.load(), m_device->getUserDefinedName(), m_device->getId());

            return QRectF();
        }
    }
}

void ObjectCoordinatesTranslator::at_propertyChanged(
    const QnResourcePtr& device, const QString& propertyName)
{
    if (propertyName == QnMediaResource::rotationKey())
    {
        NX_DEBUG(this, "Rotation has been changed, new rotataion is %1 degrees, device %2 (%3)",
            getRotation(m_device), m_device->getUserDefinedName(), m_device->getId());

        m_rotationAngleDegrees.store(getRotation(m_device));
    }
}

} // namespace nx::vms::server::analytics
