#include "resource_tree_model_node_factory.h"

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/resource_tree_model_node.h>

#include <ui/models/resource/tree/resource_tree_model_user_resources_node.h>
#include <ui/models/resource/tree/resource_tree_model_my_cloud_node.h>

QnResourceTreeModelNodePtr QnResourceTreeModelNodeFactory::createNode(Qn::NodeType nodeType,
    QnResourceTreeModel* model)
{
    QnResourceTreeModelNodePtr result;
    switch (nodeType)
    {
        case Qn::UserResourcesNode:
            result.reset(new QnResourceTreeModelUserResourcesNode(model));
            break;
        case Qn::MyCloudNode:
            result.reset(new QnResourceTreeModeMyCloudNode(model));
            break;
        default:
            result.reset(new QnResourceTreeModelNode(model, nodeType));
            break;
    }
    result->initialize();
    return result;
}
