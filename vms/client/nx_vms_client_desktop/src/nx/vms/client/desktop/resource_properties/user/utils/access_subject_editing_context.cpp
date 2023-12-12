// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_subject_editing_context.h"

#include <memory>
#include <type_traits>

#include <QtCore/QAbstractItemModel>

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/access_rights_resolver.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/common/system_context.h>

#include "proxy_access_rights_manager.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::api;
using namespace nx::vms::common;
using namespace nx::core::access;

namespace {

using AccessRightInt = std::make_unsigned_t<std::underlying_type_t<AccessRight>>;

static const QHash<AccessRight, AccessRights> kRequiredAccessRights{
    {AccessRight::viewArchive, AccessRight::view},
    {AccessRight::exportArchive, AccessRight::view | AccessRight::viewArchive},
    {AccessRight::viewBookmarks, AccessRight::view | AccessRight::viewArchive},
    {AccessRight::manageBookmarks,
        AccessRight::view | AccessRight::viewArchive | AccessRight::viewBookmarks},
    {AccessRight::userInput, AccessRight::view},
    {AccessRight::edit, AccessRight::view | AccessRight::userInput}};

static const QHash<AccessRight, AccessRights> kDependentAccessRights =
    []()
    {
        QHash<AccessRight, AccessRights> dependentAccessRights;
        for (const auto [dependent, dependsOn]: kRequiredAccessRights.asKeyValueRange())
        {
            for (AccessRightInt flag = 1; flag; flag <<= 1)
            {
                const auto requirement = static_cast<AccessRight>(flag);
                if (dependsOn.testFlag(requirement))
                    dependentAccessRights[requirement] |= dependent;
            }
        }

        return dependentAccessRights;
    }();

QModelIndex topLevelIndex(const QModelIndex& child)
{
    if (!child.isValid())
        return {};

    for (auto current = child;;)
    {
        const auto parent = current.parent();
        if (!parent.isValid())
            return current;

        current = parent;
    }
}

void fetchChildResourcesRecursively(const QModelIndex& parentIndex, QSet<QnResourcePtr>& resources)
{
    if (!NX_ASSERT(parentIndex.isValid()))
        return;

    const auto model = parentIndex.model();
    const int rowCount = model->rowCount(parentIndex);

    for (int row = 0; row < rowCount; ++row)
    {
        const auto child = model->index(row, 0, parentIndex);
        const auto resource = child.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (resource)
            resources.insert(resource);

        fetchChildResourcesRecursively(child, resources);
    }
}

ResourceAccessTarget accessTarget(const QModelIndex& resourceTreeModelIndex)
{
    const auto nodeTypeVar = resourceTreeModelIndex.data(Qn::NodeTypeRole);
    if (nodeTypeVar.isValid())
    {
        const auto nodeType = nodeTypeVar.value<ResourceTree::NodeType>();
        if (nodeType != ResourceTree::NodeType::resource)
            return ResourceAccessTarget(AccessSubjectEditingContext::specialResourceGroup(nodeType));
    }

    const auto resource = resourceTreeModelIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
    return ResourceAccessTarget(resource);
}

} // namespace

// ------------------------------------------------------------------------------------------------
// ResourceAccessTarget

ResourceAccessTarget::ResourceAccessTarget(const QnResourcePtr& resource):
    m_resource(resource),
    m_id(resource ? resource->getId() : QnUuid{})
{
}

ResourceAccessTarget::ResourceAccessTarget(SpecialResourceGroup group):
    m_group(group),
    m_id(specialResourceGroupId(group))
{
}

ResourceAccessTarget::ResourceAccessTarget(const QnUuid& specialResourceGroupId):
    m_group(specialResourceGroup(specialResourceGroupId)),
    m_id(m_group ? specialResourceGroupId : QnUuid{})
{
    NX_ASSERT(specialResourceGroupId.isNull() || m_group);
}

ResourceAccessTarget::ResourceAccessTarget(
    QnResourcePool* resourcePool,
    const QnUuid& resourceOrGroupId)
    :
    m_resource(NX_ASSERT(resourcePool)
        ? resourcePool->getResourceById(resourceOrGroupId)
        : QnResourcePtr{}),
    m_group(m_resource
        ? decltype(m_group){}
        : specialResourceGroup(resourceOrGroupId)),
    m_id(m_resource ? m_resource->getId() : (m_group ? resourceOrGroupId : QnUuid{}))
{
}

QString ResourceAccessTarget::toString() const
{
    return m_resource
        ? nx::toString(m_resource)
        : (m_group ? nx::toString(*m_group) : "<invalid>");
}

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
    SubjectType currentSubjectType = SubjectType::user;
    const QPointer<SubjectHierarchy> systemSubjectHierarchy;
    const std::unique_ptr<Hierarchy> currentHierarchy;
    const std::unique_ptr<ProxyAccessRightsManager> accessRightsManager;
    const std::unique_ptr<AccessRightsResolver> accessRightsResolver;
    const std::unique_ptr<AccessRightsResolver::Notifier> notifier;
    bool hierarchyChanged = false;
    bool updatingAccessRights = false;
    mutable QSet<QnUuid> currentlyAccessibleIdsCache; //< For filter by permissions.

public:
    explicit Private(AccessSubjectEditingContext* q):
        q(q),
        systemSubjectHierarchy(q->systemContext()->accessSubjectHierarchy()),
        currentHierarchy(new Hierarchy()),
        accessRightsManager(new ProxyAccessRightsManager(q->systemContext()->accessRightsManager())),
        accessRightsResolver(new AccessRightsResolver(
            q->systemContext()->resourcePool(),
            accessRightsManager.get(),
            q->systemContext()->globalPermissionsWatcher(),
            currentHierarchy.get(),
            AccessRightsResolver::Mode::editing)),
        notifier(accessRightsResolver->createNotifier())
    {
        connect(notifier.get(), &AccessRightsResolver::Notifier::resourceAccessChanged, this,
            [this]()
            {
                emit this->q->resourceAccessChanged();

                if (!updatingAccessRights)
                    emit this->q->accessibleByPermissionsFilterChanged();
            });

        connect(accessRightsResolver.get(), &AccessRightsResolver::resourceAccessReset,
            [this]()
            {
                emit this->q->resourceAccessChanged();
                this->q->resetAccessibleByPermissionsFilter();
            });

        connect(systemSubjectHierarchy, &SubjectHierarchy::reset,
            q, &AccessSubjectEditingContext::resetRelations);

        connect(
            accessRightsManager.get(), &ProxyAccessRightsManager::proxyAccessRightsAboutToBeChanged,
            this, [this]() { updatingAccessRights = true; });

        connect(accessRightsManager.get(), &ProxyAccessRightsManager::proxyAccessRightsChanged,
            this, [this]() { updatingAccessRights = false; });

        connect(systemSubjectHierarchy, &SubjectHierarchy::changed,
            this, &Private::handleSystemHierarchyChanged);

        connect(currentHierarchy.get(), &SubjectHierarchy::changed,
            q, &AccessSubjectEditingContext::hierarchyChanged);

        connect(currentHierarchy.get(), &SubjectHierarchy::reset,
            q, &AccessSubjectEditingContext::hierarchyChanged);

        connect(q->systemContext()->resourcePool(), &QnResourcePool::resourcesAdded,
            this, &Private::handleResourcesAddedOrRemoved);

        connect(q->systemContext()->resourcePool(), &QnResourcePool::resourcesRemoved,
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

    void handleSystemHierarchyChanged(
        const QSet<QnUuid>& added,
        const QSet<QnUuid>& removed,
        const QSet<QnUuid>& /*groupsWithChangedMembers*/,
        const QSet<QnUuid>& subjectsWithChangedParents)
    {
        if (removed.contains(currentSubjectId))
        {
            q->setCurrentSubject({}, SubjectType::user);
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

    static Qt::CheckState combinedOwnCheckState(
        const ResourceAccessMap& accessMap,
        const QModelIndexList& indexes,
        AccessRight accessRight)
    {
        int total = 0;
        int checked = 0;

        for (const auto& index: indexes)
        {
            if (!index.isValid())
                continue;

            const auto target = accessTarget(index);

            if (!target.isValid()) //< Simple grouping node.
            {
                QModelIndexList children;
                const auto model = index.model();
                const int childCount = model->rowCount(index);

                for (int i = 0; i < childCount; ++i)
                    children.push_back(model->index(i, 0, index));

                total += childCount;
                switch (combinedOwnCheckState(accessMap, children, accessRight))
                {
                    case Qt::PartiallyChecked: //< Do early return, if possible.
                        return Qt::PartiallyChecked;

                    case Qt::Checked:
                        checked += childCount;
                        break;

                    case Qt::Unchecked:
                        break;

                    default:
                        NX_ASSERT(false);
                        break;
                }

                continue;
            }

            if (!relevantAccessRights(target).testFlag(accessRight))
                continue;

            ++total;

            if (target.resource())
            {
                if (const auto groupId = specialResourceGroupFor(target.resource()); !groupId.isNull())
                {
                    if (accessMap.value(groupId).testFlag(accessRight))
                    {
                        ++checked;
                        continue;
                    }
                }
            }

            if (accessMap.value(target.id()).testFlag(accessRight))
                ++checked;

            if (checked > 0 && checked < total) //< Do early return, if possible.
                return Qt::PartiallyChecked;
        }

        return (checked < total || total == 0)
            ? (checked > 0 ? Qt::PartiallyChecked : Qt::Unchecked)
            : Qt::Checked;
    }

    void modifyAccessRight(ResourceAccessMap& accessMap, const QModelIndex& objectIndex,
        AccessRight accessRight, bool value, bool withDependentAccessRights) const
    {
        const auto item = resourceAccessTreeItemInfo(objectIndex);
        const auto toggledRight = static_cast<AccessRight>(accessRight);
        const auto objectName = objectIndex.data(Qt::DisplayRole).toString();
        const auto logValue = [](bool value) { return value ? "on" : "off"; };

        NX_VERBOSE(q, "Toggle `%1` %2 for \"%3\"", toggledRight, logValue(value), objectName);

        const auto outerGroupAccessRights = item.outerSpecialResourceGroupId.isNull()
            ? AccessRights{}
            : accessMap.value(item.outerSpecialResourceGroupId);

        const Qt::CheckState oldCheckState = combinedOwnCheckState(accessMap, {objectIndex},
            (AccessRight) accessRight);

        const bool itemWasChecked = oldCheckState == Qt::Checked;
        const bool outerGroupWasChecked = outerGroupAccessRights.testFlag(toggledRight);

        if (oldCheckState != Qt::PartiallyChecked && itemWasChecked == value)
        {
            NX_VERBOSE(q, "Already toggled %1, nothing to do", logValue(value));
            return;
        }

        NX_VERBOSE(q, "Relevant access rights: `%1`", item.relevantAccessRights);

        if (!item.relevantAccessRights.testFlag(toggledRight))
            return;

        AccessRights toggledMask = toggledRight;
        if (withDependentAccessRights)
        {
            toggledMask |= (value
                ? (requiredAccessRights(toggledRight) & item.relevantAccessRights)
                : dependentAccessRights(toggledRight));
        }

        NX_VERBOSE(q, "Access rights to toggle %1: `%2`", logValue(value), toggledMask);

        if (item.type == ResourceAccessTreeItem::Type::specialResourceGroup)
        {
            // If we're toggling a special group off, we must explicitly toggle its children off.
            // If we're toggling a special group on, we also must explicitly toggle its children off,
            // except access rights required for other set access rights.

            if (value)
            {
                for (const auto& resource: q->getGroupResources(item.target.id()))
                {
                    const auto resourceId = resource->getId();
                    const auto toKeep = accessMap.value(resourceId) & ~toggledMask;
                    const auto toClear = toggledMask & ~Private::withRequirements(toKeep);

                    if (toClear)
                    {
                        NX_VERBOSE(q, "Toggling off `%1` for %2", toClear, resource);
                        modifyAccessRightMap(accessMap, resourceId, toClear, false);
                    }
                }
            }
            else
            {
                NX_VERBOSE(q, "Toggling off `%1` for all group's children", toggledMask);

                for (const auto& resource: q->getGroupResources(item.target.id()))
                    modifyAccessRightMap(accessMap, resource->getId(), toggledMask, false);
            }
        }

        if (outerGroupWasChecked)
        {
            // If we're toggling off an item that was implicitly toggled on by its group,
            // we must toggle the group off, and explicitly toggle all its children on.

            NX_VERBOSE(q,
                "`%1` was toggled on for the outer group %2; toggling it on for all its children",
                toggledMask, item.outerSpecialResourceGroupId);

            for (const auto& resource: q->getGroupResources(item.outerSpecialResourceGroupId))
            {
                modifyAccessRightMap(
                    accessMap, resource->getId(), toggledMask & outerGroupAccessRights, true);
            }

            NX_VERBOSE(q, "Toggling off `%1` for the outer group %2", toggledMask,
                item.outerSpecialResourceGroupId);
            modifyAccessRightMap(accessMap, item.outerSpecialResourceGroupId, toggledMask, false);
        }

        if (item.target.isValid())
        {
            // Toggle the item.
            NX_VERBOSE(q, "Toggling %1 `%2` for %3", logValue(value), toggledMask, item.target);
            modifyAccessRightMap(accessMap, item.target.id(), toggledMask, value);
        }
        else
        {
            // If we're toggling a grouping node on or off, we just toggle its children on or off.
            NX_VERBOSE(q, "Toggling %1 `%2` for group's children", logValue(value), toggledMask);

            for (const auto& resource: getChildResources(objectIndex))
                modifyAccessRightMap(accessMap, resource->getId(), toggledMask, value);
        }
    }

    static AccessRights withRequirements(AccessRights for_)
    {
        if (!for_)
            return {};

        AccessRights requirements{};

        for (unsigned int i = 1; i != 0; i <<= 1)
        {
            if ((for_ & i) != 0)
                requirements |= requiredAccessRights((AccessRight) i);
        }

        return for_ | requirements;
    };
};

// ------------------------------------------------------------------------------------------------
// AccessSubjectEditingContext

AccessSubjectEditingContext::AccessSubjectEditingContext(
    nx::vms::client::core::SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    nx::vms::client::core::SystemContextAware(systemContext),
    d(new Private(this))
{
}

AccessSubjectEditingContext::~AccessSubjectEditingContext()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnUuid AccessSubjectEditingContext::currentSubjectId() const
{
    return d->currentSubjectId;
}

AccessSubjectEditingContext::SubjectType AccessSubjectEditingContext::currentSubjectType() const
{
    return d->currentSubjectType;
}

void AccessSubjectEditingContext::setCurrentSubject(
    const QnUuid& subjectId, SubjectType subjectType)
{
    if (d->currentSubjectId == subjectId)
    {
        if (!NX_ASSERT(d->currentSubjectType == subjectType))
            d->currentSubjectType = subjectType;
        return;
    }

    NX_DEBUG(this, "Changing current subject to %1 (%2)", subjectId, subjectType);

    d->currentSubjectId = subjectId;
    d->currentSubjectType = subjectType;
    d->accessRightsManager->setCurrentSubjectId(subjectId);
    d->notifier->setSubjectId(subjectId);
    resetRelations();
    resetAccessibleByPermissionsFilter();

    emit subjectChanged();
    emit resourceAccessChanged();

    if (!d->currentSubjectId.isNull())
    {
        NX_DEBUG(this, "Current subject's access map is %1",
            toString(ownResourceAccessMap(), resourcePool()));
    }
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
    NX_DEBUG(this, "Setting access map to %1", toString(resourceAccessMap, resourcePool()));
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

AccessRights AccessSubjectEditingContext::accessRights(const ResourceAccessTarget& target) const
{
    if (!target.isValid() || d->currentSubjectId.isNull())
        return {};

    return target.resource()
        ? d->accessRightsResolver->accessRights(d->currentSubjectId, target.resource())
        : d->accessRightsResolver->accessRights(d->currentSubjectId, target.id());
}

ResourceAccessDetails AccessSubjectEditingContext::accessDetails(
    const QnResourcePtr& resource, AccessRight accessRight) const
{
    return d->currentSubjectId.isNull()
        ? ResourceAccessDetails{}
        : d->accessRightsResolver->accessDetails(d->currentSubjectId, resource, accessRight);
}

QList<QnUuid> AccessSubjectEditingContext::resourceGroupAccessProviders(
    const QnUuid& resourceGroupId, AccessRight accessRight) const
{
    if (d->currentSubjectId.isNull())
        return {};

    QList<QnUuid> result;
    for (const auto groupId: d->currentHierarchy->directParents(d->currentSubjectId))
    {
        if (d->accessRightsResolver->accessRights(groupId, resourceGroupId).testFlag(accessRight))
            result.push_back(groupId);
    }

    return result;
}

AccessSubjectEditingContext::IsIndexAccepted
    AccessSubjectEditingContext::accessibleByPermissionsFilter() const
{
    const auto filter =
        [this, guard = QPointer(d.get())](const QModelIndex& sourceIndex) -> bool
        {
            if (!guard || !sourceIndex.isValid())
                return false;

            const auto target = accessTarget(sourceIndex);
            if (!target.isValid())
                return false;

            if (d->currentlyAccessibleIdsCache.contains(target.id()))
                return true;

            const auto possessedRights = accessRights(target);
            if (!possessedRights)
                return false;

            d->currentlyAccessibleIdsCache.insert(target.id());
            return true;
    };

    return filter;
}

void AccessSubjectEditingContext::resetAccessibleByPermissionsFilter()
{
    NX_VERBOSE(this, "Resetting 'Accessible by permissions' filter");

    d->currentlyAccessibleIdsCache.clear();
    emit accessibleByPermissionsFilterChanged();
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
    resetAccessibleByPermissionsFilter();
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

QnUuid AccessSubjectEditingContext::specialResourceGroup(ResourceTree::NodeType nodeType)
{
    switch (nodeType)
    {
        case ResourceTree::NodeType::camerasAndDevices:
            return nx::vms::api::kAllDevicesGroupId;

        case ResourceTree::NodeType::videoWalls:
            return nx::vms::api::kAllVideoWallsGroupId;

        case ResourceTree::NodeType::integrations:
        case ResourceTree::NodeType::webPages:
            return nx::vms::api::kAllWebPagesGroupId;

        case ResourceTree::NodeType::servers:
            return nx::vms::api::kAllServersGroupId;

        default:
            return QnUuid();
    }
}

AccessRights AccessSubjectEditingContext::relevantAccessRights(const ResourceAccessTarget& target)
{
    if (!target.isValid())
        return {};

    const auto relevantRights =
        [](SpecialResourceGroup group) -> AccessRights
        {
            switch (group)
            {
                case SpecialResourceGroup::allDevices:
                    return kFullAccessRights;

                case SpecialResourceGroup::allVideowalls:
                    return AccessRight::edit;

                default:
                    return AccessRight::view;
            }
        };

    if (auto group = target.group())
        return relevantRights(*group);

    if (target.resource().objectCast<QnLayoutResource>())
        return kFullAccessRights;

    if (const auto group = api::specialResourceGroup(specialResourceGroupFor(target.resource())))
        return relevantRights(*group);

    NX_ASSERT(false, "Unexpected resource type: %1", target.resource());
    return {};
}

AccessRights AccessSubjectEditingContext::combinedRelevantAccessRights(
    const QModelIndexList& indexes) const
{
    return std::accumulate(indexes.cbegin(), indexes.cend(), AccessRights{},
        [this](AccessRights sum, const QModelIndex& index)
        {
            return sum | resourceAccessTreeItemInfo(index).relevantAccessRights;
        });
}

void AccessSubjectEditingContext::modifyAccessRightMap(
    ResourceAccessMap& accessRightMap,
    const QnUuid& resourceOrGroupId,
    AccessRights modifiedRightsMask,
    bool value,
    bool withDependent,
    AccessRights relevantRightsMask)
{
    if (withDependent)
    {
        const auto dependencies = value
            ? kRequiredAccessRights
            : kDependentAccessRights;

        const auto sourceRights = modifiedRightsMask;
        for (AccessRightInt flag = 1; flag != 0; flag <<= 1)
        {
            const auto accessRight = static_cast<AccessRight>(flag);
            if (sourceRights.testFlag(accessRight))
                modifiedRightsMask |= dependencies.value(accessRight);
        }
    }

    if (value)
        modifiedRightsMask &= relevantRightsMask;

    const auto accessRights = accessRightMap.value(resourceOrGroupId);
    const auto newAccessRights = value
        ? accessRights | modifiedRightsMask
        : accessRights & ~modifiedRightsMask;

    if (newAccessRights == accessRights)
        return;

    if (newAccessRights != 0)
        accessRightMap.emplace(resourceOrGroupId, newAccessRights);
    else
        accessRightMap.remove(resourceOrGroupId);
}

Qt::CheckState AccessSubjectEditingContext::combinedOwnCheckState(
    const QModelIndexList& indexes, AccessRight accessRight) const
{
    return d->combinedOwnCheckState(ownResourceAccessMap(), indexes, accessRight);
}

QnResourceList AccessSubjectEditingContext::selectionLayouts(
    const QModelIndexList& selection) const
{
    const auto currentUser = accessController()->user();
    if (!NX_ASSERT(currentUser))
        return {};

    const auto currentUserId = currentUser->getId();

    QnLayoutResourceList result;
    for (const auto& index: selection)
    {
        const auto nodeType = index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
        if (nodeType == ResourceTree::NodeType::layouts)
        {
            return resourcePool()->getResources<LayoutResource>(
                [this, &currentUserId](const LayoutResourcePtr& layout) -> bool
                {
                    if (layout->isFile())
                        return false;

                    const auto layoutId = layout->getParentId();
                    return layoutId.isNull() || layoutId == currentUserId;
                });
        }

        if (const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>())
        {
            if (const auto layout = resource.objectCast<QnLayoutResource>())
                result.push_back(layout);
        }
    }

    return result;
}

QnResourceList AccessSubjectEditingContext::getGroupResources(const QnUuid& resourceGroupId) const
{
    // Only special resource groups are supported at this time.

    const auto group = api::specialResourceGroup(resourceGroupId);
    if (!NX_ASSERT(group))
        return {};

    const auto resourcePool = systemContext()->resourcePool();
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

QnResourceList AccessSubjectEditingContext::getChildResources(
    const QModelIndex& parentTreeNodeIndex)
{
    QSet<QnResourcePtr> result;
    fetchChildResourcesRecursively(parentTreeNodeIndex, result);
    return QnResourceList(
        std::make_move_iterator(result.begin()),
        std::make_move_iterator(result.end()));
}

void AccessSubjectEditingContext::modifyAccessRight(const QModelIndex& objectIndex,
    int /*AccessRight*/ accessRight, bool value, bool withDependentAccessRights)
{
    modifyAccessRight(objectIndex, (AccessRight) accessRight, value, withDependentAccessRights);
}

void AccessSubjectEditingContext::modifyAccessRight(const QModelIndex& objectIndex,
    AccessRight accessRight, bool value, bool withDependentAccessRights)
{
    auto accessMap = ownResourceAccessMap();
    d->modifyAccessRight(accessMap, objectIndex, accessRight, value, withDependentAccessRights);
    setOwnResourceAccessMap(accessMap);
}

void AccessSubjectEditingContext::modifyAccessRights(const QModelIndexList& indexes,
    AccessRights modifiedRightsMask, bool value, bool withDependent)
{
    auto accessMap = ownResourceAccessMap();

    for (unsigned int i = 1; i != 0; i <<= 1)
    {
        if ((i & modifiedRightsMask) == 0)
            continue;

        for (const auto& index: indexes)
            d->modifyAccessRight(accessMap, index, (AccessRight) i, value, withDependent);
    }

    setOwnResourceAccessMap(accessMap);
}

AccessRights AccessSubjectEditingContext::dependentAccessRights(AccessRight dependingOn)
{
    return kDependentAccessRights.value(dependingOn);
}

AccessRights AccessSubjectEditingContext::requiredAccessRights(AccessRight requiredFor)
{
    return kRequiredAccessRights.value(requiredFor);
}

bool AccessSubjectEditingContext::isDependingOn(int /*AccessRight*/ what, AccessRights on)
{
    return requiredAccessRights(static_cast<AccessRight>(what)).testAnyFlags(on);
}

bool AccessSubjectEditingContext::isRequiredFor(int /*AccessRight*/ what, AccessRights for_)
{
    return dependentAccessRights(static_cast<AccessRight>(what)).testAnyFlags(for_);
}

ResourceAccessTreeItem AccessSubjectEditingContext::resourceAccessTreeItemInfo(
    const QModelIndex& resourceTreeModelIndex)
{
    if (!resourceTreeModelIndex.isValid())
        return {};

    const auto resource = resourceTreeModelIndex.data(Qn::ResourceRole).value<QnResourcePtr>();

    if (resource)
    {
        return ResourceAccessTreeItem{
            .type = ResourceAccessTreeItem::Type::resource,
            .target = resource,
            .nodeType = ResourceTree::NodeType::resource, //< For consistency.
            .outerSpecialResourceGroupId = specialResourceGroupFor(resource),
            .relevantAccessRights = relevantAccessRights(resource)};
    }

    const auto nodeType =
        resourceTreeModelIndex.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();

    const auto specialResourceGroupId = specialResourceGroup(nodeType);
    if (!specialResourceGroupId.isNull())
    {
        return ResourceAccessTreeItem{
            .type = ResourceAccessTreeItem::Type::specialResourceGroup,
            .target = specialResourceGroupId,
            .nodeType = nodeType,
            .relevantAccessRights = relevantAccessRights(specialResourceGroupId)};
    }

    const auto topLevel = topLevelIndex(resourceTreeModelIndex);
    const auto topLevelNodeType = topLevel.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();

    if (nodeType == ResourceTree::NodeType::layouts)
    {
        return ResourceAccessTreeItem{
            .type = ResourceAccessTreeItem::Type::groupingNode,
            .nodeType = nodeType,
            .relevantAccessRights = kFullAccessRights};
    }

    const auto outerSpecialResourceGroupId = specialResourceGroup(topLevelNodeType);

    return ResourceAccessTreeItem{
        .type = ResourceAccessTreeItem::Type::groupingNode,
        .nodeType = nodeType,
        .outerSpecialResourceGroupId = outerSpecialResourceGroupId,
        .relevantAccessRights = relevantAccessRights(outerSpecialResourceGroupId)};
}

} // namespace nx::vms::client::desktop
