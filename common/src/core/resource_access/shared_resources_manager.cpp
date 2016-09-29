#include "shared_resources_manager.h"

#include <nx_ec/data/api_access_rights_data.h>

QnSharedResourcesManager::QnSharedResourcesManager(QObject* parent):
    base_type(parent),
    m_mutex(QnMutex::NonRecursive),
    m_sharedResources()
{

}

QnSharedResourcesManager::~QnSharedResourcesManager()
{
}

void QnSharedResourcesManager::reset(const ec2::ApiAccessRightsDataList& accessibleResourcesList)
{
    QnMutexLocker lk(&m_mutex);
    m_sharedResources.clear();
    for (const auto& item : accessibleResourcesList)
    {
        QSet<QnUuid>& resources = m_sharedResources[item.userId];
        for (const auto& id : item.resourceIds)
            resources << id;
    }
    //TODO: #GDM possibly we should send correct signals, shouldn't we?
}

QSet<QnUuid> QnSharedResourcesManager::sharedResources(
    const QnResourceAccessSubject& subject) const
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return QSet<QnUuid>();

    QnMutexLocker lk(&m_mutex);
    return m_sharedResources[subject.effectiveId()];
}

void QnSharedResourcesManager::setSharedResources(const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& resources)
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return;

    NX_ASSERT(subject.effectiveId() == subject.id() || resources.empty(),
        "Security check. We must not set custom accessible resources to user in custom role."
    );

    auto id = subject.id();
    {
        QnMutexLocker lk(&m_mutex);
        if (m_sharedResources[id] == resources)
            return;

        m_sharedResources[id] = resources;
    }

    emit sharedResourcesChanged(subject, resources);
}

