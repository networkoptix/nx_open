// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_layout_item_access_provider.h"

#include <core/resource_access/helpers/layout_item_aggregator.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <nx/vms/common/system_context.h>

namespace nx::core::access {

SharedLayoutItemAccessProvider::SharedLayoutItemAccessProvider(
    Mode mode,
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(mode, context, parent)
{
    if (mode == Mode::cached)
    {
        connect(m_context->sharedResourcesManager(),
            &QnSharedResourcesManager::sharedResourcesChanged,
            this,
            &SharedLayoutItemAccessProvider::handleSharedResourcesChanged);
    }
}

SharedLayoutItemAccessProvider::~SharedLayoutItemAccessProvider()
{
}

Source SharedLayoutItemAccessProvider::baseSource() const
{
    return Source::layout;
}

bool SharedLayoutItemAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    GlobalPermissions /*globalPermissions*/) const
{
    if (!isMediaResource(resource))
        return false;

    if (mode() == Mode::direct)
    {
        const auto resourcePool = m_context->resourcePool();
        const auto sharedLayouts = resourcePool->getResourcesByIds<QnLayoutResource>(
            m_context->sharedResourcesManager()->sharedResources(subject));

        const auto resourceId = resource->getId();
        for (const auto& layout: sharedLayouts)
        {
            // Layout may be not shared yet in the process of sharing
            if (!layout->isShared())
                continue;

            const auto layoutItems = layout->getItems();
            for (const auto& item: layoutItems)
            {
                if (item.resource.id == resourceId)
                    return true;
            }
        }
        return false;
    }

    NX_ASSERT(mode() == Mode::cached);

    // Method is called under the mutex.
    // Using effective ids as aggregators are created for users and roles separately.
    for (const auto& effectiveId:
        m_context->resourceAccessSubjectsCache()->subjectWithParents(subject))
    {
        QnLayoutItemAggregatorPtr aggregator = m_aggregatorsBySubject.value(effectiveId);

        if (!aggregator)
        {
            // We may got here if role is deleted while user is not or in case of predefined role.
            NX_ASSERT(subject.isUser()
                || QnPredefinedUserRoles::enumValue(effectiveId) != Qn::UserRole::customUserRole,
                "Subject %1 inherited by %2 does not have an aggregator", subject, effectiveId);

            aggregator = m_aggregatorsBySubject.value(subject.id());
        }

        NX_ASSERT(aggregator);
        if (aggregator && aggregator->hasItem(resource->getId()))
            return true;
    }

    return false;
}

void SharedLayoutItemAccessProvider::fillProviders(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    QnResourceList& providers) const
{
    if (!isMediaResource(resource))
        return;

    const auto sharedLayouts = m_context->resourcePool()->getResourcesByIds<QnLayoutResource>(
        m_context->sharedResourcesManager()->sharedResources(subject));

    const auto resourceId = resource->getId();
    for (const auto& layout: sharedLayouts)
    {
        NX_ASSERT(layout->isShared());
        if (!layout->isShared())
            continue;

        const auto layoutItems = layout->getItems();
        for (const auto& item: layoutItems)
        {
            if (item.resource.id != resourceId)
                continue;
            providers.append(layout);
            break;
        }
    }
}

void SharedLayoutItemAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    base_type::handleResourceAdded(resource);

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout.get(), &QnResource::parentIdChanged, this,
            [this, layout]
            {
                updateAccessToLayout(layout);
            });

        updateAccessToLayout(layout);
    }
}

void SharedLayoutItemAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    base_type::handleResourceRemoved(resource);
    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (!layout->isShared())
            return;

        QVector<QnLayoutItemAggregatorPtr> aggregators;

        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            for (const auto& aggregator: std::as_const(m_aggregatorsBySubject))
            {
                if (aggregator->hasLayout(layout))
                    aggregators.push_back(aggregator);
            }
        }

        for (auto aggregator: aggregators)
            aggregator->removeWatchedLayout(layout);
    }

}

void SharedLayoutItemAccessProvider::handleSubjectAdded(const QnResourceAccessSubject& subject)
{
    NX_ASSERT(mode() == Mode::cached);

    auto aggregator = ensureAggregatorForSubject(subject);

    const auto sharedLayouts = m_context->resourcePool()->getResourcesByIds<QnLayoutResource>(
        m_context->sharedResourcesManager()->sharedResources(subject));
    for (const auto& layout: sharedLayouts)
        aggregator->addWatchedLayout(layout);

    base_type::handleSubjectAdded(subject);
}

void SharedLayoutItemAccessProvider::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    NX_ASSERT(mode() == Mode::cached);

    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_aggregatorsBySubject.remove(subject.id());
    }
    base_type::handleSubjectRemoved(subject);
}

void SharedLayoutItemAccessProvider::handleSharedResourcesChanged(
    const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& oldValues,
    const QSet<QnUuid>& newValues)
{
    NX_ASSERT(mode() == Mode::cached);

    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return;

    auto aggregator = findAggregatorForSubject(subject);
    if (!aggregator)
        return;

    const auto added = (newValues - oldValues);
    const auto removed = (oldValues - newValues);

    const auto resourcePool = m_context->resourcePool();

    const auto addedLayouts = resourcePool->getResourcesByIds<QnLayoutResource>(added);
    for (const auto& layout: addedLayouts)
    {
        if (layout->isShared())
           aggregator->addWatchedLayout(layout);
    }

    const auto removedLayouts = resourcePool->getResourcesByIds<QnLayoutResource>(removed);
    for (const auto& layout: removedLayouts)
    {
        if (layout->isShared())
            aggregator->removeWatchedLayout(layout);
    }
}

void SharedLayoutItemAccessProvider::updateAccessToLayout(const QnLayoutResourcePtr& layout)
{
    NX_ASSERT(mode() == Mode::cached);

    if (!layout->isShared())
        return;

    const auto layoutId = layout->getId();
    const auto allSubjects = m_context->resourceAccessSubjectsCache()->allSubjects();
    for (const auto& subject: allSubjects)
    {
        const auto sharedResources = m_context->sharedResourcesManager()->sharedResources(subject);
        if (!sharedResources.contains(layoutId))
            continue;

        if (auto aggregator = findAggregatorForSubject(subject))
            aggregator->addWatchedLayout(layout);
    }
}

QnLayoutItemAggregatorPtr SharedLayoutItemAccessProvider::findAggregatorForSubject(
    const QnResourceAccessSubject& subject)
{
    NX_ASSERT(mode() == Mode::cached);

    auto id = subject.id();
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        const auto existing = m_aggregatorsBySubject.constFind(id);
        if (existing != m_aggregatorsBySubject.cend())
            return *existing;
    }

    return QnLayoutItemAggregatorPtr();
}

QnLayoutItemAggregatorPtr SharedLayoutItemAccessProvider::ensureAggregatorForSubject(
    const QnResourceAccessSubject& subject)
{
    NX_ASSERT(mode() == Mode::cached);

    auto id = subject.id();

    auto updateAccessToResourceBySubject =
        [this, subject](const QnUuid& resourceId)
        {
            if (isUpdating())
                return;

            auto resource = m_context->resourcePool()->getResourceById(resourceId);
            if (!resource || !isMediaResource(resource))
                return;

            updateAccess(subject, resource);
        };

    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        const auto existing = m_aggregatorsBySubject.constFind(id);
        if (existing != m_aggregatorsBySubject.cend())
            return *existing;

        QnLayoutItemAggregatorPtr aggregator(new QnLayoutItemAggregator());
        connect(aggregator.get(), &QnLayoutItemAggregator::itemAdded, this,
            updateAccessToResourceBySubject);
        connect(aggregator.get(), &QnLayoutItemAggregator::itemRemoved, this,
            updateAccessToResourceBySubject);
        m_aggregatorsBySubject.insert(id, aggregator);
        return aggregator;
    }
}

} // namespace nx::core::access
