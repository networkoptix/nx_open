#include "resource_tree_model_node_factory.h"

#include <core/resource/media_server_resource.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/resource_tree_model_node.h>

#include <ui/models/resource/tree/resource_tree_model_layout_node.h>
#include <ui/models/resource/tree/resource_tree_model_user_resources_node.h>
#include <ui/models/resource/tree/resource_tree_model_other_systems_node.h>
#include <ui/models/resource/tree/resource_tree_model_cloud_system_node.h>
#include <ui/models/resource/tree/resource_tree_model_layout_tours_node.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <nx/vms/client/desktop/resource_views/models/generic_resource_tree_model_node.h>

namespace {

bool isAcceptableForModelCamera(
    QnResourceTreeModel* model,
    const QnResourcePtr& resource)
{
    const auto context = model->context();
    return model->scope() != QnResourceTreeModel::UsersScope
        && QnResourceAccessFilter::isShareableMedia(resource)
        && context->resourceAccessProvider()->hasAccess(context->user(), resource)
        && resource->hasFlags(Qn::live_cam);
}

bool isAcceptableForModelLayout(
    QnResourceTreeModel* model,
    const QnResourcePtr& resource)
{
    const auto context = model->context();
    return model->scope() == QnResourceTreeModel::FullScope
        && context->resourceAccessProvider()->hasAccess(context->user(), resource)
        && resource->flags().testFlag(Qn::layout)
        && !resource->flags().testFlag(Qn::local);
}

bool isAcceptableForModelServer(
    QnResourceTreeModel* model,
    const QnResourcePtr& resource)
{
    const auto context = model->context();
    return model->scope() == QnResourceTreeModel::FullScope
        && context->resourceAccessProvider()->hasAccess(context->user(), resource)
        && resource->hasFlags(Qn::server)
        && !resource->hasFlags(Qn::fake);
}

bool isAcceptableForModelUser(
    QnResourceTreeModel* model,
    const QnResourcePtr& resource)
{
    const auto context = model->context();
    return model->scope() != QnResourceTreeModel::CamerasScope
        && context->resourceAccessProvider()->hasAccess(context->user(), resource)
        && resource->hasFlags(Qn::user);
}

bool isAcceptableForModelVideowall(
    QnResourceTreeModel* model,
    const QnResourcePtr& resource)
{
    const auto context = model->context();
    return model->scope() == QnResourceTreeModel::FullScope
        && context->resourceAccessProvider()->hasAccess(context->user(), resource)
        && resource->hasFlags(Qn::videowall);
}

} // namespace

using namespace nx::vms::client::desktop;

QnResourceTreeModelNodePtr QnResourceTreeModelNodeFactory::createNode(
    NodeType nodeType,
    QnResourceTreeModel* model,
    bool initialize)
{
    QnResourceTreeModelNodePtr result;
    switch (nodeType)
    {
        case NodeType::filteredCameras:
            result.reset(new GenericResourceTreeModelNode(
                model, isAcceptableForModelCamera, nodeType));
            break;

        case NodeType::filteredLayouts:
            result.reset(new GenericResourceTreeModelNode(
                model, isAcceptableForModelLayout, nodeType));
            break;

        case NodeType::filteredServers:
            result.reset(new GenericResourceTreeModelNode(
                model, isAcceptableForModelServer, nodeType));
            break;

        case NodeType::filteredUsers:
            result.reset(new GenericResourceTreeModelNode(
                model, isAcceptableForModelUser, nodeType));
            break;

        case NodeType::filteredVideowalls:
            result.reset(new GenericResourceTreeModelNode(
                model, isAcceptableForModelVideowall, nodeType));
            break;

        case NodeType::userResources:
            result.reset(new QnResourceTreeModelUserResourcesNode(model));
            break;

        case NodeType::otherSystems:
            if (model->scope() == QnResourceTreeModel::FullScope)
                result.reset(new QnResourceTreeModelOtherSystemsNode(model));
            break;

        case NodeType::layoutTours:
            if (model->scope() == QnResourceTreeModel::FullScope)
                result.reset(new QnResourceTreeModelLayoutToursNode(model));
            break;

        default:
            result.reset(new QnResourceTreeModelNode(model, nodeType));
            break;
    }
    if (result && initialize)
        result->initialize();
    return result;
}

QnResourceTreeModelNodePtr QnResourceTreeModelNodeFactory::createNode(
    NodeType nodeType,
    const QnUuid& id,
    QnResourceTreeModel* model,
    bool initialize)
{
    QnResourceTreeModelNodePtr result(new QnResourceTreeModelNode(model, id, nodeType));
    if (initialize)
        result->initialize();
    return result;
}

QnResourceTreeModelNodePtr QnResourceTreeModelNodeFactory::createLocalSystemNode(
    const QString& systemName,
    QnResourceTreeModel* model)
{
    QnResourceTreeModelNodePtr result(
        new QnResourceTreeModelNode(model, NodeType::localSystem, systemName));
    result->initialize();
    return result;
}

QnResourceTreeModelNodePtr QnResourceTreeModelNodeFactory::createCloudSystemNode(
    const QnSystemDescriptionPtr& system,
    QnResourceTreeModel* model)
{
    QnResourceTreeModelNodePtr result(new QnResourceTreeModelCloudSystemNode(system, model));
    result->initialize();
    return result;
}

QnResourceTreeModelNodePtr QnResourceTreeModelNodeFactory::createResourceNode(
    const QnResourcePtr& resource,
    QnResourceTreeModel* model)
{
    NodeType nodeType = NodeType::resource;

    if (model->context()->accessController()->hasGlobalPermission(GlobalPermission::admin)
        && QnMediaServerResource::isHiddenServer(resource->getParentResource()))
    {
        nodeType = NodeType::edge;
    }

    QnResourceTreeModelNodePtr result(resource->hasFlags(Qn::layout)
        ? new QnResourceTreeModelLayoutNode(model, resource, nodeType)
        : new QnResourceTreeModelNode(model, resource, nodeType));
    result->initialize();
    return result;
}
