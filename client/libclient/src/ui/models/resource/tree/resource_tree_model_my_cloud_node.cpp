#include "resource_tree_model_my_cloud_node.h"

#include <core/resource_management/resource_pool.h>

#include <finders/systems_finder.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/tree/resource_tree_model_cloud_system_node.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>

QnResourceTreeModeMyCloudNode::QnResourceTreeModeMyCloudNode(QnResourceTreeModel* model):
    base_type(model, Qn::MyCloudNode)
{
    connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered,
        this, &QnResourceTreeModeMyCloudNode::handleSystemDiscovered);
    connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemLost,
        this, &QnResourceTreeModeMyCloudNode::handleSystemLost);

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

QIcon QnResourceTreeModeMyCloudNode::calculateIcon() const
{
    return qnSkin->icon("welcome_page/cloud_online.png");
}

void QnResourceTreeModeMyCloudNode::handleSystemDiscovered(const QnSystemDescriptionPtr& system)
{
    const QString id = system->id();

    m_disconnectHelpers[id] << connect(system, &QnBaseSystemDescription::isCloudSystemChanged, this,
        [this, system, id]
        {
            if (system->isCloudSystem())
                ensureSystemNode(system);
            else
                removeNode(m_nodes.value(id));
        });

    if (system->isCloudSystem())
        ensureSystemNode(system);
}

void QnResourceTreeModeMyCloudNode::handleSystemLost(const QString& id)
{
    m_disconnectHelpers.remove(id);
    removeNode(m_nodes.value(id));
}

QnResourceTreeModelNodePtr QnResourceTreeModeMyCloudNode::ensureSystemNode(
    const QnSystemDescriptionPtr& system)
{
    auto iter = m_nodes.find(system->id());
    if (iter == m_nodes.end())
    {
        QnResourceTreeModelNodePtr node(new QnResourceTreeModelCloudSystemNode(system, model()));
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
    if (!node)
        return;

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
