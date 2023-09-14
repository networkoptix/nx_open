// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_rights_model.h"

#include <algorithm>
#include <vector>

#include <QtCore/QCollator>
#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/format.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>

// We removed common metatype declaration but for some reason it's still required here for proper
// `QVector<AccessRight>` registration.
Q_DECLARE_METATYPE(nx::vms::api::AccessRight)

namespace nx::vms::client::desktop {

using namespace nx::core::access;
using namespace nx::vms::api;

namespace {

void modifyAccessRightMap(ResourceAccessMap& accessRightMap, const QnUuid& resourceOrGroupId,
    AccessRights modifiedRightsMask, bool value)
{
    AccessSubjectEditingContext::modifyAccessRightMap(
        accessRightMap,
        resourceOrGroupId,
        modifiedRightsMask,
        value,
        /*withDependent*/ false,
        /*relevantRightsMask*/ kFullAccessRights);
}

AccessRights relevantAccessRights(const QnUuid& resourceGroupId)
{
    if (const auto group = specialResourceGroup(resourceGroupId))
        return AccessSubjectEditingContext::relevantAccessRights(*group);

    return kFullAccessRights;
}

int providerPriority(ResourceAccessInfo::ProvidedVia providerType)
{
    static const QVector<ResourceAccessInfo::ProvidedVia> kProvidersSortedByAscendingPriority = {
        ResourceAccessInfo::ProvidedVia::parentUserGroup,
        ResourceAccessInfo::ProvidedVia::videowall,
        ResourceAccessInfo::ProvidedVia::layout,
        ResourceAccessInfo::ProvidedVia::own,
        ResourceAccessInfo::ProvidedVia::ownResourceGroup};

    static const QMap<ResourceAccessInfo::ProvidedVia, int> kPriorityByProvider =
        [&]()
        {
            QMap<ResourceAccessInfo::ProvidedVia, int> result;
            for (int i = 0; i < kProvidersSortedByAscendingPriority.size(); ++i)
                result[kProvidersSortedByAscendingPriority[i]] = i;
            return result;
        }();

    static const int kLowestPriority = -1;
    return kPriorityByProvider.value(providerType, /*defaultValue*/ kLowestPriority);
}

} // namespace

// ------------------------------------------------------------------------------------------------
// ResourceAccessInfo

bool ResourceAccessInfo::operator==(const ResourceAccessInfo& other) const
{
    return providedVia == other.providedVia
        && providerUserGroups == other.providerUserGroups
        && indirectProviders == other.indirectProviders
        && checkedChildCount == other.checkedChildCount;
}

bool ResourceAccessInfo::operator!=(const ResourceAccessInfo& other) const
{
    return !(*this == other);
}

// ------------------------------------------------------------------------------------------------
// ResourceAccessRightsModel

struct ResourceAccessRightsModel::Private
{
    ResourceAccessRightsModel* const q;

    QPointer<AccessSubjectEditingContext> context;
    QVector<AccessRight> accessRightList;
    QnResourcePtr resource;
    ResourceTree::NodeType nodeType = ResourceTree::NodeType::spacer;
    QVector<ResourceAccessInfo> info;

    nx::utils::ScopedConnections contextConnections;

    void updateInfo(bool suppressSignals = false);
    QVector<ResourceAccessInfo> calculateInfo() const;
    QnResourceList getResources() const;
    QnResourceList getGroupResources(const QnUuid& groupId) const;
    std::vector<QnUuid> getGroupContents(const QnUuid& groupId) const;

    void countGroupResources(const QnUuid& groupId, AccessRight accessRight,
        int& checked, int& total) const;

    AccessRights relevantAccessRights() const;

    bool isEditable(int index) const;

    QString accessDetailsText(const ResourceAccessInfo& accessInfo) const;
};

ResourceAccessRightsModel::ResourceAccessRightsModel(QObject* parent):
    base_type(parent),
    d(new Private{this})
{
}

ResourceAccessRightsModel::~ResourceAccessRightsModel()
{
    // Required here for forward-declared scoped pointer destruction.
}

AccessSubjectEditingContext* ResourceAccessRightsModel::context() const
{
    return d->context;
}

void ResourceAccessRightsModel::setContext(AccessSubjectEditingContext* value)
{
    if (d->context == value)
        return;

    d->contextConnections.reset();

    d->context = value;
    d->updateInfo();

    if (d->context)
    {
        d->contextConnections <<
            connect(d->context, &AccessSubjectEditingContext::resourceAccessChanged, this,
                [this]() { d->updateInfo(); });

        d->contextConnections <<
            connect(d->context, &AccessSubjectEditingContext::resourceGroupsChanged, this,
                [this](const QSet<QnUuid>& resourceGroupIds)
                {
                    if (resourceGroupIds.contains(groupId()))
                        d->updateInfo();
                });
    }

    emit contextChanged();
}

QVector<AccessRight> ResourceAccessRightsModel::accessRightsList() const
{
    return d->accessRightList;
}

void ResourceAccessRightsModel::setAccessRightsList(const QVector<AccessRight>& value)
{
    if (d->accessRightList == value)
        return;

    beginResetModel();
    d->accessRightList = value;
    d->updateInfo(/*suppressSignals*/ true);
    endResetModel();

    emit accessRightsListChanged();
}

QnResource* ResourceAccessRightsModel::resource() const
{
    return d->resource.get();
}

void ResourceAccessRightsModel::setResource(QnResource* value)
{
    const auto shared = value ? value->toSharedPointer() : QnResourcePtr();
    if (!NX_ASSERT(shared.get() == value, "Cannot obtain a shared pointer to the resource"))
        return;

    if (d->resource == shared)
        return;

    d->resource = shared;
    d->updateInfo();

    emit resourceChanged();
}

QnUuid ResourceAccessRightsModel::groupId() const
{
    switch (d->nodeType)
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

ResourceTree::NodeType ResourceAccessRightsModel::nodeType() const
{
    return d->nodeType;
}

void ResourceAccessRightsModel::setNodeType(ResourceTree::NodeType value)
{
    if (d->nodeType == value)
        return;

    d->nodeType = value;
    d->updateInfo();

    emit nodeTypeChanged();
}

QVariant ResourceAccessRightsModel::data(const QModelIndex& index, int role) const
{
    if (index.row() >= rowCount() || index.row() < 0)
        return {};

    switch (role)
    {
        case ProviderRole:
            return static_cast<int>(d->info[index.row()].providedVia);
        case TotalChildCountRole:
            return static_cast<int>(d->info[index.row()].totalChildCount);
        case CheckedChildCountRole:
            return static_cast<int>(d->info[index.row()].checkedChildCount);
        case AccessRightRole:
            return static_cast<int>(d->accessRightList[index.row()]);
        case EditableRole:
            return d->isEditable(index.row());
        case Qt::ToolTipRole:
            return d->accessDetailsText(d->info[index.row()]);
        default:
            return {};
    }
}

void ResourceAccessRightsModel::toggle(int index, bool withDependentAccessRights)
{
    if (index >= rowCount() || index < 0 || !d->context)
        return;

    const auto id = d->resource ? d->resource->getId() : groupId();
    if (!NX_ASSERT(!id.isNull()))
        return;

    const auto toggledRight = d->accessRightList[index];

    if (d->resource)
        NX_VERBOSE(this, "Toggle request for `%1`, for %2", toggledRight, d->resource);
    else
        NX_VERBOSE(this, "Toggle request for `%1`, for %2", toggledRight, id);

    const bool isGroup = !d->resource;
    const auto outerGroupId = AccessSubjectEditingContext::specialResourceGroupFor(d->resource);
    const bool hasOuterGroup = !outerGroupId.isNull();

    auto accessMap = d->context->ownResourceAccessMap();
    const auto itemAccessRights = accessMap.value(id);

    const auto outerGroupAccessRights = hasOuterGroup
        ? accessMap.value(outerGroupId)
        : AccessRights{};

    const auto& info = d->info[index];
    const auto allChildrenWereChecked = info.totalChildCount > 0
        && info.totalChildCount == info.checkedChildCount;

    const bool outerGroupWasChecked = outerGroupAccessRights.testFlag(toggledRight);
    const bool itemWasChecked = outerGroupWasChecked || itemAccessRights.testFlag(toggledRight);
    const bool itemWillBeChecked = !(itemWasChecked || allChildrenWereChecked);

    const auto relevantRights = isGroup
        ? relevantAccessRights(groupId())
        : relevantAccessRights(outerGroupId);

    NX_VERBOSE(this, "Relevant access rights: `%1`", relevantRights);

    if (!NX_ASSERT(relevantRights.testFlag(toggledRight)))
        return;

    AccessRights toggledMask = toggledRight;
    if (withDependentAccessRights)
    {
        toggledMask |= (itemWillBeChecked
            ? (AccessSubjectEditingContext::requiredAccessRights(toggledRight) & relevantRights)
            : AccessSubjectEditingContext::dependentAccessRights(toggledRight));
    }

    NX_VERBOSE(this, "Access rights to toggle %1: `%2`", itemWillBeChecked ? "on" : "off",
        toggledMask);

    if (isGroup)
    {
        // If we're toggling a group on, we must explicitly toggle all its children off.
        // If we're toggling a group off, we must explicitly toggle all its children on.

        auto mask = itemWasChecked
            ? (toggledMask & itemAccessRights)
            : toggledMask;

        NX_VERBOSE(this, "Toggling %1 `%2` for group's children", itemWasChecked ? "on" : "off",
            mask);

        for (const auto& itemId: d->getGroupContents(id))
            modifyAccessRightMap(accessMap, itemId, mask, itemWasChecked);
    }

    if (outerGroupWasChecked)
    {
        // If we're toggling off an item that was implicitly toggled on by its group,
        // we must toggle the group off, and explicitly toggle all its children on.

        NX_VERBOSE(this,
            "`%1` was toggled on for the outer group %2; toggling it on for all its children",
            toggledMask, outerGroupId);

        for (const auto& itemId: d->getGroupContents(outerGroupId))
            modifyAccessRightMap(accessMap, itemId, toggledMask, true);

        NX_VERBOSE(this, "Toggling off `%1` for the outer group %2", toggledMask, outerGroupId);
        modifyAccessRightMap(accessMap, outerGroupId, toggledMask, false);
    }

    // Toggle the item.
    NX_VERBOSE(this, "Toggling %1 `%2` for %3", itemWillBeChecked ? "on" : "off", toggledMask, id);
    modifyAccessRightMap(accessMap, id, toggledMask, itemWillBeChecked);
    d->context->setOwnResourceAccessMap(accessMap);
}

QHash<int, QByteArray> ResourceAccessRightsModel::roleNames() const
{
    auto names = base_type::roleNames();
    names.insert(ProviderRole, "providedVia");
    names.insert(TotalChildCountRole, "totalChildCount");
    names.insert(CheckedChildCountRole, "checkedChildCount");
    names.insert(AccessRightRole, "accessRight");
    names.insert(EditableRole, "editable");
    return names;
}

int ResourceAccessRightsModel::rowCount(const QModelIndex&) const
{
    return d->accessRightList.size();
}

QVector<ResourceAccessInfo> ResourceAccessRightsModel::info() const
{
    return d->info;
}

ResourceAccessInfo::ProvidedVia ResourceAccessRightsModel::providerType(QnResource* provider)
{
    if (qobject_cast<QnLayoutResource*>(provider))
        return ResourceAccessInfo::ProvidedVia::layout;

    if (qobject_cast<QnVideoWallResource*>(provider))
        return ResourceAccessInfo::ProvidedVia::videowall;

    NX_ASSERT(false, "Unknown indirect provider type: %1", provider);
    return ResourceAccessInfo::ProvidedVia::unknown;
}

void ResourceAccessRightsModel::registerQmlTypes()
{
    qRegisterMetaType<QVector<AccessRight>>();
    qRegisterMetaType<QVector<ResourceAccessInfo>>();
    qRegisterMetaType<QVector<QnUuid>>();
    qRegisterMetaType<QVector<QnResource*>>();

    qmlRegisterUncreatableType<ResourceAccessInfo>("nx.vms.client.desktop", 1, 0,
        "ResourceAccessInfo", "Cannot create an instance of ResourceAccessInfo");

    qmlRegisterType<ResourceAccessRightsModel>(
        "nx.vms.client.desktop", 1, 0, "ResourceAccessRightsModel");
}

// ------------------------------------------------------------------------------------------------
// ResourceAccessRightsModel::Private

QnResourceList ResourceAccessRightsModel::Private::getResources() const
{
    if (!context)
        return {};

    const auto resourcePool = context->systemContext()->resourcePool();

    switch (nodeType)
    {
        case ResourceTree::NodeType::camerasAndDevices:
            return resourcePool->getAllCameras(QnUuid{}, /*ignoreDesktopCameras*/ true);

        case ResourceTree::NodeType::integrations:
        case ResourceTree::NodeType::webPages:
            return resourcePool->getResources<QnWebPageResource>();

        case ResourceTree::NodeType::layouts:
            return resourcePool->getResources<QnLayoutResource>().filtered(
                [](const QnLayoutResourcePtr& layout)
                {
                    return !layout->isFile() && !layout->hasFlags(Qn::cross_system);
                });

        case ResourceTree::NodeType::servers:
            return resourcePool->getResources<QnMediaServerResource>();

        case ResourceTree::NodeType::videoWalls:
            return resourcePool->getResources<QnVideoWallResource>();

        default:
            return resource ? QnResourceList{resource} : QnResourceList{};
    }
}

void ResourceAccessRightsModel::Private::updateInfo(bool suppressSignals)
{
    const auto newResourceAccessInfo = calculateInfo();
    if (newResourceAccessInfo == info)
        return;

    info = newResourceAccessInfo;
    if (info.size() > 0 && !suppressSignals)
    {
        emit q->dataChanged(
            q->index(0, 0),
            q->index(info.size() - 1, 0));
    }
}

QVector<ResourceAccessInfo> ResourceAccessRightsModel::Private::calculateInfo() const
{
    const auto groupId = q->groupId();
    const bool isResourceGroup = !groupId.isNull();

    const int count = accessRightList.size();
    QVector<ResourceAccessInfo> result(count);

    if (!context || context->currentSubjectId().isNull() || (!resource && !isResourceGroup))
        return result;

    for (int i = 0; i < count; ++i)
    {
        auto& newInfo = result[i];
        if (isResourceGroup)
        {
            newInfo.providerUserGroups = context->resourceGroupAccessProviders(
                groupId, accessRightList[i]);

            // Keep arrays sorted for easy comparison.
            std::sort(newInfo.providerUserGroups.begin(), newInfo.providerUserGroups.end());

            if (context->hasOwnAccessRight(groupId, accessRightList[i]))
            {
                newInfo.providedVia = ResourceAccessInfo::ProvidedVia::own;
            }
            else
            {
                if (!newInfo.providerUserGroups.empty())
                    newInfo.providedVia = ResourceAccessInfo::ProvidedVia::parentUserGroup;

                countGroupResources(groupId, accessRightList[i],
                    newInfo.checkedChildCount, newInfo.totalChildCount);
            }

            continue;
        }

        const auto details = context->accessDetails(resource, accessRightList[i]);
        if (details.contains(context->currentSubjectId()))
        {
            const auto providers = details.value(context->currentSubjectId());
            for (const auto& provider: providers)
            {
                if (!NX_ASSERT(provider))
                    continue;

                if (provider == resource)
                {
                    const auto resourceGroupId =
                        AccessSubjectEditingContext::specialResourceGroupFor(resource);

                    const auto accessViaResourceGroup = !resourceGroupId.isNull()
                        && context->hasOwnAccessRight(resourceGroupId, accessRightList[i]);

                    if (accessViaResourceGroup)
                        newInfo.parentResourceGroupId = resourceGroupId;

                    newInfo.providedVia = accessViaResourceGroup
                        ? ResourceAccessInfo::ProvidedVia::ownResourceGroup
                        : ResourceAccessInfo::ProvidedVia::own;
                }
                else
                {
                    const auto providedVia = providerType(provider.get());
                    if (providerPriority(providedVia) > providerPriority(newInfo.providedVia))
                        newInfo.providedVia = providedVia;

                    // Keep arrays sorted for easy comparison.
                    const auto insertionPos = std::upper_bound(
                        newInfo.indirectProviders.begin(),
                        newInfo.indirectProviders.end(),
                        provider.get());

                    newInfo.indirectProviders.insert(insertionPos, provider);
                }
            }
        }
        else if (!details.empty())
        {
            newInfo.providedVia = ResourceAccessInfo::ProvidedVia::parentUserGroup;
        }

        if (newInfo.providedVia != ResourceAccessInfo::ProvidedVia::none)
        {
            const QSet<QnUuid> providerIds{details.keyBegin(), details.keyEnd()};

            // Show only the direct parents which provide current access rights.
            const auto directParents =
                context->subjectHierarchy()->directParents(context->currentSubjectId());

            newInfo.providerUserGroups = {};
            for (const auto& key: directParents)
            {
                if (providerIds.contains(key)
                    || context->subjectHierarchy()->isRecursiveMember(key, providerIds))
                {
                    newInfo.providerUserGroups << key;
                }
            }

            // Keep arrays sorted for easy comparison.
            std::sort(newInfo.providerUserGroups.begin(), newInfo.providerUserGroups.end());
        }
    }

    return result;
}

QnResourceList ResourceAccessRightsModel::Private::getGroupResources(const QnUuid& groupId) const
{
    // Only special resource groups are supported at this time.

    const auto group = specialResourceGroup(groupId);
    if (!context || !NX_ASSERT(group))
        return {};

    const auto resourcePool = context->systemContext()->resourcePool();
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

std::vector<QnUuid> ResourceAccessRightsModel::Private::getGroupContents(
    const QnUuid& groupId) const
{
    // Only special resource groups are supported at this time.

    const auto groupResources = getGroupResources(groupId);
    if (groupResources.empty())
        return {};

    std::vector<QnUuid> result;
    result.reserve(groupResources.size());
    std::transform(groupResources.begin(), groupResources.end(), std::back_inserter(result),
        [](const QnResourcePtr& resource) { return resource->getId(); });
    return result;
}

void ResourceAccessRightsModel::Private::countGroupResources(
    const QnUuid& groupId, AccessRight accessRight, int& checked, int& total) const
{
    const auto contents = getGroupResources(groupId);
    total = contents.size();

    const auto accessMap = context->ownResourceAccessMap();
    checked = std::count_if(contents.cbegin(), contents.cend(),
        [accessMap, accessRight](const QnResourcePtr& resource)
        {
            return accessMap.value(resource->getId()).testFlag(accessRight);
        });
}

QString ResourceAccessRightsModel::Private::accessDetailsText(
    const ResourceAccessInfo& accessInfo) const
{
    using namespace nx::vms::common;

    if (!context)
        return {};

    QStringList groups, layouts, videoWalls;
    QStringList descriptions;

    std::vector<nx::vms::api::UserGroupData> groupsData;

    for (const auto& groupId: accessInfo.providerUserGroups)
    {
        if (const auto group = context->systemContext()->userGroupManager()->find(groupId))
            groupsData.push_back(*group);
    }

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);

    std::sort(groupsData.begin(), groupsData.end(),
        [&collator](const auto& left, const auto& right)
        {
            // Predefined groups go first.
            const auto predefinedLeft = PredefinedUserGroups::contains(left.id);
            const auto predefinedRight = PredefinedUserGroups::contains(right.id);
            if (predefinedLeft != predefinedRight)
                return predefinedLeft;
            if (predefinedLeft)
                return left.id < right.id;

            // Sort according to type: local, ldap, cloud.
            if (left.type != right.type)
                return left.type < right.type;

            NX_ASSERT(left.id != right.id, "Duplicated group %1 (%2)", left.id, left.name);

            // "LDAP Default" goes in front of all LDAP groups.
            if (left.id == nx::vms::api::kDefaultLdapGroupId
                || right.id == nx::vms::api::kDefaultLdapGroupId)
            {
                return left.id == nx::vms::api::kDefaultLdapGroupId
                    && right.id != nx::vms::api::kDefaultLdapGroupId;
            }

            // Sort identical names by UUID.
            if (left.name == right.name)
                return left.id < right.id;

            // Case insensitive numeric-aware search.
            return collator(left.name, right.name);
        });

    for (const auto& group: groupsData)
        groups << group.name;

    for (const auto& providerResource: accessInfo.indirectProviders)
    {
        if (auto layout = providerResource.dynamicCast<QnLayoutResource>())
            layouts << layout->getName();
        else if (auto videoWall = providerResource.dynamicCast<QnVideoWallResource>())
            videoWalls << videoWall->getName();
    }

    std::sort(layouts.begin(), layouts.end(), collator);
    std::sort(videoWalls.begin(), videoWalls.end(), collator);

    const auto makeDescription =
        [this](const QString& single, const QString& plural, QStringList list)
        {
            for (auto& name: list)
                name = html::bold(name);

            return list.size() == 1
                ? nx::format(single, list.front()).toQString()
                : nx::format(plural, list.join(", ")).toQString();
        };

    const auto resourceGroupName =
        [this](const QnUuid& resourceGroupId) -> std::optional<QString>
        {
            const auto group = specialResourceGroup(resourceGroupId);
            if (!group)
                return {};

            switch (*group)
            {
                case SpecialResourceGroup::allDevices:
                    return tr("Cameras & Devices");

                case SpecialResourceGroup::allWebPages:
                    return tr("Web Pages & Integrations");

                case SpecialResourceGroup::allServers:
                    return tr("Health Monitors");

                case SpecialResourceGroup::allVideowalls:
                    return tr("Video Walls");
            }

            return {};
        };

    if (accessInfo.providedVia == ResourceAccessInfo::ProvidedVia::own)
    {
        const auto currentUser = context->systemContext()->resourcePool()->
            getResourceById<QnUserResource>(context->currentSubjectId());

        descriptions << (currentUser
            ? tr("User's custom permissions")
            : tr("Group's custom permissions"));
    }
    else if (auto nodeName = resourceGroupName(accessInfo.parentResourceGroupId))
    {
        descriptions << tr("Access granted by %1",
            "`%1` will be substituted with a resource group like `Cameras & Devices`")
                .arg(html::bold(*nodeName));
    }

    if (!layouts.empty())
    {
        descriptions << makeDescription(
            tr("Access granted by %1 layout"),
            tr("Access granted by %n layouts: %1", "", layouts.size()),
            layouts);
    }

    if (!videoWalls.empty())
    {
        descriptions << makeDescription(
            tr("Access granted by %1 video wall"),
            tr("Access granted by %n video walls: %1", "", videoWalls.size()),
            videoWalls);
    }

    if (!groups.empty())
    {
        descriptions << makeDescription(
            tr("Access granted by %1 group"),
            tr("Access granted by %n groups: %1", "", groups.size()),
            groups);
    }

    return descriptions.join("<br>");
}

AccessRights ResourceAccessRightsModel::Private::relevantAccessRights() const
{
    if (resource)
        return AccessSubjectEditingContext::relevantAccessRights(resource);

    if (const auto group = nx::vms::api::specialResourceGroup(q->groupId()))
        return AccessSubjectEditingContext::relevantAccessRights(*group);

    return {};
}

bool ResourceAccessRightsModel::Private::isEditable(int index) const
{
    return relevantAccessRights().testFlag(accessRightList[index]);
}

} // namespace nx::vms::client::desktop
