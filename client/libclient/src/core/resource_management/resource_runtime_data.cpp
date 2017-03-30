#include "resource_runtime_data.h"

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

QnResourceRuntimeDataManager::QnResourceRuntimeDataManager(QObject* parent):
    base_type(parent)
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            m_data.remove(resource->getId());
            if (auto layout = resource.dynamicCast<QnLayoutResource>())
            {
                for (auto item: layout->getItems())
                    m_data.remove(item.uuid);
            }
        });
}

QVariant QnResourceRuntimeDataManager::resourceData(const QnResourcePtr& resource, Qn::ItemDataRole role) const
{
    NX_ASSERT(resource);
    if (!resource)
        return QVariant();
    return m_data.value(resource->getId()).value(role);
}

void QnResourceRuntimeDataManager::setResourceData(
    const QnResourcePtr& resource,
    Qn::ItemDataRole role,
    const QVariant& data)
{
    NX_ASSERT(resource);
    if (!resource)
        return;
    setDataInternal(resource->getId(), role, data);
}

QVariant QnResourceRuntimeDataManager::layoutItemData(const QnUuid& id, Qn::ItemDataRole role) const
{
    return m_data.value(id).value(role);
}

void QnResourceRuntimeDataManager::setLayoutItemData(
    const QnUuid& id,
    Qn::ItemDataRole role,
    const QVariant& data)
{
    setDataInternal(id, role, data);
}

void QnResourceRuntimeDataManager::setDataInternal(
    const QnUuid& id,
    Qn::ItemDataRole role,
    const QVariant& data)
{
    const QVariant& oldData = m_data.value(id).value(role);
    if (oldData == data)
        return;

    if (data.isValid())
    {
        m_data[id][role] = data;
    }
    else
    {
        if (!m_data.contains(id))
            return;

        m_data[id].remove(role);
        if (m_data[id].isEmpty())
            m_data.remove(id);
    }
    emit layoutItemDataChanged(id, role, data);
}
