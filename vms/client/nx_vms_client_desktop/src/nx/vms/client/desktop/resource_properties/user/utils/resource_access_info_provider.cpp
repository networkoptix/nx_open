// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_info_provider.h"
#include <algorithm>

#include <QtCore/QPointer>
#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>

#include "access_subject_editing_context.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

// ------------------------------------------------------------------------------------------------
// ResourceAccessInfo

bool ResourceAccessInfo::operator==(const ResourceAccessInfo& other) const
{
    return providedVia == other.providedVia
        && providerGroups == other.providerGroups
        && indirectProviders == other.indirectProviders;
}

bool ResourceAccessInfo::operator!=(const ResourceAccessInfo& other) const
{
    return !(*this == other);
}

// ------------------------------------------------------------------------------------------------
// ResourceAccessInfoProvider

struct ResourceAccessInfoProvider::Private
{
    ResourceAccessInfoProvider* const q;

    QPointer<AccessSubjectEditingContext> context;
    QVector<AccessRight> accessRightList;
    QnResourcePtr resource;
    ResourceTree::NodeType nodeType = ResourceTree::NodeType::spacer;
    bool collapsed = false;
    QVector<ResourceAccessInfo> info;

    nx::utils::ScopedConnections contextConnections;

    void updateInfo();
    QnResourceList getResources() const;
};

ResourceAccessInfoProvider::ResourceAccessInfoProvider(QObject* parent):
    base_type(parent),
    d(new Private{this})
{
}

ResourceAccessInfoProvider::~ResourceAccessInfoProvider()
{
    // Required here for forward-declared scoped pointer destruction.
}

AccessSubjectEditingContext* ResourceAccessInfoProvider::context() const
{
    return d->context;
}

void ResourceAccessInfoProvider::setContext(AccessSubjectEditingContext* value)
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
    }

    emit contextChanged();
}

QVector<AccessRight> ResourceAccessInfoProvider::accessRightsList() const
{
    return d->accessRightList;
}

void ResourceAccessInfoProvider::setAccessRightsList(const QVector<AccessRight>& value)
{
    if (d->accessRightList == value)
        return;

    const int diff = value.size() - d->accessRightList.size();

    if (diff > 0)
        beginInsertRows({}, d->accessRightList.size(), d->accessRightList.size() + diff - 1);
    else if (diff < 0)
        beginRemoveRows({}, d->accessRightList.size() + diff, d->accessRightList.size() - 1);

    d->accessRightList = value;

    emit accessRightsListChanged();

    if (diff > 0)
        endInsertRows();
    else if (diff < 0)
        endRemoveRows();

    d->updateInfo();
}

QnResource* ResourceAccessInfoProvider::resource() const
{
    return d->resource.get();
}

void ResourceAccessInfoProvider::setResource(QnResource* value)
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

ResourceTree::NodeType ResourceAccessInfoProvider::nodeType() const
{
    return d->nodeType;
}

void ResourceAccessInfoProvider::setNodeType(ResourceTree::NodeType value)
{
    if (d->nodeType == value)
        return;

    d->nodeType = value;

    emit nodeTypeChanged();
}

bool ResourceAccessInfoProvider::collapsed() const
{
    return d->collapsed;
}

void ResourceAccessInfoProvider::setCollapsed(bool value)
{
    if (d->collapsed == value)
        return;

    d->collapsed = value;
    emit collapsedChanged();

    d->updateInfo();
}

QString ResourceAccessInfoProvider::accessDetailsText(
    const QnUuid& resourceId,
    nx::vms::api::AccessRight accessRight) const
{
    using namespace nx::vms::common;

    if (!d->resource)
        return {};

    const auto resourcePool = d->resource->systemContext()->resourcePool();
    const auto resource = resourcePool->getResourceById(resourceId);
    if (!resource)
        return {};

    const auto details = d->context->accessDetails(resource, accessRight);

    if (details.isEmpty())
        return {};

    QStringList descriptions;

    for (const auto& id: details.keys())
    {
        if (id == d->context->currentSubjectId())
        {
            for (const auto& sharedResource: details.value(id))
            {
                if (sharedResource->getId() == resourceId)
                {
                    descriptions << tr("Direct access");
                }
                else if (auto layout = sharedResource.dynamicCast<QnLayoutResource>())
                {
                    descriptions << tr("Access granted by %1 layout").arg(
                        html::bold(layout->getName()));
                }
                else if (auto videoWall = sharedResource.dynamicCast<QnVideoWallResource>())
                {
                    descriptions << tr("Access granted by %1 video wall").arg(
                        html::bold(videoWall->getName()));
                }
            }
        }
        else if (const auto group = d->resource->systemContext()->userRolesManager()->userRole(id);
            !group.name.isEmpty())
        {
            descriptions << tr("Access granted by %1 group").arg(
                html::bold(group.name));
        }
    }

    return descriptions.join("<br>");
}

QString ResourceAccessInfoProvider::accessDetailsText(const ResourceAccessInfo& accessInfo) const
{
    using namespace nx::vms::common;

    if (!d->resource)
        return {};

    QStringList descriptions;

    for (const auto& groupId: accessInfo.providerGroups)
    {
        const auto group = d->resource->systemContext()->userRolesManager()->userRole(groupId);
        if (!group.name.isEmpty())
            descriptions << tr("Access granted by %1 group").arg(html::bold(group.name));
    }

    for (const auto& providerResource: accessInfo.indirectProviders)
    {
        if (auto layout = providerResource.dynamicCast<QnLayoutResource>())
        {
            descriptions << tr("Access granted by %1 layout").arg(
                html::bold(layout->getName()));
        }
        else if (auto videoWall = providerResource.dynamicCast<QnVideoWallResource>())
        {
            descriptions << tr("Access granted by %1 video wall").arg(
                html::bold(videoWall->getName()));
        }
    }

    return descriptions.join("<br>");
}

QVariant ResourceAccessInfoProvider::data(const QModelIndex& index, int role) const
{
    if (index.row() >= rowCount() || index.row() < 0)
        return {};

    switch (role)
    {
        case providedVia:
            return static_cast<int>(d->info.at(index.row()).providedVia);
        case isOwn:
            return d->info.at(index.row()).providedVia == ResourceAccessInfo::ProvidedVia::own;
        case Qt::ToolTipRole:
            return accessDetailsText(d->info.at(index.row()));
        default:
            return {};
    }
}

bool ResourceAccessInfoProvider::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (index.row() >= rowCount() || index.row() < 0 || role != isOwn || !d->context)
        return false;

    if (value.toBool() == data(index, isOwn).toBool())
        return false;

    const auto accessRight = d->accessRightList.at(index.row());

    if (d->resource)
        d->context->setOwnAccessRight(d->resource->getId(), accessRight, value.toBool());
    else if (d->collapsed)
        d->context->setOwnAccessRight({}, accessRight, value.toBool());

    d->info[index.row()].providedVia = value.toBool()
        ? ResourceAccessInfo::ProvidedVia::own
        : ResourceAccessInfo::ProvidedVia::none;

    return true;
}

void ResourceAccessInfoProvider::toggleAll()
{
    if (!d->resource)
        return;

    auto accessMap = d->context->ownResourceAccessMap();
    auto accessRights = accessMap.value(d->resource->getId(), {});

    const bool allSet = std::all_of(d->accessRightList.cbegin(), d->accessRightList.cend(),
        [accessRights](auto accessRight)
        {
            return accessRights.testFlag(accessRight);
        });

    for (auto accessRight: d->accessRightList)
        accessRights.setFlag(accessRight, !allSet);

    if (accessRights == 0)
        accessMap.remove(d->resource->getId());
    else
        accessMap[d->resource->getId()] = accessRights;

    d->context->setOwnResourceAccessMap(accessMap);
}

QHash<int, QByteArray> ResourceAccessInfoProvider::roleNames() const
{
    auto names = base_type::roleNames();

    names.insert(providedVia, "providedVia");
    names.insert(isOwn, "isOwn");
    return names;
}

int ResourceAccessInfoProvider::rowCount(const QModelIndex&) const
{
    return d->accessRightList.size();
}

QVector<ResourceAccessInfo> ResourceAccessInfoProvider::info() const
{
    return d->info;
}

ResourceAccessInfo::ProvidedVia ResourceAccessInfoProvider::providerType(QnResource* provider)
{
    if (qobject_cast<QnLayoutResource*>(provider))
        return ResourceAccessInfo::ProvidedVia::layout;

    if (qobject_cast<QnVideoWallResource*>(provider))
        return ResourceAccessInfo::ProvidedVia::videowall;

    NX_ASSERT(false, "Unknown indirect provider type.");
    return ResourceAccessInfo::ProvidedVia::unknown;
}

void ResourceAccessInfoProvider::registerQmlTypes()
{
    qRegisterMetaType<QVector<AccessRight>>();
    qRegisterMetaType<QVector<ResourceAccessInfo>>();
    qRegisterMetaType<QVector<QnUuid>>();
    qRegisterMetaType<QVector<QnResource*>>();

    qmlRegisterUncreatableType<ResourceAccessInfo>("nx.vms.client.desktop", 1, 0,
        "ResourceAccessInfo", "Cannot create an instance of ResourceAccessInfo");

    qmlRegisterType<ResourceAccessInfoProvider>(
        "nx.vms.client.desktop", 1, 0, "ResourceAccessInfoProvider");
}

// ------------------------------------------------------------------------------------------------
// ResourceAccessInfoProvider::Private

QnResourceList ResourceAccessInfoProvider::Private::getResources() const
{
    if (!context)
        return {};

    const auto resourcePool = context->systemContext()->resourcePool();

    switch (nodeType)
    {
        case ResourceTree::NodeType::camerasAndDevices:
            return resourcePool->getAllCameras(QnUuid{}, /*ignoreDesktopCameras*/ true);

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

void ResourceAccessInfoProvider::Private::updateInfo()
{
    QVector<ResourceAccessInfo> newResourceAccessInfo;

    const int count = accessRightList.size();
    newResourceAccessInfo.resize(count);

    if (!resource && context)
    {
        const auto accessMap = context->ownResourceAccessMap();

        nx::vms::api::AccessRights rights;

        if (collapsed)
        {
            rights = accessMap.value({});
        }
        else
        {
            QnResourceList resources = getResources();

            if (!resources.empty())
            {
                rights = accessMap.value(resources.first()->getId());

                for (const auto& resource: resources)
                    rights &= accessMap.value(resource->getId());
            }
        }

        for (int i = 0; i < count; ++i)
        {
            auto& newInfo = newResourceAccessInfo[i];
            newInfo.providedVia = rights.testFlag(accessRightList.at(i))
                ? ResourceAccessInfo::ProvidedVia::own
                : ResourceAccessInfo::ProvidedVia::none;
        }
    }

    if (context && resource && !context->currentSubjectId().isNull())
    {
        for (int i = 0; i < count; ++i)
        {
            auto& newInfo = newResourceAccessInfo[i];
            const auto details = context->accessDetails(resource, accessRightList[i]);

            if (details.empty())
            {
                newInfo.providedVia = ResourceAccessInfo::ProvidedVia::none;
            }
            else if (details.contains(context->currentSubjectId()))
            {
                const auto providers = details.value(context->currentSubjectId());
                if (providers.contains(resource))
                {
                    newInfo.providedVia = ResourceAccessInfo::ProvidedVia::own;
                }
                else
                {
                    for (const auto& provider: providers)
                    {
                        if (!NX_ASSERT(provider))
                            continue;

                        static const auto priorityKey =
                            [](ResourceAccessInfo::ProvidedVia providerType)
                            {
                                switch (providerType)
                                {
                                    case ResourceAccessInfo::ProvidedVia::videowall:
                                        return 1;
                                    case ResourceAccessInfo::ProvidedVia::layout:
                                        return 2;
                                    default:
                                        return 0;
                                }
                            };

                        const auto providedVia = providerType(provider.get());
                        if (priorityKey(providedVia) > priorityKey(newInfo.providedVia))
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
            else
            {
                newInfo.providedVia = ResourceAccessInfo::ProvidedVia::parent;

                // Show only the direct parents which provide current access rights.
                const auto directParents =
                    context->subjectHierarchy()->directParents(context->currentSubjectId());

                const QSet<QnUuid> providerIds{details.keyBegin(), details.keyEnd()};

                newInfo.providerGroups = {};
                for (const auto& key: directParents)
                {
                    if (providerIds.contains(key)
                        || context->subjectHierarchy()->isRecursiveMember(key, providerIds))
                    {
                        newInfo.providerGroups << key;
                    }
                }

                // Keep arrays sorted for easy comparison.
                std::sort(newInfo.providerGroups.begin(), newInfo.providerGroups.end());
            }
        }
    }

    if (newResourceAccessInfo == info)
        return;

    info = newResourceAccessInfo;
    emit q->infoChanged();

    if (info.size() > 0)
    {
        emit q->dataChanged(
            q->index(0, 0),
            q->index(info.size() - 1, 0));
    }
}

} // namespace nx::vms::client::desktop
