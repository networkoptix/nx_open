// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_entities_tree.h"

#include <type_traits>

#include <QtConcurrent/QtConcurrent>

#include <client/client_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/engine_descriptor_manager.h>
#include <nx/analytics/event_type_descriptor_manager.h>
#include <nx/analytics/group_descriptor_manager.h>
#include <nx/analytics/helpers.h>
#include <nx/analytics/object_type_descriptor_manager.h>
#include <nx/analytics/plugin_descriptor_manager.h>
#include <nx/utils/data_structures/map_helper.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>

namespace nx::vms::client::desktop {

using namespace nx::vms;

using Node = AnalyticsEntitiesTreeBuilder::Node;
using NodePtr = AnalyticsEntitiesTreeBuilder::NodePtr;
using NodeType = AnalyticsEntitiesTreeBuilder::NodeType;
using NodeFilter = AnalyticsEntitiesTreeBuilder::NodeFilter;

namespace {

NodePtr makeNode(NodeType nodeType, QWeakPointer<Node> parent, const QString& text = QString())
{
    return NodePtr(new Node(nodeType, parent, text));
}

template<typename Entity>
auto managerByEntityType(QnCommonModule* commonModule)
{
    if constexpr (std::is_same_v<Entity, api::analytics::EventTypeDescriptor>)
        return commonModule->analyticsEventTypeDescriptorManager();
    else if constexpr (std::is_same_v<Entity, api::analytics::ObjectTypeDescriptor>)
        return commonModule->analyticsObjectTypeDescriptorManager();
    else if constexpr (std::is_same_v<Entity, api::analytics::GroupDescriptor>)
        return commonModule->analyticsGroupDescriptorManager();
    else if constexpr (std::is_same_v<Entity, api::analytics::EngineDescriptor>)
        return commonModule->analyticsEngineDescriptorManager();
    else if constexpr (std::is_same_v<Entity, api::analytics::PluginDescriptor>)
        return commonModule->analyticsPluginDescriptorManager();
    else
        return nullptr;
}

template<typename Entity>
api::analytics::ScopedEntityTypeIds resolveEntities(
    QnCommonModule* commonModule,
    const AnalyticsEntitiesTreeBuilder::UnresolvedEntities& entities)
{
    const auto manager = managerByEntityType<Entity>(commonModule);
    if (!NX_ASSERT(manager))
        return api::analytics::ScopedEntityTypeIds();

    api::analytics::ScopedEventTypeIds result;
    for (const auto& entity: entities)
    {
        const auto descriptor = manager->descriptor(entity.entityId);
        if (!descriptor)
            continue;

        for (const auto& scope: descriptor->scopes)
        {
            if (scope.engineId == entity.engineId)
                result[entity.engineId][scope.groupId].insert(descriptor->id);
        }
    }

    return result;
}

NodePtr buildEventTypesTree(
    QnCommonModule* commonModule,
    api::analytics::ScopedEventTypeIds scopedEventTypeIds,
    const AnalyticsEntitiesTreeBuilder::UnresolvedEntities& additionalEntities = {})
{
    const auto engineDescriptorManager = commonModule->analyticsEngineDescriptorManager();
    const auto groupDescriptorManager = commonModule->analyticsGroupDescriptorManager();
    const auto eventTypeDescriptorManager = commonModule->analyticsEventTypeDescriptorManager();

    auto root = makeNode(NodeType::root, {});

    const auto resolvedEntities =
        resolveEntities<api::analytics::EventTypeDescriptor>(commonModule, additionalEntities);

    nx::utils::data_structures::MapHelper::merge(
        &scopedEventTypeIds, resolvedEntities, nx::analytics::mergeEntityIds);

    for (const auto& [engineId, eventTypeIdsByGroup]: scopedEventTypeIds)
    {
        const auto engineDescriptor = engineDescriptorManager->descriptor(engineId);
        // TODO: #dmishin here must be an assert. Return it back when we are fully moved to the new
        // Taxononomy engine.
        if (!engineDescriptor)
            continue;

        auto engine = makeNode(NodeType::engine, root, engineDescriptor->name);
        engine->engineId = engineId;
        root->children.push_back(engine);

        for (const auto& [groupId, eventTypeIds]: eventTypeIdsByGroup)
        {
            NodePtr parentNode = engine;

            const auto groupDescriptor = groupDescriptorManager->descriptor(groupId);
            if (groupDescriptor)
            {
                auto group = makeNode(NodeType::group, parentNode, groupDescriptor->name);
                group->engineId = engineId;
                group->entityId = groupDescriptor->id;
                engine->children.push_back(group);
                parentNode = group;
            }

            for (const auto& eventTypeId: eventTypeIds)
            {
                const auto eventTypeDescriptor =
                    eventTypeDescriptorManager->descriptor(eventTypeId);

                if (!eventTypeDescriptor || eventTypeDescriptor->isHidden())
                    continue;

                auto eventType = makeNode(
                    NodeType::eventType, parentNode, eventTypeDescriptor->name);
                eventType->engineId = engineId;
                eventType->entityId = eventTypeDescriptor->id;
                parentNode->children.push_back(eventType);
            }
        }
    }

    return AnalyticsEntitiesTreeBuilder::cleanupTree(root);
}

NodePtr buildObjectTypesTree(QnCommonModule* commonModule, api::analytics::ScopedObjectTypeIds scopedObjectTypeIds)
{
    const auto engineDescriptorManager = commonModule->analyticsEngineDescriptorManager();
    const auto groupDescriptorManager = commonModule->analyticsGroupDescriptorManager();
    const auto objectTypeDescriptorManager = commonModule->analyticsObjectTypeDescriptorManager();

    auto root = makeNode(NodeType::root, {});

    for (const auto& [engineId, objectTypeIdsByGroup]: scopedObjectTypeIds)
    {
        const auto engineDescriptor = engineDescriptorManager->descriptor(engineId);
        // TODO: #dmishin here must be an assert. Return it back when we are fully moved to the new
        // Taxononomy engine.
        if (!engineDescriptor)
            continue;

        auto engine = makeNode(NodeType::engine, root, engineDescriptor->name);
        engine->engineId = engineId;
        root->children.push_back(engine);

        for (const auto& [groupId, objectTypeIds]: objectTypeIdsByGroup)
        {
            NodePtr parentNode = engine;

            const auto groupDescriptor = groupDescriptorManager->descriptor(groupId);
            if (groupDescriptor)
            {
                auto group = makeNode(NodeType::group, parentNode, groupDescriptor->name);
                group->engineId = engineId;
                group->entityId = groupDescriptor->id;
                engine->children.push_back(group);
                parentNode = group;
            }

            for (const auto& objectTypeId: objectTypeIds)
            {
                const auto objectTypeDescriptor =
                    objectTypeDescriptorManager->descriptor(objectTypeId);

                if (!objectTypeDescriptor)
                    continue;

                auto objectType = makeNode(
                    NodeType::objectType, parentNode, objectTypeDescriptor->name);
                objectType->engineId = engineId;
                objectType->entityId = objectTypeDescriptor->id;
                parentNode->children.push_back(objectType);
            }
        }
    }

    return AnalyticsEntitiesTreeBuilder::cleanupTree(root);
}

// Cleanup empty groups from the given node.
NodePtr removeEmptyGroupsRecursive(NodePtr node)
{
    for (const auto child: node->children)
        removeEmptyGroupsRecursive(child);

    // Cleanup empty subgroups.
    nx::utils::remove_if(node->children,
        [](const auto& child)
        {
            return child->nodeType == NodeType::group && child->children.empty();
        });

    return node;
}

// Filter function must return true for nodes, which are to be filtered out from the tree.
NodePtr filterNodeExclusiveRecursive(NodePtr node, NodeFilter excludeNode)
{
    // Remove immediate children if they should be filtered out.
    nx::utils::remove_if(node->children, excludeNode);

    // Remove event types in groups.
    for (auto child: node->children)
    {
        if (child->nodeType == NodeType::group || child->nodeType == NodeType::engine)
            filterNodeExclusiveRecursive(child, excludeNode);
    }

    return node;
}

// Filter function must return true for nodes, which are to be kept in the tree with all their
// children.
NodePtr filterNodeInclusiveRecursive(NodePtr node, NodeFilter includeNode)
{
    auto& children = node->children;
    for (auto iter = children.begin(); iter != children.end(); /* do nothing */)
    {
        NodePtr child = *iter;

        // Keep groups and direct children which should be kept.
        if (includeNode(child))
        {
            ++iter;
        }
        else if (child->nodeType == NodeType::group || child->nodeType == NodeType::engine)
        {
            filterNodeInclusiveRecursive(child, includeNode);
            ++iter;
        }
        else if (NX_ASSERT(
            child->nodeType == NodeType::eventType || child->nodeType == NodeType::objectType))
        {
            if (includeNode(child))
                ++iter;
            else
                iter = children.erase(iter);
        }
        else
        {
            // Safety code, should never get here.
            ++iter;
        }
    }

    return node;
}

/** Filter resources which addition or removing can affect tree building result. */
bool isManageableResource(const QnResourcePtr& resource)
{
    return resource->hasFlags(Qn::server_live_cam)
        || (resource->hasFlags(Qn::server) && !resource->hasFlags(Qn::fake));
}

} // namespace

//-------------------------------------------------------------------------------------------------
// AnalyticsEntitiesTreeBuilder class

NodePtr AnalyticsEntitiesTreeBuilder::cleanupTree(NodePtr root)
{
    if (root->nodeType == NodeType::engine)
        return removeEmptyGroupsRecursive(root);

    NX_ASSERT(root->nodeType == NodeType::root);
    auto& engines = root->children;

    for (auto engine: engines)
        removeEmptyGroupsRecursive(engine);

    // Cleanup empty engines.
    nx::utils::remove_if(engines, [](const auto& engine) { return engine->children.empty(); });

    // Make the single engine the root if possible.
    if (engines.size() == 1)
        return engines[0];

    return root;
}

NodePtr AnalyticsEntitiesTreeBuilder::filterTreeExclusive(NodePtr root, NodeFilter excludeNode)
{
    filterNodeExclusiveRecursive(root, excludeNode);
    return cleanupTree(root);
}

NodePtr AnalyticsEntitiesTreeBuilder::filterTreeInclusive(NodePtr root, NodeFilter includeNode)
{
    if (root->nodeType == NodeType::engine)
    {
        filterNodeInclusiveRecursive(root, includeNode);
    }
    else if (NX_ASSERT(root->nodeType == NodeType::root))
    {
        for (auto engine: root->children)
            filterNodeInclusiveRecursive(engine, includeNode);
    }
    return cleanupTree(root);
}

NodePtr AnalyticsEntitiesTreeBuilder::eventTypesForRulesPurposes(
    QnCommonModule* commonModule,
    const QnVirtualCameraResourceList& devices,
    const UnresolvedEntities& additionalUnresolvedEventTypes)
{
    return buildEventTypesTree(
        commonModule,
        commonModule->analyticsEventTypeDescriptorManager()
            ->compatibleEventTypeIdsIntersection(devices),
        additionalUnresolvedEventTypes);
}

//-------------------------------------------------------------------------------------------------
// AnalyticsEventsSearchTreeBuilder class

AnalyticsEventsSearchTreeBuilder::AnalyticsEventsSearchTreeBuilder(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    cachedEventTypesTree(makeNode(NodeType::root, {}))
{
    connect(
        qnClientModule->taxonomyManager(),
        &analytics::TaxonomyManager::currentTaxonomyChanged,
        this,
        &AnalyticsEventsSearchTreeBuilder::onTaxonomyChanged);
}

AnalyticsEventsSearchTreeBuilder::~AnalyticsEventsSearchTreeBuilder()
{
    if (!eventTypesTreeFuture.isFinished())
        eventTypesTreeFuture.waitForFinished();
}

NodePtr AnalyticsEventsSearchTreeBuilder::eventTypesTree() const
{
    QMutexLocker lock(&mutex);
    return cachedEventTypesTree;
}

void AnalyticsEventsSearchTreeBuilder::onTaxonomyChanged()
{
    dirty = true;
    if (eventTypesTreeFuture.isFinished())
    {
        eventTypesTreeFuture = QtConcurrent::run(
            [this]
            {
                while (dirty)
                {
                    dirty = false;
                    {
                        QMutexLocker lock(&mutex);
                        cachedEventTypesTree = getEventTypesTree();
                    }
                    emit eventTypesTreeChanged();
                }
            });
    }
}

NodePtr AnalyticsEventsSearchTreeBuilder::getEventTypesTree() const
{
    using namespace nx::vms::event;

    QSet<api::analytics::EventTypeId> actuallyUsedEventTypes;
    for (const auto& rule: commonModule()->eventRuleManager()->rules())
    {
        if (rule->eventType() == EventType::analyticsSdkEvent)
            actuallyUsedEventTypes.insert(rule->eventParams().getAnalyticsEventTypeId());
    }

    // Early exit if no analytics rules are configured.
    if (actuallyUsedEventTypes.isEmpty())
        return NodePtr(new Node(NodeType::root, {}));

    // TODO: #sivanov Shouldn't we filter out cameras here the same way?
    const auto devices = resourcePool()->getAllCameras(QnUuid(), /*ignoreDesktopCameras*/ true);
    const auto manager = commonModule()->analyticsEventTypeDescriptorManager();
    auto tree = buildEventTypesTree(commonModule(), manager->compatibleEventTypeIdsUnion(devices));

    return AnalyticsEntitiesTreeBuilder::filterTreeInclusive(tree,
        [actuallyUsedEventTypes](NodePtr node)
        {
            return actuallyUsedEventTypes.contains(node->entityId);
        });
}

//-------------------------------------------------------------------------------------------------
// AnalyticsObjectsSearchTreeBuilder class

AnalyticsObjectsSearchTreeBuilder::AnalyticsObjectsSearchTreeBuilder(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
    auto notifyAboutResourceListChanges =
        [this](const QnResourceList& resources)
        {
            if (std::any_of(resources.cbegin(), resources.cend(), &isManageableResource))
                emit objectTypesTreeChanged();
        };

    auto genericResourceListener = new core::SessionResourcesSignalListener<QnResource>(this);
    genericResourceListener->setOnAddedHandler(notifyAboutResourceListChanges);
    genericResourceListener->setOnRemovedHandler(notifyAboutResourceListChanges);
    genericResourceListener->start();

    auto camerasListener = new core::SessionResourcesSignalListener<QnVirtualCameraResource>(this);
    camerasListener->addOnSignalHandler(
        &QnVirtualCameraResource::compatibleObjectTypesMaybeChanged,
        [this](auto /*param*/) { emit objectTypesTreeChanged(); });
    camerasListener->start();

    auto serversListener = new core::SessionResourcesSignalListener<QnMediaServerResource>(this);
    serversListener->addOnSignalHandler(
        &QnMediaServerResource::analyticsDescriptorsChanged,
        [this](auto /*param*/) { emit objectTypesTreeChanged(); });
    serversListener->start();
}

AnalyticsEntitiesTreeBuilder::NodePtr AnalyticsObjectsSearchTreeBuilder::objectTypesTree() const
{
    const auto devices = resourcePool()->getAllCameras(QnUuid(), /*ignoreDesktopCameras*/ true);
    const auto manager = commonModule()->analyticsObjectTypeDescriptorManager();
    return buildObjectTypesTree(commonModule(), manager->compatibleObjectTypeIdsUnion(devices));
}

} // namespace nx::vms::client::desktop
