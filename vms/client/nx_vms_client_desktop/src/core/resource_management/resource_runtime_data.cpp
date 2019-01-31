#include "resource_runtime_data.h"

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

QnResourceRuntimeDataManager::QnResourceRuntimeDataManager(QnCommonModule* commonModule, QObject* parent):
    base_type(parent),
    QnCommonModuleAware(commonModule)
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
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

    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_data.value(resource->getId()).value(role);
}

void QnResourceRuntimeDataManager::setResourceData(
    const QnResourcePtr& resource,
    Qn::ItemDataRole role,
    const QVariant& data)
{
    NX_ASSERT(resource && !resource->getId().isNull());
    if (!resource)
        return;

    Qn::Notifier notify;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        notify = setDataInternal(resource->getId(), role, data);
    }
    if (notify)
        notify();
}

void QnResourceRuntimeDataManager::cleanupResourceData(const QnResourcePtr& resource, Qn::ItemDataRole role)
{
    NX_ASSERT(resource && !resource->getId().isNull());
    if (!resource)
        return;

    cleanupData(resource->getId(), role);
}

void QnResourceRuntimeDataManager::cleanupResourceData(const QnResourcePtr& resource)
{
    NX_ASSERT(resource && !resource->getId().isNull());
    if (!resource)
        return;

    cleanupData(resource->getId());
}

QVariant QnResourceRuntimeDataManager::layoutItemData(const QnUuid& id, Qn::ItemDataRole role) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_data.value(id).value(role);
}

void QnResourceRuntimeDataManager::setLayoutItemData(
    const QnUuid& id,
    Qn::ItemDataRole role,
    const QVariant& data)
{
    Qn::Notifier notify;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        notify = setDataInternal(id, role, data);
    }
    if (notify)
        notify();
}

void QnResourceRuntimeDataManager::cleanupData(const QnUuid& id, Qn::ItemDataRole role)
{
    Qn::Notifier notify;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        notify = setDataInternal(id, role, QVariant());
    }
    if (notify)
        notify();
}

void QnResourceRuntimeDataManager::cleanupData(const QnUuid& id)
{
    Qn::NotifierList notifiers;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        for (auto role: m_data.value(id).keys())
        {
            auto notify = setDataInternal(id, role, QVariant());
            if (notify)
                notifiers.push_back(notify);
        }
    }
    for (const auto& notify: notifiers)
        notify();
}

Qn::Notifier QnResourceRuntimeDataManager::setDataInternal(
    const QnUuid& id,
    Qn::ItemDataRole role,
    const QVariant& data)
{
    const QVariant& oldData = m_data.value(id).value(role);
    if (data.isValid() && oldData == data)
        return nullptr;

    if (data.isValid())
    {
        m_data[id][role] = data;
    }
    else
    {
        if (!m_data.contains(id))
            return nullptr;

        if (!m_data[id].contains(role))
            return nullptr;

        m_data[id].remove(role);
        if (m_data[id].isEmpty())
            m_data.remove(id);
    }

    return
        [this, id, role, data]()
        {
            emit resourceDataChanged(id, role, data);
            emit layoutItemDataChanged(id, role, data);
        };
}
