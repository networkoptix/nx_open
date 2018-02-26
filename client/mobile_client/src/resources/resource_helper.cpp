#include "resource_helper.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

QnResourceHelper::QnResourceHelper(QObject* parent):
    base_type(parent),
    QnConnectionContextAware()
{
}

QString QnResourceHelper::resourceId() const
{
    return m_resource ? m_resource->getId().toString() : QString();
}

void QnResourceHelper::setResourceId(const QString& id)
{
    if (resourceId() == id)
        return;

    const auto uuid = QnUuid::fromStringSafe(id);
    const auto resource = resourcePool()->getResourceById<QnResource>(uuid);

    if (m_resource)
        m_resource->disconnect(this);

    m_resource = resource;

    connect(m_resource, &QnResource::nameChanged,
        this, &QnResourceHelper::resourceNameChanged);
    connect(m_resource, &QnResource::statusChanged,
        this, &QnResourceHelper::resourceStatusChanged);

    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
    {
        connect(camera, &QnSecurityCamResource::capabilitiesChanged,
            this, &QnResourceHelper::defaultCameraPasswordChanged);
    }

    emit resourceIdChanged();
    emit resourceNameChanged();
    emit resourceStatusChanged();
    emit defaultCameraPasswordChanged();
}

Qn::ResourceStatus QnResourceHelper::resourceStatus() const
{
    return m_resource ? m_resource->getStatus() : Qn::NotDefined;
}

QString QnResourceHelper::resourceName() const
{
    return m_resource ? m_resource->getName() : QString();
}

QnResourcePtr QnResourceHelper::resource() const
{
    return m_resource;
}

bool QnResourceHelper::isDefaultCameraPassword() const
{
    if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
        return camera->hasCameraCapabilities(Qn::isDefaultPasswordCapability);

    return false;
}
