// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_edit_delegate.h"

#include <QtCore/QModelIndex>
#include <QtCore/QVariant>

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>

using namespace nx::vms::client;
using NodeType = nx::vms::client::desktop::ResourceTree::NodeType;
using ActionId = nx::vms::client::desktop::menu::IDType;
using ActionParameters = nx::vms::client::desktop::menu::Parameters;

namespace {

QnResourceList childIndexesResources(const QModelIndex& index, bool recursive)
{
    Qt::MatchFlags matchFlags = Qt::MatchExactly;
    if (recursive)
        matchFlags.setFlag(Qt::MatchRecursive);

    auto childResourceIndexes = index.model()->match(index.model()->index(0, 0, index), Qn::NodeTypeRole,
        QVariant::fromValue<NodeType>(NodeType::resource), -1, matchFlags);

    QnResourceList result;
    for (const auto& childResourceIndex: childResourceIndexes)
        result.append(childResourceIndex.data(core::ResourceRole).value<QnResourcePtr>());

    return result;
}

ActionId getActionId(NodeType nodeType)
{
    switch (nodeType)
    {
        case NodeType::edge:
        case NodeType::resource:
        case NodeType::sharedResource:
        case NodeType::sharedLayout:
        case NodeType::recorder:
            return ActionId::RenameResourceAction;

        case NodeType::videoWallItem:
        case NodeType::videoWallMatrix:
            return ActionId::RenameVideowallEntityAction;

        case NodeType::customResourceGroup:
            return ActionId::RenameCustomGroupAction;

        case NodeType::showreel:
            return ActionId::RenameShowreelAction;

        default:
            break;
    }

    NX_ASSERT(false, "Edit action is undefined for %1 node type", nodeType);
    return ActionId::NoAction;
}

ActionParameters getActionParameters(
    const QModelIndex& index,
    NodeType nodeType,
    QnResourcePool* resourcePool)
{
    if (nodeType == NodeType::resource
        || nodeType == NodeType::sharedResource
        || nodeType == NodeType::sharedLayout)
    {
        return ActionParameters(index.data(core::ResourceRole).value<QnResourcePtr>());
    }

    if (nodeType == NodeType::recorder)
    {
        const auto childResources = childIndexesResources(index, false);
        if (!childResources.isEmpty())
            return ActionParameters(childResources.first());
    }

    if (nodeType == NodeType::videoWallItem)
    {
        const auto uuid = index.data(core::UuidRole).value<nx::Uuid>();
        QnVideoWallItemIndex itemIndex = resourcePool->getVideoWallItemByUuid(uuid);
        if (!itemIndex.isNull())
        {
            ActionParameters parameters = {{itemIndex}};
            parameters.setArgument(core::UuidRole, uuid);
            return parameters;
        }
    }

    if (nodeType == NodeType::videoWallMatrix)
    {
        const auto uuid = index.data(core::UuidRole).value<nx::Uuid>();
        QnVideoWallMatrixIndex matrixIndex = resourcePool->getVideoWallMatrixByUuid(uuid);
        if (!matrixIndex.isNull())
        {
            ActionParameters parameters = {{matrixIndex}};
            parameters.setArgument(core::UuidRole, uuid);
            return parameters;
        }
    }

    if (nodeType == NodeType::customResourceGroup)
    {
        const auto childResources = childIndexesResources(index, true).filtered(
            [](const auto& resource) -> bool
            {
                return !resource.template dynamicCast<QnVirtualCameraResource>().isNull()
                    || resource->hasFlags(Qn::web_page)
                    || resource->hasFlags(Qn::layout);
            });

        if (!childResources.isEmpty())
        {
            const auto groupId = index.data(Qn::ResourceTreeCustomGroupIdRole).toString();
            ActionParameters parameters(childResources);
            parameters.setArgument(Qn::SelectedGroupIdsRole, QStringList({groupId}));
            parameters.setArgument(Qn::ResourceTreeCustomGroupIdRole, groupId);
            return parameters;
        }
    }

    if (nodeType == NodeType::showreel)
        return ActionParameters(core::UuidRole, index.data(core::UuidRole).value<nx::Uuid>());

    return ActionParameters();
}

} // namespace

namespace nx::vms::client::desktop {

TreeEditDelegate createResourceTreeEditDelegate(WindowContext* context)
{
    return
        [context](const QModelIndex& index, const QVariant& value)
        {
            if (!NX_ASSERT(index.isValid(), "Invalid model index"))
                return false;

            const auto stringValue = value.toString();
            if (!NX_ASSERT(!stringValue.isEmpty(), "Value with non-empty string representation expected"))
                return false;

            QVariant nodeTypeData = index.data(Qn::NodeTypeRole);
            if (!NX_ASSERT(!nodeTypeData.isNull(), "Model should provide node type"))
                return false;

            const auto nodeType = nodeTypeData.value<ResourceTree::NodeType>();

            ActionId actionId = getActionId(nodeType);
            if (actionId == menu::NoAction)
                return false;

            ActionParameters parameters = getActionParameters(
                index,
                nodeType,
                context->system()->resourcePool());
            parameters.setArgument(core::ResourceNameRole, stringValue);
            parameters.setArgument(Qn::NodeTypeRole, nodeType);

            // View state should be updated before triggering any actions if item editor was closed
            // by interaction with tree view.
            const auto actionManager = context->menu();
            QMetaObject::invokeMethod(
                actionManager,
                [actionManager, actionId, parameters] { actionManager->trigger(actionId, parameters); },
                Qt::QueuedConnection);

            return true;
        };
}

} // namespace nx::vms::client::desktop
