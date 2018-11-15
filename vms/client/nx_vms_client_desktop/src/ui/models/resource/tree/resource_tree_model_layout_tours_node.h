#pragma once

#include <ui/models/resource/resource_tree_model_node.h>

#include <nx_ec/data/api_fwd.h>

/**
 * Node which displays layout tours, accessible to the user.
 */
class QnResourceTreeModelLayoutToursNode: public QnResourceTreeModelNode
{
    using base_type = QnResourceTreeModelNode;
    Q_OBJECT

public:
    QnResourceTreeModelLayoutToursNode(QnResourceTreeModel* model);
    virtual ~QnResourceTreeModelLayoutToursNode();

    virtual void initialize() override;
    virtual void deinitialize() override;

private:
    void handleTourAdded(const nx::vms::api::LayoutTourData& tour);
    void handleTourChanged(const nx::vms::api::LayoutTourData& tour);
    void handleTourRemoved(const QnUuid& tourId);

    QnResourceTreeModelNodePtr ensureLayoutTourNode(const nx::vms::api::LayoutTourData& tour);

    void rebuild();

    /** Cleanup all node references. */
    void removeNode(QnResourceTreeModelNodePtr node);

    /** Remove all nodes. */
    void clean();

private:
    QHash<QnUuid, QnResourceTreeModelNodePtr> m_nodes;
};
