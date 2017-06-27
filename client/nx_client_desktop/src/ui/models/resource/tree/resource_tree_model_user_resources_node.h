#pragma once

#include <core/resource_access/resource_access_subject.h>

#include <ui/models/resource/resource_tree_model_node.h>

class QnResourceTreeModelUserResourcesNode: public QnResourceTreeModelNode
{
    using base_type = QnResourceTreeModelNode;
public:
    QnResourceTreeModelUserResourcesNode(QnResourceTreeModel* model);
    virtual ~QnResourceTreeModelUserResourcesNode();

    virtual void initialize() override;
    virtual void deinitialize() override;

private:
    void handleAccessChanged(const QnResourceAccessSubject& subject, const QnResourcePtr& resource);

    bool isResourceVisible(const QnResourcePtr& resource) const;

    /** Get or create shared resource node if needed. */
    QnResourceTreeModelNodePtr ensureResourceNode(const QnResourcePtr& resource);

    /** Get or create recorder placeholder for grouped cameras. */
    QnResourceTreeModelNodePtr ensureRecorderNode(const QnVirtualCameraResourcePtr& camera);

    void rebuild();

    /** Cleanup all node references. */
    void removeNode(const QnResourceTreeModelNodePtr& node);

    /** Remove all nodes. */
    void clean();

    /** Remove recorder nodes that are not in use. */
    void cleanupRecorders();

private:
    NodeList m_items;
    RecorderHash m_recorders;
};
