#include "shared_layout_item_access_provider.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource_access/helpers/layout_item_aggregator.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_access/shared_resources_manager.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <common/common_module.h>

QnSharedLayoutItemAccessProvider::QnSharedLayoutItemAccessProvider(QObject* parent):
    base_type(parent)
{
    connect(sharedResourcesManager(), &QnSharedResourcesManager::sharedResourcesChanged, this,
        &QnSharedLayoutItemAccessProvider::handleSharedResourcesChanged);
}

QnSharedLayoutItemAccessProvider::~QnSharedLayoutItemAccessProvider()
{

}

QnAbstractResourceAccessProvider::Source QnSharedLayoutItemAccessProvider::baseSource() const
{
    return Source::layout;
}

bool QnSharedLayoutItemAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!isMediaResource(resource))
        return false;

    // Method is called under the mutex.
    // Using effective id as aggregators are created for users and roles separately
    QnLayoutItemAggregatorPtr aggregator = m_aggregatorsBySubject.value(subject.effectiveId());

    if (!aggregator)
    {
        // We may got here if role is deleted while user is not
        NX_EXPECT(subject.isUser() && subject.user()->userRole() == Qn::UserRole::CustomUserRole);
        aggregator = m_aggregatorsBySubject.value(subject.id());
    }

    NX_ASSERT(aggregator);
    if (aggregator)
        return aggregator->hasItem(resource->getId());

    return false;
}

void QnSharedLayoutItemAccessProvider::fillProviders(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    QnResourceList& providers) const
{
    if (!isMediaResource(resource))
        return;

    auto sharedLayouts = commonModule()->resourcePool()->getResources<QnLayoutResource>(
        sharedResourcesManager()->sharedResources(subject));

    auto resourceId = resource->getId();
    for (const auto& layout : sharedLayouts)
    {
        NX_ASSERT(layout->isShared());
        if (!layout->isShared())
            continue;

        for (const auto& item : layout->getItems())
        {
            if (item.resource.id != resourceId)
                continue;
            providers << layout;
            break;
        }
    }
}

void QnSharedLayoutItemAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    base_type::handleResourceAdded(resource);

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout, &QnResource::parentIdChanged, this,
            [this, layout]
            {
                updateAccessToLayout(layout);
            });

        updateAccessToLayout(layout);
    }
}

void QnSharedLayoutItemAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    base_type::handleResourceRemoved(resource);
    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (!layout->isShared())
            return;

        QList<QnLayoutItemAggregatorPtr> aggregators;

        {
            QnMutexLocker lk(&m_mutex);
            for (const auto& aggregator: m_aggregatorsBySubject)
            {
                if (aggregator->hasLayout(layout))
                    aggregators.push_back(aggregator);
            }
        }

        for (auto aggregator: aggregators)
            aggregator->removeWatchedLayout(layout);
    }

}

void QnSharedLayoutItemAccessProvider::handleSubjectAdded(const QnResourceAccessSubject& subject)
{
    auto aggregator = ensureAggregatorForSubject(subject);

    auto sharedLayouts = commonModule()->resourcePool()->getResources<QnLayoutResource>(
        sharedResourcesManager()->sharedResources(subject));
    for (auto layout : sharedLayouts)
        aggregator->addWatchedLayout(layout);

    base_type::handleSubjectAdded(subject);
}

void QnSharedLayoutItemAccessProvider::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    {
        QnMutexLocker lk(&m_mutex);
        m_aggregatorsBySubject.remove(subject.id());
    }
    base_type::handleSubjectRemoved(subject);
}

void QnSharedLayoutItemAccessProvider::handleSharedResourcesChanged(
    const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& oldValues,
    const QSet<QnUuid>& newValues)
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return;

    auto aggregator = ensureAggregatorForSubject(subject);

    auto added = (newValues - oldValues);
    auto removed = (oldValues - newValues);

    for (const auto& layout: commonModule()->resourcePool()->getResources<QnLayoutResource>(added))
    {
        if (layout->isShared())
           aggregator->addWatchedLayout(layout);
    }

    for (const auto& layout: commonModule()->resourcePool()->getResources<QnLayoutResource>(removed))
    {
        if (layout->isShared())
            aggregator->removeWatchedLayout(layout);
    }
}

void QnSharedLayoutItemAccessProvider::updateAccessToLayout(const QnLayoutResourcePtr& layout)
{
    if (!layout->isShared())
        return;

    const auto layoutId = layout->getId();
    for (const auto& subject : resourceAccessSubjectsCache()->allSubjects())
    {
        auto shared = sharedResourcesManager()->sharedResources(subject);
        if (!shared.contains(layoutId))
            continue;

        ensureAggregatorForSubject(subject)->addWatchedLayout(layout);
    }
}

QnLayoutItemAggregatorPtr QnSharedLayoutItemAccessProvider::ensureAggregatorForSubject(
    const QnResourceAccessSubject& subject)
{
    auto id = subject.id();

    auto updateAccessToResourceBySubject = [this, subject](const QnUuid& resourceId)
        {
            if (isUpdating())
                return;

            auto resource = commonModule()->resourcePool()->getResourceById(resourceId);
            if (!resource || !isMediaResource(resource))
                return;

            updateAccess(subject, resource);
        };

    {
        QnMutexLocker lk(&m_mutex);
        const auto existing = m_aggregatorsBySubject.constFind(id);
        if (existing != m_aggregatorsBySubject.cend())
            return *existing;

        QnLayoutItemAggregatorPtr aggregator(new QnLayoutItemAggregator());
        connect(aggregator, &QnLayoutItemAggregator::itemAdded, this,
            updateAccessToResourceBySubject);
        connect(aggregator, &QnLayoutItemAggregator::itemRemoved, this,
            updateAccessToResourceBySubject);
        m_aggregatorsBySubject.insert(id, aggregator);
        return aggregator;
    }
}

