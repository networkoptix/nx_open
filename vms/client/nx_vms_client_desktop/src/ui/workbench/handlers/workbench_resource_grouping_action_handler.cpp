// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_resource_grouping_action_handler.h"

#include <QtGui/QAction>

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/resource_views/resource_tree_settings.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

ResourceGroupingActionHandler::ResourceGroupingActionHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(menu::CreateNewCustomGroupAction), &QAction::triggered,
        this, &ResourceGroupingActionHandler::createNewCustomResourceTreeGroup);

    connect(action(menu::AssignCustomGroupIdAction), &QAction::triggered,
        this, &ResourceGroupingActionHandler::assignCustomResourceTreeGroupId);

    connect(action(menu::RenameCustomGroupAction), &QAction::triggered,
        this, &ResourceGroupingActionHandler::renameCustomResourceTreeGroup);

    connect(action(menu::MoveToCustomGroupAction), &QAction::triggered,
        this, &ResourceGroupingActionHandler::moveToCustomResourceTreeGroup);

    connect(action(menu::RemoveCustomGroupAction), &QAction::triggered,
        this, &ResourceGroupingActionHandler::removeCustomResourceTreeGroup);
}

void ResourceGroupingActionHandler::createNewCustomResourceTreeGroup() const
{
    using namespace entity_resource_tree::resource_grouping;
    using NodeType = ResourceTree::NodeType;

    const auto parameters = menu()->currentParameters(sender());
    if (!NX_ASSERT(parameters.argument(Qn::OnlyResourceTreeSiblingsRole).toBool()))
        return;

    const auto resources = parameters.resources();
    const auto compositeGroupId = parameters.argument(Qn::ResourceTreeCustomGroupIdRole).toString();
    const auto newGroupSubId = getNewGroupSubId(resourcePool());
    const auto nodeTypeArgument = parameters.argument(Qn::NodeTypeRole);

    for (const auto& resource: resources)
    {
        if (compositeIdDimension(resourceCustomGroupId(resource)) >= kUserDefinedGroupingDepth)
        {
            showMaximumDepthWarning();
            return;
        }
    }

    QString newCompositeGroupId;

    int insertBefore = compositeIdDimension(compositeGroupId);
    if (nodeTypeArgument.value<NodeType>() == NodeType::customResourceGroup)
        newCompositeGroupId = replaceSubId(compositeGroupId, newGroupSubId, --insertBefore);
    else
        newCompositeGroupId = insertSubId(compositeGroupId, newGroupSubId, insertBefore);

    for (const auto& resource: resources)
    {
        auto resourceCompositeGroupId = resourceCustomGroupId(resource);
        const auto resourceGroupIdDimension = compositeIdDimension(resourceCompositeGroupId);

        if (resourceGroupIdDimension < insertBefore)
        {
            NX_ASSERT("Invalid parameter", false);
            continue;
        }

        resourceCompositeGroupId =
            insertSubId(resourceCompositeGroupId, newGroupSubId, insertBefore);
        setResourceCustomGroupId(resource, resourceCompositeGroupId);
    }

    for (const auto& resource: resources)
        resource->savePropertiesAsync();

    menu()->trigger(menu::NewCustomGroupCreatedEvent,
        menu::Parameters(Qn::ResourceTreeCustomGroupIdRole, newCompositeGroupId));
}

void ResourceGroupingActionHandler::assignCustomResourceTreeGroupId() const
{
    using namespace entity_resource_tree::resource_grouping;

    const auto parameters = menu()->currentParameters(sender());
    if (!parameters.hasArgument(Qn::ResourceTreeCustomGroupIdRole))
        return;

    const auto newGroupCompositeId =
        parameters.argument(Qn::ResourceTreeCustomGroupIdRole).toString();

    const QnResourceList resources = menu()->currentParameters(sender()).resources();

    for (const auto& resource: resources)
        setResourceCustomGroupId(resource, newGroupCompositeId);

    for (const auto& resource: resources)
        resource->savePropertiesAsync();

    menu()->trigger(menu::CustomGroupIdAssignedEvent);
}

void ResourceGroupingActionHandler::renameCustomResourceTreeGroup() const
{
    using namespace entity_resource_tree::resource_grouping;

    const auto parameters = menu()->currentParameters(sender());

    const auto resources = parameters.resources();
    const auto groupCompositeId = parameters.argument(Qn::ResourceTreeCustomGroupIdRole).toString();
    const auto groupCompositeIdDimension = compositeIdDimension(groupCompositeId);
    const auto changedSubIdOrder = groupCompositeIdDimension - 1;

    const auto newGroupName = fixupSubId(parameters.argument(core::ResourceNameRole).toString());
    if (!isValidSubId(newGroupName) || !NX_ASSERT(!resources.empty()))
        return;

    const auto newGroupCompositeId = replaceSubId(groupCompositeId, newGroupName, changedSubIdOrder);
    if (newGroupCompositeId.compare(groupCompositeId, Qt::CaseInsensitive) == 0)
        return;

    const bool serversShownInTree = context()->resourceTreeSettings()->showServersInTree();

    const auto allCameras = context()->resourcePool()->getAllCameras(
        serversShownInTree ? resources[0]->getParentResource() : QnResourcePtr());

    const auto it = std::find_if(allCameras.cbegin(), allCameras.cend(),
        [newGroupCompositeId, groupCompositeIdDimension](const QnVirtualCameraResourcePtr& resource)
        {
            const auto resourceGroupId = resourceCustomGroupId(resource);
            if (compositeIdDimension(resourceGroupId) < groupCompositeIdDimension)
                return false;

            return newGroupCompositeId.compare(
                trimCompositeId(resourceGroupId, groupCompositeIdDimension), Qt::CaseInsensitive) == 0;
        });

    const bool merging = it != allCameras.cend();
    if (merging && !ui::messages::Resources::mergeResourceGroups(mainWindowWidget(), newGroupName))
    {
        menu()->trigger(menu::EditResourceInTreeAction);
        return;
    }

    for (const auto& resource: resources)
    {
        const auto groupCompositeId = resourceCustomGroupId(resource);
        const auto newGroupCompositeId =
            replaceSubId(groupCompositeId, newGroupName, changedSubIdOrder);

        if (groupCompositeId.compare(newGroupCompositeId, Qt::CaseSensitive) == 0)
            continue;

        setResourceCustomGroupId(resource, newGroupCompositeId);
    }

    for (const auto& resource: resources)
        resource->savePropertiesAsync();

    menu()->trigger(menu::CustomGroupRenamedEvent,
        menu::Parameters(Qn::TargetResourceTreeCustomGroupIdRole, newGroupCompositeId));
}

void ResourceGroupingActionHandler::moveToCustomResourceTreeGroup() const
{
    using namespace entity_resource_tree::resource_grouping;

    const auto parameters = menu()->currentParameters(sender());
    if (!parameters.hasArgument(Qn::ResourceTreeCustomGroupIdRole))
        return;

    const auto sourceGroupId = parameters.argument(Qn::ResourceTreeCustomGroupIdRole).toString();
    const auto destinationGroupId =
        parameters.argument(Qn::TargetResourceTreeCustomGroupIdRole).toString();

    const auto resources = parameters.resources();

    std::vector<QString> resourcesCompositeIds;
    for (const auto& resource: resources)
        resourcesCompositeIds.push_back(resourceCustomGroupId(resource));

    const auto commonLeadingPartDimension = compositeIdDimension(sourceGroupId);
    if (commonLeadingPartDimension > 0)
    {
        for (auto& compositeId: resourcesCompositeIds)
            cutCompositeIdFront(compositeId, commonLeadingPartDimension - 1);
    }

    const auto destinationIdDimension = compositeIdDimension(destinationGroupId);

    for (const auto& id: resourcesCompositeIds)
    {
        if (destinationIdDimension + compositeIdDimension(id) >= kUserDefinedGroupingDepth)
        {
            showMaximumDepthWarning();
            return;
        }
    }

    for (int i = 0; i < resources.size(); ++i)
    {
        setResourceCustomGroupId(
            resources[i], appendCompositeId(destinationGroupId, resourcesCompositeIds.at(i)));
    }

    for (const auto& resource: resources)
        resource->savePropertiesAsync();

    menu()->trigger(menu::MovedToCustomGroupEvent);
}

void ResourceGroupingActionHandler::removeCustomResourceTreeGroup() const
{
    using namespace entity_resource_tree::resource_grouping;

    const auto parameters = menu()->currentParameters(sender());
    const auto resources = parameters.resources();

    const auto currentGroupId = parameters.argument(Qn::ResourceTreeCustomGroupIdRole).toString();
    const auto currentGroupIdDimension = compositeIdDimension(currentGroupId);

    if (!NX_ASSERT(!currentGroupId.isEmpty(), "Invalid parameter"))
        return;

    QnResourceList modifiedResources;
    for (const auto& resource: resources)
    {
        const auto resourceGroupId = resourceCustomGroupId(resource);
        if (trimCompositeId(resourceGroupId, currentGroupIdDimension)
            .compare(currentGroupId, Qt::CaseInsensitive) != 0)
        {
            continue;
        }

        modifiedResources.push_back(resource);
        const auto newResourceGroupId = removeSubId(resourceGroupId, currentGroupIdDimension - 1);
        setResourceCustomGroupId(resource, newResourceGroupId);
    }

    for (const auto& modifiedResource: modifiedResources)
        modifiedResource->savePropertiesAsync();

    menu()->trigger(menu::CustomGroupRemovedEvent);
}

void ResourceGroupingActionHandler::showMaximumDepthWarning() const
{
    SceneBanner::show(tr("Maximum level of nesting is reached"));
}

} // namespace nx::vms::client::desktop
