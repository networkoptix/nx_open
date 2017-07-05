#include "resource_tree_model_other_systems_node.h"

#include <api/global_settings.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/user_resource.h>

#include <finders/systems_finder.h>

#include <network/system_helpers.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/models/resource/resource_tree_model_node_factory.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>

QnResourceTreeModelOtherSystemsNode::QnResourceTreeModelOtherSystemsNode(QnResourceTreeModel* model):
    base_type(model, Qn::OtherSystemsNode)
{
}

QnResourceTreeModelOtherSystemsNode::~QnResourceTreeModelOtherSystemsNode()
{
    /* Make sure all nodes are removed from the parent model. */
    clean();
}

void QnResourceTreeModelOtherSystemsNode::initialize()
{
    base_type::initialize();

    connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered,
        this, &QnResourceTreeModelOtherSystemsNode::handleSystemDiscovered);
    connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemLost,
        this, &QnResourceTreeModelOtherSystemsNode::handleSystemLost);

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnResourceTreeModelOtherSystemsNode::handleResourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnResourceTreeModelOtherSystemsNode::handleResourceRemoved);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnResourceTreeModelOtherSystemsNode::rebuild);
    connect(qnGlobalSettings, &QnGlobalSettings::autoDiscoveryChanged, this,
         &QnResourceTreeModelOtherSystemsNode::rebuild);
    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged, this,
        &QnResourceTreeModelOtherSystemsNode::rebuild);

    rebuild();
}

void QnResourceTreeModelOtherSystemsNode::deinitialize()
{
    qnGlobalSettings->disconnect(this);
    context()->disconnect(this);
    resourcePool()->disconnect(this);
    qnSystemsFinder->disconnect(this);

    clean();
    base_type::deinitialize();
}

QIcon QnResourceTreeModelOtherSystemsNode::calculateIcon() const
{
    return qnResIconCache->icon(QnResourceIconCache::OtherSystems);
}

void QnResourceTreeModelOtherSystemsNode::handleSystemDiscovered(const QnSystemDescriptionPtr& system)
{
    /* Only cloud systems are handled here. */
    const QString id = system->id();

    m_disconnectHelpers[id] << connect(system, &QnBaseSystemDescription::isCloudSystemChanged, this,
        [this, system, id]
        {
            if (canSeeSystem(system))
                ensureCloudSystemNode(system);
            else
                removeNode(m_cloudNodes.value(id));
        });

    if (canSeeSystem(system))
        ensureCloudSystemNode(system);
}

void QnResourceTreeModelOtherSystemsNode::handleSystemLost(const QString& id)
{
    /* Only cloud systems are handled here. */
    m_disconnectHelpers.remove(id);
    removeNode(m_cloudNodes.value(id));
}

void QnResourceTreeModelOtherSystemsNode::handleResourceAdded(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::fake))
        return;

    auto fakeServer = resource.dynamicCast<QnFakeMediaServerResource>();
    NX_ASSERT(fakeServer);
    if (!fakeServer)
        return;

    connect(fakeServer, &QnFakeMediaServerResource::moduleInformationChanged, this,
        &QnResourceTreeModelOtherSystemsNode::updateFakeServerNode);
    connect(fakeServer, &QnFakeMediaServerResource::moduleInformationChanged, this,
        &QnResourceTreeModelOtherSystemsNode::cleanupEmptyLocalNodes);

    updateFakeServerNode(fakeServer);
}

void QnResourceTreeModelOtherSystemsNode::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::fake))
        return;

    resource->disconnect(this);

    auto fakeServer = resource.dynamicCast<QnFakeMediaServerResource>();
    NX_ASSERT(fakeServer);
    if (!fakeServer)
        return;

    removeNode(m_fakeServers.value(fakeServer));
    cleanupEmptyLocalNodes();
}

void QnResourceTreeModelOtherSystemsNode::updateFakeServerNode(
    const QnFakeMediaServerResourcePtr& server)
{
    if (!canSeeFakeServers())
    {
        if (auto node = m_fakeServers.value(server))
        {
            removeNode(node);
            cleanupEmptyLocalNodes();
        }
        return;
    }

    const auto info = server->getModuleInformation();
    const QString systemName = helpers::isNewSystem(info)
        ? tr("New System")
        : info.systemName;

    const bool isCurrentSystemServer = helpers::serverBelongsToCurrentSystem(server);

    const auto parent = isCurrentSystemServer
        ? model()->rootNode(Qn::ServersNode)
        : ensureLocalSystemNode(systemName);

    auto node = ensureFakeServerNode(server);
    const bool cleanupNeeded = (node->parent() && node->parent() != parent);
    node->setParent(parent);
    if (cleanupNeeded)
        cleanupEmptyLocalNodes();
}

QnResourceTreeModelNodePtr QnResourceTreeModelOtherSystemsNode::ensureCloudSystemNode(
    const QnSystemDescriptionPtr& system)
{
    auto iter = m_cloudNodes.find(system->id());
    if (iter == m_cloudNodes.end())
    {
        auto node = QnResourceTreeModelNodeFactory::createCloudSystemNode(system, model());
        node->setParent(toSharedPointer());

        iter = m_cloudNodes.insert(system->id(), node);
    }
    return *iter;
}

QnResourceTreeModelNodePtr QnResourceTreeModelOtherSystemsNode::ensureLocalSystemNode(
    const QString& systemName)
{
    auto iter = m_localNodes.find(systemName);
    if (iter == m_localNodes.end())
    {
        auto node = QnResourceTreeModelNodeFactory::createLocalSystemNode(systemName, model());
        node->setParent(toSharedPointer());

        iter = m_localNodes.insert(systemName, node);
    }
    return *iter;
}

QnResourceTreeModelNodePtr QnResourceTreeModelOtherSystemsNode::ensureFakeServerNode(
    const QnFakeMediaServerResourcePtr& server)
{
    auto iter = m_fakeServers.find(server);
    if (iter == m_fakeServers.end())
    {
        auto node = QnResourceTreeModelNodeFactory::createResourceNode(server, model());
        node->setParent(toSharedPointer());

        iter = m_fakeServers.insert(server, node);
    }
    return *iter;
}

bool QnResourceTreeModelOtherSystemsNode::canSeeFakeServers() const
{
    bool isOwner = context()->user() && context()->user()->isOwner();
    bool isAutoDiscoveryEnabled = qnGlobalSettings->isAutoDiscoveryEnabled();

    return isOwner && isAutoDiscoveryEnabled;
}

bool QnResourceTreeModelOtherSystemsNode::canSeeSystem(const QnSystemDescriptionPtr& system) const
{
    return system->isCloudSystem() && system->id() != qnGlobalSettings->cloudSystemId();
}

void QnResourceTreeModelOtherSystemsNode::cleanupEmptyLocalNodes()
{
    NodeList nodesToRemove;
    for (auto node: m_localNodes)
    {
        if (node->children().isEmpty())
            nodesToRemove << node;
    }
    for (auto node: nodesToRemove)
        removeNode(node);
}

void QnResourceTreeModelOtherSystemsNode::rebuild()
{
    clean();

    for (const auto& system : qnSystemsFinder->systems())
        handleSystemDiscovered(system);

    if (!canSeeFakeServers())
        return;

    for (const auto& resource: resourcePool()->getIncompatibleServers())
    {
        const auto server = resource.dynamicCast<QnFakeMediaServerResource>();
        NX_ASSERT(server);
        if (server)
            updateFakeServerNode(server);
    }
}

void QnResourceTreeModelOtherSystemsNode::removeNode(const QnResourceTreeModelNodePtr& node)
{
    if (!node)
        return;

    switch(node->type())
    {
        case Qn::SystemNode:
            m_localNodes.remove(m_localNodes.key(node));
            break;
        case Qn::CloudSystemNode:
            m_cloudNodes.remove(m_cloudNodes.key(node));
            break;
        case Qn::ResourceNode:
            m_fakeServers.remove(node->resource());
            break;
        default:
            NX_ASSERT(false);
            break;
    }

    node->deinitialize();
}

void QnResourceTreeModelOtherSystemsNode::clean()
{
    for (auto node: m_cloudNodes)
        node->deinitialize();
    m_cloudNodes.clear();

    for (auto node: m_localNodes)
        node->deinitialize();
    m_localNodes.clear();

    for (auto node: m_fakeServers)
        node->deinitialize();
    m_fakeServers.clear();
}
