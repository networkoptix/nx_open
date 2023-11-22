// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_rights_model.h"

#include <algorithm>
#include <vector>

#include <QtCore/QCollator>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/format.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/core/qml/nx_globals_object.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/system_administration/models/members_sort.h>
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
        && checkedChildCount == other.checkedChildCount
        && checkedAndInheritedChildCount == other.checkedAndInheritedChildCount
        && totalChildCount == other.totalChildCount;
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

    QPersistentModelIndex resourceTreeIndex;
    ResourceAccessTreeItem item;

    QVector<ResourceAccessInfo> info;
    nx::utils::ScopedConnections contextConnections;

    void updateInfo(bool suppressSignals = false);
    QVector<ResourceAccessInfo> calculateInfo() const;
    std::vector<QnUuid> getGroupContents(const QnUuid& groupId) const;

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
                    if (resourceGroupIds.contains(d->item.target.groupId()))
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

QModelIndex ResourceAccessRightsModel::resourceTreeIndex() const
{
    return d->resourceTreeIndex;
}

void ResourceAccessRightsModel::setResourceTreeIndex(const QModelIndex& value)
{
    if (d->resourceTreeIndex == value)
        return;

    d->resourceTreeIndex = value;
    d->item = AccessSubjectEditingContext::resourceAccessTreeItemInfo(d->resourceTreeIndex);

    d->info.clear();
    d->updateInfo();

    emit resourceTreeIndexChanged();
}

ResourceAccessTreeItem::Type ResourceAccessRightsModel::rowType() const
{
    return d->item.type;
}

AccessRights ResourceAccessRightsModel::relevantAccessRights() const
{
    return d->item.relevantAccessRights;
}

QVariant ResourceAccessRightsModel::data(const QModelIndex& index, int role) const
{
    if (index.row() >= rowCount() || index.row() < 0)
        return {};

    switch (role)
    {
        case ProviderRole:
            return static_cast<int>(d->info[index.row()].providedVia);

        case InheritedFromRole:
            return static_cast<int>(d->info[index.row()].inheritedFrom);

        case TotalChildCountRole:
            return static_cast<int>(d->info[index.row()].totalChildCount);

        case CheckedChildCountRole:
            return static_cast<int>(d->info[index.row()].checkedChildCount);

        case CheckedAndInheritedChildCountRole:
            return static_cast<int>(d->info[index.row()].checkedAndInheritedChildCount);

        case AccessRightRole:
            return static_cast<int>(d->accessRightList[index.row()]);

        case EditableRole:
            return d->isEditable(index.row());

        case InheritanceInfoTextRole:
            return d->accessDetailsText(d->info[index.row()]);

        case DependentAccessRightsRole:
            return QVariant::fromValue(AccessSubjectEditingContext::dependentAccessRights(
                d->accessRightList[index.row()]) & d->item.relevantAccessRights);

        case RequiredAccessRightsRole:
            return QVariant::fromValue(AccessSubjectEditingContext::requiredAccessRights(
                d->accessRightList[index.row()]) & d->item.relevantAccessRights);

        default:
            return {};
    }
}

QHash<int, QByteArray> ResourceAccessRightsModel::roleNames() const
{
    auto names = base_type::roleNames();
    names.insert(ProviderRole, "providedVia");
    names.insert(InheritedFromRole, "inheritedFrom");
    names.insert(TotalChildCountRole, "totalChildCount");
    names.insert(CheckedChildCountRole, "checkedChildCount");
    names.insert(CheckedAndInheritedChildCountRole, "checkedAndInheritedChildCount");
    names.insert(AccessRightRole, "accessRight");
    names.insert(EditableRole, "editable");
    names.insert(InheritanceInfoTextRole, "inheritanceInfoText");
    names.insert(DependentAccessRightsRole, "dependentAccessRights");
    names.insert(RequiredAccessRightsRole, "requiredAccessRights");

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
    qRegisterMetaType<QVector<ResourceAccessTreeItem>>();
    qRegisterMetaType<QVector<QnUuid>>();
    qRegisterMetaType<QVector<QnResource*>>();

    qmlRegisterUncreatableType<ResourceAccessInfo>("nx.vms.client.desktop", 1, 0,
        "ResourceAccessInfo", "Cannot create an instance of ResourceAccessInfo");

    qmlRegisterUncreatableType<ResourceAccessTreeItem>("nx.vms.client.desktop", 1, 0,
        "ResourceAccessTreeItem", "Cannot create an instance of ResourceAccessTreeItem");

    qmlRegisterType<ResourceAccessRightsModel>(
        "nx.vms.client.desktop", 1, 0, "ResourceAccessRightsModel");
}

// ------------------------------------------------------------------------------------------------
// ResourceAccessRightsModel::Private

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
    const int count = accessRightList.size();
    QVector<ResourceAccessInfo> result(count);

    if (!context || context->currentSubjectId().isNull() || !resourceTreeIndex.isValid())
        return result;

    const auto childResources = AccessSubjectEditingContext::getChildResources(resourceTreeIndex);
    const auto accessMap = context->ownResourceAccessMap();

    for (int i = 0; i < count; ++i)
    {
        auto& newInfo = result[i];
        const auto accessRight = accessRightList[i];
        const bool checked = context->hasOwnAccessRight(item.target.id(), accessRight);
        const bool outerGroupChecked =
            accessMap.value(item.outerSpecialResourceGroupId).testFlag(accessRight);

        if (!item.relevantAccessRights.testFlag(accessRight))
            continue;

        newInfo.totalChildCount = childResources.size();

        newInfo.checkedChildCount = outerGroupChecked
            ? newInfo.totalChildCount
            : std::count_if(childResources.cbegin(), childResources.cend(),
                [&accessMap, accessRight](const QnResourcePtr& resource)
                {
                    return accessMap.value(resource->getId()).testFlag(accessRight);
                });

        newInfo.checkedAndInheritedChildCount = outerGroupChecked
            ? newInfo.totalChildCount
            : std::count_if(childResources.cbegin(), childResources.cend(),
                [this, accessRight](const QnResourcePtr& resource)
                {
                    return context->accessRights(resource).testFlag(accessRight);
                });

        if (item.target.group()) //< Special resource group.
        {
            newInfo.providerUserGroups = context->resourceGroupAccessProviders(
                item.target.id(), accessRight);

            // Keep arrays sorted for easy comparison.
            std::sort(newInfo.providerUserGroups.begin(), newInfo.providerUserGroups.end());

            if (!newInfo.providerUserGroups.empty())
                newInfo.inheritedFrom = ResourceAccessInfo::ProvidedVia::parentUserGroup;

            newInfo.providedVia = checked
                ? ResourceAccessInfo::ProvidedVia::own
                : newInfo.inheritedFrom;

            continue;
        }

        if (!item.target.resource()) //< Other grouping node.
            continue;

        const auto details = context->accessDetails(item.target.resource(), accessRight);
        if (details.contains(context->currentSubjectId()))
        {
            const auto providers = details.value(context->currentSubjectId());
            for (const auto& provider: providers)
            {
                if (!NX_ASSERT(provider))
                    continue;

                if (provider == item.target.resource())
                {
                    if (checked)
                    {
                        newInfo.providedVia = ResourceAccessInfo::ProvidedVia::own;
                    }
                    else if (NX_ASSERT(outerGroupChecked))
                    {
                        newInfo.providedVia = ResourceAccessInfo::ProvidedVia::ownResourceGroup;
                        newInfo.parentResourceGroupId = item.outerSpecialResourceGroupId;
                    }
                }
                else
                {
                    const auto providedVia = providerType(provider.get());
                    if (providerPriority(providedVia) > providerPriority(newInfo.inheritedFrom))
                        newInfo.inheritedFrom = providedVia;

                    // Keep arrays sorted for easy comparison.
                    const auto insertionPos = std::upper_bound(
                        newInfo.indirectProviders.begin(),
                        newInfo.indirectProviders.end(),
                        provider.get());

                    newInfo.indirectProviders.insert(insertionPos, provider);
                }
            }

            if (newInfo.inheritedFrom == ResourceAccessInfo::ProvidedVia::none
                && details.size() > 1)
            {
                newInfo.inheritedFrom = ResourceAccessInfo::ProvidedVia::parentUserGroup;
            }
        }
        else if (!details.empty())
        {
            newInfo.inheritedFrom = ResourceAccessInfo::ProvidedVia::parentUserGroup;
        }

        if (newInfo.providedVia == ResourceAccessInfo::ProvidedVia::none)
            newInfo.providedVia = newInfo.inheritedFrom;

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

std::vector<QnUuid> ResourceAccessRightsModel::Private::getGroupContents(
    const QnUuid& groupId) const
{
    // Only special resource groups are supported at this time.

    if (!context)
        return {};

    const auto groupResources = context->getGroupResources(groupId);
    if (groupResources.empty())
        return {};

    std::vector<QnUuid> result;
    result.reserve(groupResources.size());
    std::transform(groupResources.begin(), groupResources.end(), std::back_inserter(result),
        [](const QnResourcePtr& resource) { return resource->getId(); });
    return result;
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
        [](const auto& left, const auto& right)
        {
            return ComparableGroup(left) < ComparableGroup(right);
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
        [this](const QString& single, const QString& plural, QStringList list) -> QString
        {
            return list.size() == 1
                ? nx::format(single, html::bold(list.front()))
                : nx::format(plural, html::bold(list.front()));
        };

    const int numLines = (int) !layouts.empty() + (int) !videoWalls.empty() + (int) !groups.empty();
    const auto linePrefix = numLines > 1 ? QString(" - ") : QString{};

    if (!layouts.empty())
    {
        descriptions << (linePrefix + makeDescription(
            tr("%1 layout", "%1 will be substituted with a layout name"),
            tr("%1 and %n more layouts", "%1 will be substituted with a layout name",
                layouts.size() - 1),
            layouts));
    }

    if (!videoWalls.empty())
    {
        descriptions << (linePrefix + makeDescription(
            tr("%1 video wall", "%1 will be substituted with a video wall name"),
            tr("%1 and %n more video walls", "%1 will be substituted with a video wall name",
                videoWalls.size() - 1),
            videoWalls));
    }

    if (!groups.empty())
    {
        descriptions << (linePrefix + makeDescription(
            tr("%1 group", "%1 will be substituted with a user group name"),
            tr("%1 and %n more groups", "%1 will be substituted with a user group name",
                groups.size() - 1),
            groups));
    }

    return descriptions.join("<br>");
}

bool ResourceAccessRightsModel::Private::isEditable(int index) const
{
    return item.relevantAccessRights.testFlag(accessRightList[index]);
}

} // namespace nx::vms::client::desktop
