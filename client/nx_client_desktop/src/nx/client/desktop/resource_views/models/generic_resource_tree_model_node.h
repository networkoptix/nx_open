#pragma once

#include <ui/models/resource/resource_tree_model_node.h>
#include <core/resource_access/resource_access_subject.h>

class QnResourceTreeModel;

namespace nx {
namespace client {
namespace desktop {

class GenericResourceTreeModelNode: public QnResourceTreeModelNode
{
    using base_type = QnResourceTreeModelNode;

public:
    using IsAcceptableResourceCheckFunction =
        std::function<bool (
            QnResourceTreeModel* model,
            const QnResourcePtr& resource)>;

    GenericResourceTreeModelNode(
        QnResourceTreeModel* model,
        const IsAcceptableResourceCheckFunction& checkFunction,
        Qn::NodeType nodeType);

    virtual ~GenericResourceTreeModelNode() override;

    virtual void initialize() override;
    virtual void deinitialize() override;

private:
    void handleAccessChanged(const QnResourceAccessSubject& subject, const QnResourcePtr& resource);

    /** Get or create shared resource node if needed. */
    QnResourceTreeModelNodePtr tryEnsureResourceNode(const QnResourcePtr& resource);

    /** Get or create recorder placeholder for grouped cameras. */
    QnResourceTreeModelNodePtr ensureRecorderNode(const QnVirtualCameraResourcePtr& camera);

    void rebuild();

    void tryRemoveResource(const QnResourcePtr& resource);

    /** Cleanup all node references. */
    void removeNode(const QnResourceTreeModelNodePtr& node);

    /** Remove all nodes. */
    void clean();

    /** Remove recorder nodes that are not in use. */
    void cleanupRecorders();

private:
    const IsAcceptableResourceCheckFunction m_isAcceptableCheck;
    NodeList m_items;
    RecorderHash m_recorders;
    Qn::NodeType m_type;
};

} // namespace desktop
} // namespace client
} // namespace nx
