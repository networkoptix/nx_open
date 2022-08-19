// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_interaction_handler.h"

#include <memory>

#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>

#include <client/client_runtime_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_item_index.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/build_info.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/models/filter_proxy_model.h>
#include <nx/vms/client/desktop/common/models/item_model_algorithm.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

namespace {

template<class ResourceType>
QnSharedResourcePointer<ResourceType> getResource(const QModelIndex& index)
{
    return index.data(Qn::ResourceRole).value<QnResourcePtr>().objectCast<ResourceType>();
}

QnResourceList selectedResources(const QModelIndexList& selection)
{
    QnResourceList result;
    QSet<QnResourcePtr> addedResources;

    const auto addResource =
        [&](const QnResourcePtr& resource)
        {
            if (!resource || addedResources.contains(resource))
                return;

            addedResources.insert(resource);
            result.push_back(resource->toSharedPointer());
        };

    for (const QModelIndex& modelIndex: selection)
    {
        const auto nodeType = modelIndex.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
        switch (nodeType)
        {
            case ResourceTree::NodeType::recorder:
            {
                // TODO: #vkutin #vbreus #sivanov This should be refactored to using a new data
                // role to avoid child index access / model structure dependency.

                const auto model = modelIndex.model();
                for (int i = 0; i < model->rowCount(modelIndex); ++i)
                {
                    const auto subIndex = model->index(i, 0, modelIndex);
                    addResource(subIndex.data(Qn::ResourceRole).value<QnResourcePtr>());
                }

                break;
            }

            case ResourceTree::NodeType::resource:
            case ResourceTree::NodeType::sharedLayout:
            case ResourceTree::NodeType::sharedResource:
            case ResourceTree::NodeType::edge:
            case ResourceTree::NodeType::currentUser:
                addResource(modelIndex.data(Qn::ResourceRole).value<QnResourcePtr>());
                break;

            default:
                break;
        }
    }

    return result;
}

QnResourceList childCameras(const QModelIndex& index)
{
    if (!index.isValid())
    {
        NX_ASSERT(false, "Invalid parameter");
        return {};
    }

    const auto model = index.model();
    QnResourceList result;

    for (int row = 0; row < model->rowCount(index); ++row)
    {
        const auto childIndex = model->index(row, 0, index);
        const auto childIndexResource = childIndex.data(Qn::ResourceRole).value<QnResourcePtr>();

        if (childIndexResource.dynamicCast<QnVirtualCameraResource>()
            || childIndexResource->hasFlags(Qn::web_page))
        {
            result.append(childIndexResource);
        }
    }

    return result;
}

QnResourceList childCamerasRecursive(const QModelIndex& index)
{
    if (!index.isValid())
    {
        NX_ASSERT(false, "Invalid parameter");
        return {};
    }

    std::function<QModelIndexList(const QModelIndex&)> getChildren;
    getChildren =
        [&getChildren](const QModelIndex& parent)
        {
            QModelIndexList result;
            for (int row = 0; row < parent.model()->rowCount(parent); ++row)
            {
                const auto childIndex = parent.model()->index(row, 0, parent);
                result.append(childIndex);
                result.append(getChildren(childIndex));
            }
            return result;
        };

    const auto children = getChildren(index);

    QnResourceList result;
    for (const auto& child: children)
    {
        const auto resourceData = child.data(Qn::ResourceRole);
        if (resourceData.isNull())
            continue;

        const auto resource = resourceData.value<QnResourcePtr>();
        if (resource.dynamicCast<QnVirtualCameraResource>()
            || resource.dynamicCast<QnWebPageResource>())
        {
            result.push_back(resource);
        }
    }

    return result;
}

QnLayoutItemIndexList selectedLayoutItems(const QModelIndexList& selection)
{
    QnLayoutItemIndexList result;

    for (const auto& modelIndex: selection)
    {
        const auto id = modelIndex.data(Qn::ItemUuidRole).value<QnUuid>();
        if (id.isNull())
            continue;

        const auto layout = getResource<QnLayoutResource>(modelIndex.parent());
        if (layout)
            result.push_back(QnLayoutItemIndex(layout, id));
    }

    return result;
}

QnVideoWallItemIndexList selectedVideoWallItems(
    const QModelIndexList& selection, QnResourcePool* resourcePool)
{
    QnVideoWallItemIndexList result;

    for (const auto& modelIndex: selection)
    {
        const auto id = modelIndex.data(Qn::ItemUuidRole).value<QnUuid>();
        if (id.isNull())
            continue;

        const auto videowallItemIndex = resourcePool->getVideoWallItemByUuid(id);
        if (!videowallItemIndex.isNull())
            result.push_back(videowallItemIndex);
    }

    return result;
}

QnVideoWallMatrixIndexList selectedVideoWallMatrices(
    const QModelIndexList& selection, QnResourcePool* resourcePool)
{
    QnVideoWallMatrixIndexList result;

    for (const auto& modelIndex: selection)
    {
        const auto id = modelIndex.data(Qn::ItemUuidRole).value<QnUuid>();
        if (id.isNull())
            continue;

        const auto videowallMatrixIndex = resourcePool->getVideoWallMatrixByUuid(id);
        if (!videowallMatrixIndex.isNull())
            result.push_back(videowallMatrixIndex);
    }

    return result;
}

QVariant parentNodeType(const QModelIndex& index)
{
    if (!index.isValid())
        return {};

    if (!index.parent().isValid())
        return QVariant::fromValue<ResourceTree::NodeType>(ResourceTree::NodeType::root);

    return index.parent().data(Qn::NodeTypeRole);
}

QVariant topLevelParentNodeType(const QModelIndex& index)
{
    if (!index.isValid() || !index.parent().isValid())
        return {};

    QModelIndex parent = index.parent();
    while (parent.parent().isValid())
        parent = parent.parent();

    return parent.data(Qn::NodeTypeRole);
}

} // namespace

struct ResourceTreeInteractionHandler::Private
{
    using NodeType = ResourceTree::NodeType;

    const QPointer<QnWorkbenchContext> context;
    const std::unique_ptr<QAction> renameAction{new QAction{}};

    ui::action::Parameters actionParameters(
        const QModelIndex& index, const QModelIndexList& selection) const
    {
        if (!NX_ASSERT(context))
            return {};

        if (!index.isValid())
            return ui::action::Parameters{Qn::NodeTypeRole, ResourceTree::NodeType::root};

        // TODO: #sivanov #vkutin #vbreus Refactor to a simple switch by node type.

        const auto nodeType = index.data(Qn::NodeTypeRole).value<NodeType>();

        const auto withNodeType =
            [nodeType](ui::action::Parameters parameters)
            {
                return parameters.withArgument(Qn::NodeTypeRole, nodeType);
            };

        switch (nodeType)
        {
            case NodeType::videoWallItem:
                return withNodeType(selectedVideoWallItems(selection, context->resourcePool()));

            case NodeType::videoWallMatrix:
                return withNodeType(selectedVideoWallMatrices(selection, context->resourcePool()));

            case NodeType::layoutItem:
                return withNodeType(selectedLayoutItems(selection));

            case NodeType::cloudSystem:
            {
                const auto systemId = index.data(Qn::CloudSystemIdRole).toString();
                CloudSystemConnectData connectData{systemId, ConnectScenario::connectFromTree};
                const auto parameters = ui::action::Parameters()
                    .withArgument(Qn::CloudSystemIdRole, systemId)
                    .withArgument(Qn::CloudSystemConnectDataRole, connectData);

                return withNodeType(parameters);
            }

            default:
                break;
        }

        ui::action::Parameters result(selectedResources(selection));

        // For working with shared layout links we must know owning user resource.
        const auto parentIndex = index.parent();
        const auto parentIndexNodeType = parentNodeType(index).value<ResourceTree::NodeType>();

        // We can select several layouts and some other resources in any part of tree -
        // in this case just do not set anything.
        QnUserResourcePtr user;
        auto uuid = index.data(Qn::UuidRole).value<QnUuid>();

        switch (nodeType)
        {
            case NodeType::sharedLayouts:
                user = getResource<QnUserResource>(parentIndex);
                uuid = parentIndex.data(Qn::UuidRole).value<QnUuid>();
                break;

            default:
                break;
        }

        switch (parentIndexNodeType)
        {
            case NodeType::layouts:
                user = context->user();
                break;

            case NodeType::sharedResources:
            case NodeType::sharedLayouts:
                user = getResource<QnUserResource>(parentIndex.parent());
                uuid = parentIndex.parent().data(Qn::UuidRole).value<QnUuid>();
                break;

            case NodeType::resource:
                user = getResource<QnUserResource>(parentIndex);
                break;

            default:
                break;
        }

        if (user)
            result.setArgument(Qn::UserResourceRole, user);

        if (!uuid.isNull())
            result.setArgument(Qn::UuidRole, uuid);

        result.setArgument(Qn::NodeTypeRole, nodeType);
        result.setArgument(Qn::ParentNodeTypeRole, parentIndexNodeType);
        result.setArgument(Qn::TopLevelParentNodeTypeRole, topLevelParentNodeType(index));

        QSet<QnResourcePtr> resources;

        // Consider recorder's children its siblings.
        const auto effectiveParent =
            [](const QModelIndex& index)
            {
                const auto result = index.parent();
                const auto resultType = result.data(Qn::NodeTypeRole).value<NodeType>();
                return resultType != NodeType::recorder ? result : result.parent();
            };

        const auto firstParentIndex = selection.empty()
            ? QModelIndex()
            : effectiveParent(selection[0]);

        bool onlySiblings = true;
        QStringList selectedGroupIds;

        for (const auto& index: selection)
        {
            if (effectiveParent(index) != firstParentIndex)
                onlySiblings = false;

            if (index.data(Qn::NodeTypeRole).value<NodeType>() == NodeType::customResourceGroup)
            {
                selectedGroupIds << index.data(Qn::ResourceTreeCustomGroupIdRole).toString();
                const auto childCameras = childCamerasRecursive(index);
                for (const auto camera: childCameras)
                    resources.insert(camera);
            }
        }

        if (!resources.empty())
        {
            // Merge resources within selected groups with directly selected resources.
            for (const auto directlySelectedResource: result.resources())
                resources.insert(directlySelectedResource);

            result.setResources(QnResourceList(resources.cbegin(), resources.cend()));
        }

        result.setArgument(Qn::OnlyResourceTreeSiblingsRole, onlySiblings);
        result.setArgument(Qn::SelectedGroupIdsRole, selectedGroupIds);

        const auto compositeGroupId = nodeType == NodeType::recorder
            ? index.model()->index(0, 0, index).data(Qn::ResourceTreeCustomGroupIdRole)
            : index.data(Qn::ResourceTreeCustomGroupIdRole);

        result.setArgument(Qn::ResourceTreeCustomGroupIdRole, compositeGroupId);

        if (nodeType == NodeType::customResourceGroup)
        {
            const auto getFilterModelAndIndex =
                [](const QModelIndex& index) -> std::pair<const FilterProxyModel*, QModelIndex>
                {
                    if (!index.isValid())
                        return {};

                    auto currentIndex = index;
                    auto currentProxy = qobject_cast<const QAbstractProxyModel*>(index.model());
                    while (currentProxy)
                    {
                        if (const auto filterModel =
                            qobject_cast<const FilterProxyModel*>(currentProxy))
                        {
                            return {filterModel, currentIndex};
                        }
                        currentIndex = currentProxy->mapToSource(currentIndex);
                        currentProxy =
                            qobject_cast<const QAbstractProxyModel*>(currentProxy->sourceModel());
                    }
                    return {};
                };

            const auto [filterModel, filterIndex] = getFilterModelAndIndex(index);

            if (!NX_ASSERT(filterModel && filterIndex.isValid()))
                return result;

            const auto sourceIndex = filterModel->mapToSource(filterIndex);
            if (!NX_ASSERT(sourceIndex.isValid()))
                return result;

            const auto filterLeafChildrenCount =
                item_model::getLeafIndexes(filterModel, filterIndex).size();

            const auto sourceLeafChildrenCount =
                item_model::getLeafIndexes(sourceIndex.model(), sourceIndex).size();

            const bool fullMatch = filterLeafChildrenCount == sourceLeafChildrenCount;
            result.setArgument(Qn::SelectedGroupFullyMatchesFilter, fullMatch);
        }

        return result;
    }
};

ResourceTreeInteractionHandler::ResourceTreeInteractionHandler(
    QnWorkbenchContext* context,
    QObject* parent)
    :
    base_type(parent),
    d(new Private{context})
{
    connect(d->renameAction.get(), &QAction::triggered,
        this, &ResourceTreeInteractionHandler::editRequested);
}

ResourceTreeInteractionHandler::~ResourceTreeInteractionHandler()
{
    // Required here for forward-declared scoped pointer deletion.
}

void ResourceTreeInteractionHandler::showContextMenu(QWidget* parent, const QPoint& globalPos,
    const QModelIndex& index, const QModelIndexList& selection)
{
    if (qnRuntime->isVideoWallMode() || !NX_ASSERT(d->context))
        return;

    const std::shared_ptr<QMenu> menu(d->context->menu()->newMenu(
        ui::action::TreeScope,
        parent,
        d->actionParameters(index, selection)));

    if (menu->isEmpty())
        return;

    static const QSet<ui::action::IDType> renameActions = {
        ui::action::RenameResourceAction,
        ui::action::RenameVideowallEntityAction,
        ui::action::RenameLayoutTourAction,
        ui::action::RenameCustomGroupAction};

    for (auto id: renameActions)
        d->context->menu()->redirectAction(menu.get(), id, d->renameAction.get());

    executeLater([menu, globalPos]() { QnHiDpiWorkarounds::showMenu(menu.get(), globalPos); },
        this);
}

ui::action::Parameters ResourceTreeInteractionHandler::actionParameters(
    const QModelIndex& index, const QModelIndexList& selection)
{
    return d->actionParameters(index, selection);
}

void ResourceTreeInteractionHandler::activateItem(const QModelIndex& index,
    const QModelIndexList& selection,
    const ResourceTree::ActivationType activationType,
    const Qt::KeyboardModifiers modifiers)
{
    if (!NX_ASSERT(d->context))
        return;

    if (activationType == ResourceTree::ActivationType::middleClick)
    {
        const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        d->context->menu()->trigger(ui::action::OpenInNewTabAction, resource);
        return;
    }

    const auto nodeType = index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
    switch (nodeType)
    {
        case ResourceTree::NodeType::cloudSystem:
        {
            const auto callback =
                [this, cloudSystemId = index.data(Qn::CloudSystemIdRole).toString()]()
                {
                    if (!d->context)
                        return;

                    CloudSystemConnectData connectData =
                        {cloudSystemId, ConnectScenario::connectFromTree};

                    d->context->menu()->trigger(ui::action::ConnectToCloudSystemAction,
                        {Qn::CloudSystemConnectDataRole, connectData});
                };

            // Looks like Qt has some bug when mouse events hangs and are targeted to
            // the wrong widget if we switch between QGLWidget-based views and QQuickView.
            // This workaround gives time to all (double)click-related events to be processed.
            if (nx::build_info::isMacOsX())
            {
                static constexpr int kSomeEnoughDelayToProcessDoubleClick = 100;
                executeDelayedParented(callback, kSomeEnoughDelayToProcessDoubleClick, this);
            }
            else
            {
                callback();
            }

            break;
        }

        case ResourceTree::NodeType::cloudSystemStatus:
        {
            if (activationType != ResourceTree::ActivationType::doubleClick)
                return;

            const auto systemId = index.data(Qn::CloudSystemIdRole).toString();
            d->context->menu()->trigger(
                ui::action::ConnectToCloudSystemWithUserInteractionAction,
                {Qn::CloudSystemIdRole, systemId});

            break;
        }

        case ResourceTree::NodeType::videoWallItem:
        {
            const auto item = d->context->resourcePool()->getVideoWallItemByUuid(
                index.data(Qn::UuidRole).value<QnUuid>());

            d->context->menu()->triggerIfPossible(ui::action::StartVideoWallControlAction,
                QnVideoWallItemIndexList() << item);

            break;
        }

        case ResourceTree::NodeType::layoutTour:
        {
            d->context->menu()->triggerIfPossible(ui::action::ReviewLayoutTourAction,
                {Qn::UuidRole, index.data(Qn::UuidRole).value<QnUuid>()});
            break;
        }

        case ResourceTree::NodeType::customResourceGroup:
        case ResourceTree::NodeType::recorder:
        {
            if (activationType == ResourceTree::ActivationType::doubleClick)
                return;

            QnResourceList resources;

            const auto childLeafIndexes = item_model::getLeafIndexes(index.model(), index);
            for (const auto& childIndex: childLeafIndexes)
            {
                if (const auto resource = childIndex.data(Qn::ResourceRole).value<QnResourcePtr>())
                    resources.push_back(resource);
            }

            d->context->menu()->trigger(ui::action::DropResourcesAction, resources);
            break;
        }

        default:
        {
            const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
            // Do not open users or fake servers.
            if (!resource || resource->hasFlags(Qn::user) || resource->hasFlags(Qn::fake))
                break;

            // Do not open servers of admin.
            if (nodeType == ResourceTree::NodeType::resource
                && resource->hasFlags(Qn::server)
                && activationType == ResourceTree::ActivationType::doubleClick)
            {
                break;
            }

            const bool isLayoutTourReviewMode =
                d->context->workbench()->currentLayout()->isLayoutTourReview();

            const auto actionId = isLayoutTourReviewMode || modifiers == Qt::ControlModifier
                ? ui::action::OpenInNewTabAction
                : ui::action::DropResourcesAction;

            if (activationType == ResourceTree::ActivationType::enterKey)
                d->context->menu()->trigger(actionId, d->actionParameters(index, selection));
            else
                d->context->menu()->trigger(actionId, resource);
            break;
        }
    }
}

void ResourceTreeInteractionHandler::activateSearchResults(
    const QModelIndexList& indexes, Qt::KeyboardModifiers modifiers)
{
    if (indexes.empty())
        return;

    QnResourceList resources;
    QnVideoWallResourceList videowalls;
    QVector<QnUuid> showreels;

    QSet<QnResourcePtr> processedResources;

    for (const auto& index: indexes)
    {
        const auto nodeType = index.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
        if (nodeType == ResourceTree::NodeType::layoutTour)
        {
            const auto showreelId = index.data(Qn::UuidRole).value<QnUuid>();
            showreels.push_back(showreelId);
            continue;
        }

        const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (!resource
            || processedResources.contains(resource)
            || !QnResourceAccessFilter::isDroppable(resource))
        {
            continue;
        }

        processedResources.insert(resource);

        if (const auto videowall = resource.objectCast<QnVideoWallResource>())
            videowalls.push_back(videowall);
        else
            resources.push_back(resource);
    }

    const auto manager = d->context->menu();
    if (!resources.isEmpty())
    {
        const auto action = modifiers.testFlag(Qt::ControlModifier)
            ? ui::action::OpenInNewTabAction
            : ui::action::OpenInCurrentLayoutAction;

        manager->trigger(action, {resources});
    }

    if (!modifiers.testFlag(Qt::ControlModifier))
        return;

    for (const auto& videowallResource: videowalls)
        manager->triggerIfPossible(ui::action::OpenVideoWallReviewAction, videowallResource);

    for (const auto& showreelId: showreels)
        manager->triggerIfPossible(ui::action::ReviewLayoutTourAction, {Qn::UuidRole, showreelId});
}

} // namespace nx::vms::client::desktop
