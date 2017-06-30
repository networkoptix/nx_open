#include "resource_tree_model_layout_tours_node.h"

#include <core/resource_management/layout_tour_manager.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/resource_tree_model_node_factory.h>

#include <ui/workbench/workbench_context.h>

QnResourceTreeModelLayoutToursNode::QnResourceTreeModelLayoutToursNode(QnResourceTreeModel* model):
    base_type(model, Qn::LayoutToursNode)
{
}

QnResourceTreeModelLayoutToursNode::~QnResourceTreeModelLayoutToursNode()
{
    /* Make sure all nodes are removed from the parent model. */
    clean();
}

void QnResourceTreeModelLayoutToursNode::initialize()
{
    base_type::initialize();

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnResourceTreeModelLayoutToursNode::rebuild);

    connect(layoutTourManager(), &QnLayoutTourManager::tourAdded, this,
        &QnResourceTreeModelLayoutToursNode::handleTourAdded);
    connect(layoutTourManager(), &QnLayoutTourManager::tourChanged, this,
        &QnResourceTreeModelLayoutToursNode::handleTourChanged);
    connect(layoutTourManager(), &QnLayoutTourManager::tourRemoved, this,
        &QnResourceTreeModelLayoutToursNode::handleTourRemoved);

    rebuild();
}

void QnResourceTreeModelLayoutToursNode::deinitialize()
{
    context()->disconnect(this);

    clean();
    base_type::deinitialize();
}

void QnResourceTreeModelLayoutToursNode::handleTourAdded(const ec2::ApiLayoutTourData& tour)
{
    ensureLayoutTourNode(tour);
}

void QnResourceTreeModelLayoutToursNode::handleTourChanged(const ec2::ApiLayoutTourData& tour)
{
    ensureLayoutTourNode(tour)->update();
}

void QnResourceTreeModelLayoutToursNode::handleTourRemoved(const QnUuid& tourId)
{
    removeNode(m_nodes.value(tourId));
}

QnResourceTreeModelNodePtr QnResourceTreeModelLayoutToursNode::ensureLayoutTourNode(
    const ec2::ApiLayoutTourData& tour)
{
    auto iter = m_nodes.find(tour.id);
    if (iter == m_nodes.end())
    {
        auto node = QnResourceTreeModelNodeFactory::createNode(Qn::LayoutTourNode, tour.id, model());
        node->setParent(toSharedPointer());
        iter = m_nodes.insert(tour.id, node);
    }
    return *iter;
}

void QnResourceTreeModelLayoutToursNode::rebuild()
{
    clean();
    for (const auto& tour: layoutTourManager()->tours())
        handleTourAdded(tour);
}

void QnResourceTreeModelLayoutToursNode::removeNode(const QnResourceTreeModelNodePtr& node)
{
    if (!node)
        return;

    switch (node->type())
    {
        case Qn::LayoutTourNode:
            NX_EXPECT(m_nodes.key(node) == node->uuid());
            m_nodes.remove(node->uuid());
            break;
        default:
            NX_ASSERT(false, "Trying to remove invalid node from layout tour nodes.");
            break;
    }

    node->deinitialize();
}

void QnResourceTreeModelLayoutToursNode::clean()
{
    for (auto node: m_nodes)
        node->deinitialize();
    m_nodes.clear();
}
