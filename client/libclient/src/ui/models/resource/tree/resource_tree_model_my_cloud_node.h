#pragma once

#include <ui/models/resource/resource_tree_model_node.h>

#include <network/base_system_description.h>

class QnResourceTreeModeMyCloudNode: public QnResourceTreeModelNode
{
    using base_type = QnResourceTreeModelNode;
public:
    QnResourceTreeModeMyCloudNode(QnResourceTreeModel* model);
    virtual ~QnResourceTreeModeMyCloudNode();

    virtual void initialize() override;
    virtual void deinitialize() override;

private:
    void handleSystemDiscovered(const QnSystemDescriptionPtr& system);

    QnResourceTreeModelNodePtr ensureSystemNode(const QnSystemDescriptionPtr& system);

    void rebuild();

    /** Cleanup all node references. */
    void removeNode(const QnResourceTreeModelNodePtr& node);

    /** Remove all nodes. */
    void clean();

private:
    QHash<QString, QnResourceTreeModelNodePtr> m_nodes;
};
