#pragma once

#include <ui/models/resource/tree/resource_tree_model_node_manager.h>

class QnResourceTreeModelLayoutNode;

class QnResourceTreeModelLayoutNodeManager: public QnResourceTreeModelNodeManager
{
    using base_type = QnResourceTreeModelNodeManager;

public:
    QnResourceTreeModelLayoutNodeManager(QnResourceTreeModel* model);
    virtual ~QnResourceTreeModelLayoutNodeManager();

protected:
    virtual void primaryNodeAdded(QnResourceTreeModelNode* node) override;
};
