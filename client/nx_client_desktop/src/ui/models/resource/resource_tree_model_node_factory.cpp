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

QnResourceTreeModelNodePtr QnResourceTreeModelNodeFactory::createNode(
    Qn::NodeType nodeType,
    QnResourceTreeModel* model,
    bool initialize)
{
    QnResourceTreeModelNodePtr result;
    switch (nodeType)
    {
        case Qn::UserResourcesNode:
            result.reset(new QnResourceTreeModelUserResourcesNode(model));
            break;

        case Qn::OtherSystemsNode:
            if (model->scope() == QnResourceTreeModel::FullScope)
                result.reset(new QnResourceTreeModelOtherSystemsNode(model));
            break;

        case Qn::LayoutToursNode:
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
    Qn::NodeType nodeType,
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
    QnResourceTreeModelNodePtr result(new QnResourceTreeModelNode(model, Qn::SystemNode, systemName));
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
    Qn::NodeType nodeType = Qn::ResourceNode;

    if (model->context()->accessController()->hasGlobalPermission(Qn::GlobalAdminPermission)
        && QnMediaServerResource::isHiddenServer(resource->getParentResource()))
    {
        nodeType = Qn::EdgeNode;
    }

    QnResourceTreeModelNodePtr result(resource->hasFlags(Qn::layout)
        ? new QnResourceTreeModelLayoutNode(model, resource, nodeType)
        : new QnResourceTreeModelNode(model, resource, nodeType));
    result->initialize();
    return result;
}
