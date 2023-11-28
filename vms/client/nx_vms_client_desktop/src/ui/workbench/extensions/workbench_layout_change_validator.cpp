// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout_change_validator.h"

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_matrix.h>
#include <core/resource/videowall_matrix_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/common/utils/videowall_layout_watcher.h>
#include <ui/workbench/workbench_context.h>

QnWorkbenchLayoutsChangeValidator::QnWorkbenchLayoutsChangeValidator(QnWorkbenchContext* context):
    QnWorkbenchContextAware(context)
{
}

bool QnWorkbenchLayoutsChangeValidator::confirmChangeVideoWallLayout(
    const QnLayoutResourcePtr& layout,
    const QnResourceList& removedResources) const
{
    if (!NX_ASSERT(accessController()->user()))
        return false;

    if (!NX_ASSERT(accessController()->user()) || accessController()->hasPowerUserPermissions())
        return true;

    const auto videowall = layout->getParentResource().objectCast<QnVideoWallResource>();

    if (!NX_ASSERT(videowall && videowall->systemContext() == systemContext()))
        return false;

    const auto videowallLayoutWatcher =
        nx::vms::common::VideowallLayoutWatcher::instance(resourcePool());

    auto otherLayoutIds = videowallLayoutWatcher->videowallLayouts(videowall);
    otherLayoutIds.remove(layout->getId());

    const auto otherLayouts =
        resourcePool()->getResourcesByIds<QnLayoutResource>(otherLayoutIds.keys());

    QSet<QnUuid> resourcesOnOtherLayouts;
    for (const auto otherLayout: otherLayouts)
        addLayoutResourceIds(otherLayout, resourcesOnOtherLayouts);

    const auto losingAccessToResources = removedResources.filtered(
        [this, resourcesOnOtherLayouts, providers = QSet<QnResourcePtr>({videowall})](
            const QnResourcePtr& resource) -> bool
        {
            return !resourcesOnOtherLayouts.contains(resource->getId())
                && isAccessibleBySpecifiedProvidersOnly(resource, providers);
        });

    if (losingAccessToResources.isEmpty())
        return true;

    return nx::vms::client::desktop::ui::messages::Resources::changeVideoWallLayout(
        mainWindowWidget(), losingAccessToResources);
}

bool QnWorkbenchLayoutsChangeValidator::confirmLoadVideoWallMatrix(
    const QnVideoWallMatrixIndex& matrixIndex) const
{
    if (!NX_ASSERT(accessController()->user()) || !NX_ASSERT(matrixIndex.isValid()))
        return false;

    if (accessController()->hasPowerUserPermissions())
        return true;

    const auto videowall = matrixIndex.videowall();
    const auto matrix = matrixIndex.matrix();

    if (!NX_ASSERT(videowall->systemContext() == systemContext()))
        return false;

    const auto videowallLayoutWatcher =
        nx::vms::common::VideowallLayoutWatcher::instance(resourcePool());

    auto otherLayoutIds = videowallLayoutWatcher->videowallLayouts(videowall);

    QSet<QnUuid> removedResourceIds;
    for (const auto& item: videowall->items()->getItems())
    {
        if (item.layout.isNull() || !matrix.layoutByItem.contains(item.uuid))
            continue;

        otherLayoutIds.remove(item.layout);

        if (const auto layout = resourcePool()->getResourceById<QnLayoutResource>(item.layout))
            addLayoutResourceIds(layout, removedResourceIds);
    }

    const auto otherLayouts =
        resourcePool()->getResourcesByIds<QnLayoutResource>(otherLayoutIds.keys());

    QSet<QnUuid> resourcesOnOtherLayouts;
    for (const auto otherLayout: otherLayouts)
        addLayoutResourceIds(otherLayout, resourcesOnOtherLayouts);

    removedResourceIds -= resourcesOnOtherLayouts;
    const auto removedResources = resourcePool()->getResourcesByIds(removedResourceIds);

    const auto losingAccessToResources = removedResources.filtered(
        [this, providers = QSet<QnResourcePtr>({videowall})](const QnResourcePtr& resource) -> bool
        {
            return isAccessibleBySpecifiedProvidersOnly(resource, providers);
        });

    if (losingAccessToResources.isEmpty())
        return true;

    return nx::vms::client::desktop::ui::messages::Resources::changeVideoWallLayout(
        mainWindowWidget(), losingAccessToResources);
}

bool QnWorkbenchLayoutsChangeValidator::confirmDeleteVideoWallMatrices(
    const QnVideoWallMatrixIndexList& matrixIndexes) const
{
    if (!NX_ASSERT(accessController()->user())
        || !NX_ASSERT(std::all_of(matrixIndexes.cbegin(), matrixIndexes.cend(),
            [this](const QnVideoWallMatrixIndex& index)
            {
                return index.isValid() && index.videowall()->systemContext() == systemContext();
            })))
    {
        return false;
    }

    if (accessController()->hasPowerUserPermissions())
        return true;

    const auto videowallLayoutWatcher =
        nx::vms::common::VideowallLayoutWatcher::instance(resourcePool());

    QHash<QnVideoWallResourcePtr, QList<QnVideoWallMatrix>> matricesByVideowall;
    for (const auto& index: matrixIndexes)
        matricesByVideowall[index.videowall()].push_back(index.matrix());

    QHash<QnResourcePtr /*resource*/, QSet<QnResourcePtr /*videowall*/>> removedResources;

    for (const auto& [videowall, matrices]: nx::utils::constKeyValueRange(matricesByVideowall))
    {
        auto otherLayoutIds = videowallLayoutWatcher->videowallLayouts(videowall);
        QSet<QnUuid> removedResourceIds;

        for (const auto& item: videowall->items()->getItems())
        {
            for (const auto& matrix: matrices)
            {
                const auto layoutId = matrix.layoutByItem.value(item.uuid);
                if (layoutId.isNull())
                    continue;

                otherLayoutIds.remove(layoutId);

                if (const auto layout = resourcePool()->getResourceById<QnLayoutResource>(layoutId))
                    addLayoutResourceIds(layout, removedResourceIds);
            }
        }

        const auto otherLayouts =
            resourcePool()->getResourcesByIds<QnLayoutResource>(otherLayoutIds.keys());

        QSet<QnUuid> resourcesOnOtherLayouts;
        for (const auto otherLayout: otherLayouts)
            addLayoutResourceIds(otherLayout, resourcesOnOtherLayouts);

        removedResourceIds -= resourcesOnOtherLayouts;
        const auto resources = resourcePool()->getResourcesByIds(removedResourceIds);

        for (const auto& resource: resources)
            removedResources[resource].insert(videowall);
    }

    QnResourceList losingAccessToResources;

    for (const auto& [resource, videowalls]: nx::utils::constKeyValueRange(removedResources))
    {
        if (isAccessibleBySpecifiedProvidersOnly(resource, videowalls))
            losingAccessToResources.push_back(resource);
    }

    if (losingAccessToResources.isEmpty())
        return true;

    return nx::vms::client::desktop::ui::messages::Resources::changeVideoWallLayout(
        mainWindowWidget(), losingAccessToResources);
}

void QnWorkbenchLayoutsChangeValidator::addLayoutResourceIds(
    const QnLayoutResourcePtr& layout, QSet<QnUuid>& resourceIds) const
{
    for (const auto& item: layout->getItems())
    {
        if (!item.resource.id.isNull())
            resourceIds.insert(item.resource.id);
    }
}

bool QnWorkbenchLayoutsChangeValidator::isAccessibleBySpecifiedProvidersOnly(
    const QnResourcePtr& resource, const QSet<QnResourcePtr>& specificProviders) const
{
    const auto details = resourceAccessManager()->accessDetails(
        accessController()->user()->getId(), resource, nx::vms::api::AccessRight::view);

    for (const auto& providers: details)
    {
        const int count = providers.size();

        if (count != specificProviders.size())
            return false;

        if (count == 1 && *providers.cbegin() != *specificProviders.cbegin())
            return false;

        if (count > 1 && nx::utils::toQSet(providers) != specificProviders)
            return false;
    }

    return true;
}
