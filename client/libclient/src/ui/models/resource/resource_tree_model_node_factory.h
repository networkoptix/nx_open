#pragma once

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <network/base_system_description.h>

#include <ui/models/resource/resource_tree_model_fwd.h>

class QnResourceTreeModelNodeFactory
{
public:
    static QnResourceTreeModelNodePtr createNode(Qn::NodeType nodeType,
        QnResourceTreeModel* model, bool initialize = true);

    static QnResourceTreeModelNodePtr createNode(Qn::NodeType nodeType, const QnUuid& id,
        QnResourceTreeModel* model, bool initialize = true);

    static QnResourceTreeModelNodePtr createLocalSystemNode(const QString& systemName,
        QnResourceTreeModel* model);

    static QnResourceTreeModelNodePtr createCloudSystemNode(const QnSystemDescriptionPtr& system,
        QnResourceTreeModel* model);

    static QnResourceTreeModelNodePtr createResourceNode(const QnResourcePtr& resource,
        QnResourceTreeModel* model);

};
