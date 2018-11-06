#pragma once

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <network/base_system_description.h>

#include <ui/models/resource/resource_tree_model_fwd.h>

class QnResourceTreeModelNodeFactory
{
    using NodeType = nx::vms::client::desktop::ResourceTreeNodeType;
public:
    static QnResourceTreeModelNodePtr createNode(
        NodeType nodeType,
        QnResourceTreeModel* model,
        bool initialize = true);

    static QnResourceTreeModelNodePtr createNode(
        NodeType nodeType,
        const QnUuid& id,
        QnResourceTreeModel* model, 
        bool initialize = true);

    static QnResourceTreeModelNodePtr createLocalSystemNode(const QString& systemName,
        QnResourceTreeModel* model);

    static QnResourceTreeModelNodePtr createCloudSystemNode(const QnSystemDescriptionPtr& system,
        QnResourceTreeModel* model);

    static QnResourceTreeModelNodePtr createResourceNode(const QnResourcePtr& resource,
        QnResourceTreeModel* model);

};
