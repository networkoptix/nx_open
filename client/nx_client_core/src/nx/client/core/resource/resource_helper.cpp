#include "resource_helper.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx {
namespace client {
namespace core {

ResourceHelper::ResourceHelper(QObject* parent):
    base_type(parent),
    QnConnectionContextAware()
{
}

QString ResourceHelper::resourceId() const
{
    return m_resource ? m_resource->getId().toString() : QString();
}

void ResourceHelper::setResourceId(const QString& id)
{
    if (resourceId() == id)
        return;

    const auto uuid = QnUuid::fromStringSafe(id);
    const auto resource = resourcePool()->getResourceById<QnResource>(uuid);

    if (m_resource)
        m_resource->disconnect(this);

    m_resource = resource;

    if (m_resource)
    {
        connect(m_resource, &QnResource::nameChanged,
            this, &ResourceHelper::resourceNameChanged);
        connect(m_resource, &QnResource::statusChanged,
            this, &ResourceHelper::resourceStatusChanged);
    }

    emit resourceIdChanged();
    emit resourceNameChanged();
    emit resourceStatusChanged();
}

Qn::ResourceStatus ResourceHelper::resourceStatus() const
{
    return m_resource ? m_resource->getStatus() : Qn::NotDefined;
}

QString ResourceHelper::resourceName() const
{
    return m_resource ? m_resource->getName() : QString();
}

QnResourcePtr ResourceHelper::resource() const
{
    return m_resource;
}

} // namespace core
} // namespace client
} // namespace nx
