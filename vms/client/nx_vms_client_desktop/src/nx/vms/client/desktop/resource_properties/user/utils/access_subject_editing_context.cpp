// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_subject_editing_context.h"

#include <memory>

#include <QtCore/QAbstractItemModel>

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/access_rights_resolver.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <nx/vms/common/system_context.h>

#include "proxy_access_rights_manager.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::api;
using namespace nx::vms::common;
using namespace nx::core::access;

// ------------------------------------------------------------------------------------------------
// AccessSubjectEditingContext::Private

class AccessSubjectEditingContext::Private: public QObject
{
    AccessSubjectEditingContext* const q;

public:
    class Hierarchy: public SubjectHierarchy
    {
    public:
        using SubjectHierarchy::SubjectHierarchy;
        using SubjectHierarchy::addOrUpdate;
        using SubjectHierarchy::remove;
        using SubjectHierarchy::operator=;
    };

public:
    QnUuid currentSubjectId;
    nx::vms::common::SystemContext* systemContext;
    const QPointer<SubjectHierarchy> systemSubjectHierarchy;
    const std::unique_ptr<Hierarchy> currentHierarchy;
    const std::unique_ptr<ProxyAccessRightsManager> accessRightsManager;
    const std::unique_ptr<AccessRightsResolver> accessRightsResolver;
    const std::unique_ptr<AccessRightsResolver::Notifier> notifier;
    bool hierarchyChanged = false;
    mutable QSet<QnResourcePtr> currentlyAccessibleResourceCache; //< For filter by permissions.

public:
    explicit Private(
        AccessSubjectEditingContext* q,
        nx::vms::common::SystemContext* systemContext)
        :
        q(q),
        systemContext(systemContext),
        systemSubjectHierarchy(systemContext->accessSubjectHierarchy()),
        currentHierarchy(new Hierarchy()),
        accessRightsManager(new ProxyAccessRightsManager(systemContext->accessRightsManager())),
        accessRightsResolver(new AccessRightsResolver(
            systemContext->resourcePool(),
            accessRightsManager.get(),
            systemContext->globalPermissionsWatcher(),
            currentHierarchy.get(),
            AccessRightsResolver::Mode::editing)),
        notifier(accessRightsResolver->createNotifier())
    {
        connect(notifier.get(), &AccessRightsResolver::Notifier::resourceAccessChanged, this,
            [this]()
            {
                emit this->q->resourceAccessChanged();
                emit this->q->accessibleResourcesFilterChanged();
            });

        connect(accessRightsResolver.get(), &AccessRightsResolver::resourceAccessReset,
            [this]()
            {
                emit this->q->resourceAccessChanged();
                this->q->resetAccessibleResourcesFilter();
            });

        connect(systemSubjectHierarchy, &SubjectHierarchy::reset,
            q, &AccessSubjectEditingContext::resetRelations);

        connect(accessRightsManager.get(), &ProxyAccessRightsManager::ownAccessRightsChanged,
            q, &AccessSubjectEditingContext::resourceAccessChanged);

        connect(systemSubjectHierarchy, &SubjectHierarchy::changed,
            this, &Private::handleSystemHierarchyChanged);

        connect(currentHierarchy.get(), &SubjectHierarchy::changed,
            q, &AccessSubjectEditingContext::hierarchyChanged);

        connect(currentHierarchy.get(), &SubjectHierarchy::reset,
            q, &AccessSubjectEditingContext::hierarchyChanged);

        connect(systemContext->resourcePool(), &QnResourcePool::resourcesAdded,
            this, &Private::handleResourcesAddedOrRemoved);

        connect(systemContext->resourcePool(), &QnResourcePool::resourcesRemoved,
            this, &Private::handleResourcesAddedOrRemoved);
    }

    void handleResourcesAddedOrRemoved(const QnResourceList& resources)
    {
        QSet<QnUuid> affectedGroupIds;

        for (const auto& resource: resources)
        {
            const auto id = specialResourceGroupFor(resource);
            if (!id.isNull())
                affectedGroupIds.insert(id);
        }

        if (!affectedGroupIds.empty())
            emit q->resourceGroupsChanged(affectedGroupIds);
    };

    QnResourceList getGroupResources(const QnUuid& groupId) const
    {
        const auto group = specialResourceGroup(groupId);
        if (!NX_ASSERT(group))
            return {};

        const auto resourcePool = systemContext->resourcePool();
        switch (*group)
        {
            case SpecialResourceGroup::allDevices:
                return resourcePool->getAllCameras(QnUuid{}, true);

            case SpecialResourceGroup::allServers:
                return resourcePool->servers();

            case SpecialResourceGroup::allWebPages:
                return resourcePool->getResources<QnWebPageResource>();

            case SpecialResourceGroup::allVideowalls:
                return resourcePool->getResources<QnVideoWallResource>();
        }

        NX_ASSERT(false, "Unhandled special resource group type");
        return {};
    }

    void handleSystemHierarchyChanged(
        const QSet<QnUuid>& added,
        const QSet<QnUuid>& removed,
        const QSet<QnUuid>& /*groupsWithChangedMembers*/,
        const QSet<QnUuid>& subjectsWithChangedParents)
    {
        if (removed.contains(currentSubjectId))
        {
            q->setCurrentSubjectId({});
            emit q->currentSubjectRemoved();
        }
        else
        {
            for (const auto& id: (subjectsWithChangedParents + added))
            {
                // Make sure that edited current subject parents are not changed.
                if (hierarchyChanged && id == currentSubjectId)
                    continue;

                currentHierarchy->addOrUpdate(id, systemSubjectHierarchy->directParents(id));
            }

            currentHierarchy->remove(removed);
        }
    }
};

// ------------------------------------------------------------------------------------------------
// AccessSubjectEditingContext

AccessSubjectEditingContext::AccessSubjectEditingContext(
    nx::vms::common::SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    d(new Private(this, systemContext))
{
}

AccessSubjectEditingContext::~AccessSubjectEditingContext()
{
    // Required here for forward-declared scoped pointer destruction.
}

nx::vms::common::SystemContext* AccessSubjectEditingContext::systemContext() const
{
    return d->systemContext;
}

QnUuid AccessSubjectEditingContext::currentSubjectId() const
{
    return d->currentSubjectId;
}

void AccessSubjectEditingContext::setCurrentSubjectId(const QnUuid& value)
{
    if (d->currentSubjectId == value)
        return;

    NX_DEBUG(this, "Changing current subject to %1", value);

    d->currentSubjectId = value;
    d->accessRightsManager->setCurrentSubjectId(value);
    d->notifier->setSubjectId(value);
    resetRelations();
    resetAccessibleResourcesFilter();

    emit subjectChanged();
    emit resourceAccessChanged();
}

ResourceAccessMap AccessSubjectEditingContext::ownResourceAccessMap() const
{
    if (!d->currentSubjectId.isNull())
        return d->accessRightsManager->ownResourceAccessMap(d->currentSubjectId);

    return {};
}

bool AccessSubjectEditingContext::hasOwnAccessRight(
    const QnUuid& resourceOrGroupId,
    nx::vms::api::AccessRight accessRight) const
{
    return ownResourceAccessMap().value(resourceOrGroupId, {}).testFlag(accessRight);
}

void AccessSubjectEditingContext::setOwnResourceAccessMap(
    const ResourceAccessMap& resourceAccessMap)
{
    NX_DEBUG(this, "Setting access map to %1", resourceAccessMap);
    d->accessRightsManager->setOwnResourceAccessMap(resourceAccessMap);
}

nx::vms::api::GlobalPermissions AccessSubjectEditingContext::globalPermissions() const
{
    return d->accessRightsResolver->globalPermissions(d->currentSubjectId);
}

QSet<QnUuid> AccessSubjectEditingContext::globalPermissionSource(
    nx::vms::api::GlobalPermission perm) const
{
    QSet<QnUuid> result;

    const auto directParents = subjectHierarchy()->directParents(d->currentSubjectId);

    for (const auto& parentId: directParents)
    {
        if (d->accessRightsResolver->globalPermissions(parentId).testFlag(perm))
            result << parentId;
    }

    return result;
}

void AccessSubjectEditingContext::resetOwnResourceAccessMap()
{
    NX_DEBUG(this, "Reverting access map changes");
    d->accessRightsManager->resetOwnResourceAccessMap();
}

AccessRights AccessSubjectEditingContext::accessRights(const QnResourcePtr& resource) const
{
    return d->currentSubjectId.isNull()
        ? AccessRights{}
        : d->accessRightsResolver->accessRights(d->currentSubjectId, resource->toSharedPointer());
}

ResourceAccessDetails AccessSubjectEditingContext::accessDetails(
    const QnResourcePtr& resource, AccessRight accessRight) const
{
    return d->currentSubjectId.isNull()
        ? ResourceAccessDetails{}
        : d->accessRightsResolver->accessDetails(d->currentSubjectId, resource, accessRight);
}

nx::vms::common::ResourceFilter AccessSubjectEditingContext::accessibleResourcesFilter() const
{
    const auto resourceFilter =
        [this, guard = QPointer(d.get())](const QnResourcePtr& resource) -> bool
        {
            if (!guard || !resource)
                return false;

            if (!d->currentlyAccessibleResourceCache.contains(resource) && !accessRights(resource))
                return false;

            d->currentlyAccessibleResourceCache.insert(resource);
            return true;
        };

    return resourceFilter;
}

void AccessSubjectEditingContext::resetAccessibleResourcesFilter()
{
    d->currentlyAccessibleResourceCache.clear();
    emit accessibleResourcesFilterChanged();
}

AccessRights AccessSubjectEditingContext::availableAccessRights() const
{
    return d->currentSubjectId.isNull()
        ? AccessRights{}
        : d->accessRightsResolver->availableAccessRights(d->currentSubjectId);
}

void AccessSubjectEditingContext::setRelations(
    const QSet<QnUuid>& parents, const QSet<QnUuid>& members)
{
    if (!NX_ASSERT(!d->currentSubjectId.isNull()))
        return;

    NX_DEBUG(this, "Setting subject relations, parents: %1, members: %2", parents, members);

    if (!d->currentHierarchy->addOrUpdate(d->currentSubjectId, parents, members))
        return;

    d->hierarchyChanged = true;
    emit hierarchyChanged();
}

void AccessSubjectEditingContext::resetRelations()
{
    NX_DEBUG(this, "Reverting subject relations");

    if (NX_ASSERT(d->systemSubjectHierarchy))
        *d->currentHierarchy = *d->systemSubjectHierarchy;
    else
        d->currentHierarchy->clear();

    d->hierarchyChanged = false;
    emit hierarchyChanged();
}

const SubjectHierarchy* AccessSubjectEditingContext::subjectHierarchy() const
{
    return d->currentHierarchy.get();
}

bool AccessSubjectEditingContext::hasChanges() const
{
    return d->hierarchyChanged || d->accessRightsManager->hasChanges();
}

void AccessSubjectEditingContext::revert()
{
    resetOwnResourceAccessMap();
    resetRelations();
    resetAccessibleResourcesFilter();
}

QnUuid AccessSubjectEditingContext::specialResourceGroupFor(const QnResourcePtr& resource)
{
    if (resource.objectCast<QnVirtualCameraResource>())
        return kAllDevicesGroupId;

    if (resource.objectCast<QnVideoWallResource>())
        return kAllVideoWallsGroupId;

    if (resource.objectCast<QnWebPageResource>())
        return kAllWebPagesGroupId;

    if (resource.objectCast<QnMediaServerResource>())
        return kAllServersGroupId;

    return {};
}

AccessRights AccessSubjectEditingContext::relevantAccessRights(SpecialResourceGroup group)
{
    switch (group)
    {
        case SpecialResourceGroup::allDevices:
            return kFullAccessRights;

        case SpecialResourceGroup::allVideowalls:
            return AccessRight::view;

        default:
            return AccessRight::view;
    }
}

AccessRights AccessSubjectEditingContext::relevantAccessRights(const QnResourcePtr& resource)
{
    if (resource.objectCast<QnLayoutResource>())
        return kFullAccessRights;

    if (const auto group = specialResourceGroup(specialResourceGroupFor(resource)))
        return relevantAccessRights(*group);

    NX_ASSERT(false, "Unexpected resource type: %1", resource);
    return {};
}

void AccessSubjectEditingContext::modifyAccessRights(ResourceAccessMap& accessMap,
    const QnUuid& resourceOrGroupId, AccessRights accessRightsMask, bool value)
{
    const auto accessRights = accessMap.value(resourceOrGroupId);
    const auto newAccessRights = value
        ? accessRights | accessRightsMask
        : accessRights & ~accessRightsMask;

    if (newAccessRights == accessRights)
        return;

    if (newAccessRights != 0)
        accessMap.emplace(resourceOrGroupId, newAccessRights);
    else
        accessMap.remove(resourceOrGroupId);
}

void AccessSubjectEditingContext::modifyAccessRights(const QList<QnResource*>& resources,
    AccessRights accessRights, bool value)
{
    auto map = ownResourceAccessMap();
    for (const auto& resource: resources)
    {
        if (!resource)
            continue;

        modifyAccessRights(map, resource->getId(),
            accessRights & relevantAccessRights(resource->toSharedPointer()), value);
    }

    setOwnResourceAccessMap(map);
}

} // namespace nx::vms::client::desktop
