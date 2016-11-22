#include "resource_tree_model_my_cloud_node.h"

#include <core/resource_management/resource_pool.h>

#include <finders/systems_finder.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/workbench/workbench_context.h>

QnResourceTreeModeMyCloudNode::QnResourceTreeModeMyCloudNode(
    QnResourceTreeModel* model)
    :
    base_type(model, Qn::MyCloudNode)
{
    connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered,
        this, &QnResourceTreeModeMyCloudNode::handleSystemDiscovered);

    rebuild();
}

QnResourceTreeModeMyCloudNode::~QnResourceTreeModeMyCloudNode()
{
    /* Make sure all nodes are removed from the parent model. */
    clean();
}

void QnResourceTreeModeMyCloudNode::initialize()
{
    base_type::initialize();
    rebuild();
}

void QnResourceTreeModeMyCloudNode::deinitialize()
{
    disconnect(qnSystemsFinder, nullptr, this, nullptr);
    clean();
    base_type::deinitialize();
}

void QnResourceTreeModeMyCloudNode::handleSystemDiscovered(const QnSystemDescriptionPtr& system)
{
    if (!system->isCloudSystem())
        return;

    auto node = ensureSystemNode(system);
    //node->setName(system->name());
}

QnResourceTreeModelNodePtr QnResourceTreeModeMyCloudNode::ensureSystemNode(
    const QnSystemDescriptionPtr& system)
{
    auto iter = m_nodes.find(system->id());
    if (iter == m_nodes.end())
    {
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelNode(
            model(),
            Qn::CloudSystemNode,
            system->name()));
        node->initialize();
        node->setParent(toSharedPointer());

        iter = m_nodes.insert(system->id(), node);
    }
    return *iter;
}

void QnResourceTreeModeMyCloudNode::rebuild()
{
    clean();
    for (const auto& system : qnSystemsFinder->systems())
        handleSystemDiscovered(system);
}

void QnResourceTreeModeMyCloudNode::removeNode(const QnResourceTreeModelNodePtr& node)
{
    /* Check if node was already removed. */
    auto key = m_nodes.key(node);
    if (key.isEmpty())
        return;

    m_nodes.remove(key);

    node->deinitialize();
}

void QnResourceTreeModeMyCloudNode::clean()
{
    for (auto node: m_nodes)
        node->deinitialize();
    m_nodes.clear();
}
