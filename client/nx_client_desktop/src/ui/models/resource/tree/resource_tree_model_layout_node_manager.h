#pragma once

#include <ui/models/resource/tree/resource_tree_model_node_manager.h>

class QnResourceTreeModelLayoutNode;

class QnResourceTreeModelLayoutNodeManager: public QnResourceTreeModelNodeManager
{
    Q_OBJECT
    using base_type = QnResourceTreeModelNodeManager;

public:
    QnResourceTreeModelLayoutNodeManager(QnResourceTreeModel* model);
    virtual ~QnResourceTreeModelLayoutNodeManager();

protected:
    virtual void primaryNodeAdded(QnResourceTreeModelNode* node) override;

private:
    void handleResourceAdded(const QnResourcePtr& resource);

    void loadedStateChanged(QnResourceTreeModelLayoutNode* node, bool loaded);

private:
    friend class QnResourceTreeModelLayoutNode;

    /* Layouts that have items not yet linked to resources. */
    QSet<QnResourceTreeModelLayoutNode*> m_loadingLayouts;
};
