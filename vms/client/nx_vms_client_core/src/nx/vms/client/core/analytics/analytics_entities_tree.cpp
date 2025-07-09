// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_entities_tree.h"

#include <type_traits>

#include <QtConcurrent/QtConcurrent>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/helpers.h>
#include <nx/analytics/taxonomy/state_helper.h>
#include <nx/analytics/taxonomy/state_watcher.h>
#include <nx/utils/data_structures/map_helper.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/analytics_event_type_field.h>
#include <nx/vms/rules/events/analytics_event.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::client::core {

using namespace nx::vms;
using namespace nx::analytics::taxonomy;

using Node = AnalyticsEntitiesTreeBuilder::Node;
using NodePtr = AnalyticsEntitiesTreeBuilder::NodePtr;
using NodeType = AnalyticsEntitiesTreeBuilder::NodeType;
using NodeFilter = AnalyticsEntitiesTreeBuilder::NodeFilter;

namespace {

static const auto kAnalyticsEventType = rules::utils::type<rules::AnalyticsEvent>();

NodePtr makeNode(NodeType nodeType, QWeakPointer<Node> parent, const QString& text = QString())
{
    return NodePtr(new Node(nodeType, parent, text));
}

template<typename EntityType>
std::vector<ScopedEntity<EntityType>> resolveEntities(
    const std::shared_ptr<AbstractState>& state,
    const AnalyticsEntitiesTreeBuilder::UnresolvedEntities& unresolvedIds,
    std::set<nx::Uuid> allowedEngines)
{
    std::vector<ScopedEntity<EntityType>> result;
    for (const auto& unresolved: unresolvedIds)
    {
        EntityType* entity = nullptr;
        if constexpr (std::is_same_v<EventType, EntityType>)
            entity = state->eventTypeById(unresolved.entityId);

        if constexpr (std::is_same_v<ObjectType, EntityType>)
            entity = state->objectTypeById(unresolved.entityId);

        if (!entity)
            continue;

        for (const auto& scope: entity->scopes())
        {
            if (const auto& engine = scope.engine();
                engine
                && allowedEngines.contains(nx::Uuid(engine->id()))
                && (unresolved.engineId.isNull()
                    || engine->id() == unresolved.engineId.toString(QUuid::WithBraces)))
            {
                result.push_back(ScopedEntity<EntityType>(engine, scope.group(), entity));
            }
        }
    }

    return result;
}

NodePtr buildEventTypesTree(const std::vector<EngineScope<EventType>>& eventTypeTree)
{
    auto root = makeNode(NodeType::root, {});

    for (const EngineScope<EventType>& engineScope: eventTypeTree)
    {
        auto engine = makeNode(NodeType::engine, root, engineScope.engine->name());
        const nx::Uuid engineId(engineScope.engine->id());
        engine->engineId = engineId;
        root->children.push_back(engine);

        for (const GroupScope<EventType>& groupScope: engineScope.groups)
        {
            NodePtr parentNode = engine;

            const AbstractGroup* taxonomyGroup = groupScope.group;
            if (taxonomyGroup)
            {
                auto group = makeNode(NodeType::group, parentNode, taxonomyGroup->name());
                group->engineId = engineId;
                group->entityId = taxonomyGroup->id();
                engine->children.push_back(group);
                parentNode = group;
            }

            for (const auto& taxonomyEventType: groupScope.entities)
            {
                if (taxonomyEventType->isHidden())
                    continue;

                auto eventType = makeNode(
                    NodeType::eventType, parentNode, taxonomyEventType->name());
                eventType->engineId = engineId;
                eventType->entityId = taxonomyEventType->id();
                parentNode->children.push_back(eventType);
            }
        }
    }

    return AnalyticsEntitiesTreeBuilder::cleanupTree(root);
}

NodePtr buildObjectTypesTree(const std::vector<EngineScope<ObjectType>>& objectTypeTree)
{
    auto root = makeNode(NodeType::root, {});

    for (const EngineScope<ObjectType>& engineScope: objectTypeTree)
    {
        const AbstractEngine* taxonomyEngine = engineScope.engine;
        if (!NX_ASSERT(taxonomyEngine))
            continue;

        const nx::Uuid engineId = nx::Uuid::fromStringSafe(taxonomyEngine->id());
        auto engine = makeNode(NodeType::engine, root, taxonomyEngine->name());
        engine->engineId = engineId;
        root->children.push_back(engine);

        for (const GroupScope<ObjectType>& groupScope: engineScope.groups)
        {
            NodePtr parentNode = engine;

            const AbstractGroup* taxonomyGroup = groupScope.group;
            if (taxonomyGroup)
            {
                auto group = makeNode(NodeType::group, parentNode, taxonomyGroup->name());
                group->engineId = engineId;
                group->entityId = taxonomyGroup->id();
                engine->children.push_back(group);
                parentNode = group;
            }

            for (const ObjectType* taxonomyObjectType: groupScope.entities)
            {
                auto objectType = makeNode(
                    NodeType::objectType, parentNode, taxonomyObjectType->name());
                objectType->engineId = engineId;
                objectType->entityId = taxonomyObjectType->id();
                parentNode->children.push_back(objectType);
            }
        }
    }

    return AnalyticsEntitiesTreeBuilder::cleanupTree(root);
}

// Cleanup empty groups from the given node.
NodePtr removeEmptyGroupsRecursive(NodePtr node)
{
    for (const auto& child: node->children)
        removeEmptyGroupsRecursive(child);

    // Cleanup empty subgroups.
    nx::utils::erase_if(node->children,
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
    nx::utils::erase_if(node->children, excludeNode);

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
    return resource->hasFlags(Qn::server_live_cam) || resource->hasFlags(Qn::server);
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
    nx::utils::erase_if(engines, [](const auto& engine) { return engine->children.empty(); });

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
    SystemContext* systemContext,
    const QnVirtualCameraResourceList& devices,
    const UnresolvedEntities& additionalUnresolvedEventTypes)
{
    std::shared_ptr<AbstractState> taxonomyState = systemContext->analyticsTaxonomyState();
    if (!taxonomyState)
        return NodePtr();

    /**
     * Event rule stores only eventTypeId, but not engineId. That lead parameter
     * 'additionalUnresolvedEventTypes' has empty engineId sometimes. Filter out incompatible
     * engines to prevent 'resolveEntities' selects events for non-compatible engines with same
     * eventTypeId.
     */
    using namespace nx::utils;
    std::set<nx::Uuid> allowed;
    for (qsizetype i = 0; i < devices.size(); ++i)
    {
        if (i == 0)
            allowed = devices[i]->compatibleAnalyticsEngines();
        else
            allowed = set_intersection(allowed, devices[i]->compatibleAnalyticsEngines());
    }

    std::vector<ScopedEntity<EventType>> additionalScopedEventTypes =
        resolveEntities<EventType>(taxonomyState, additionalUnresolvedEventTypes, allowed);

    StateHelper stateHelper(taxonomyState);

    return buildEventTypesTree(
        stateHelper.supportedEventTypeTreeIntersection(devices, additionalScopedEventTypes));
}

//-------------------------------------------------------------------------------------------------
// AnalyticsEventsSearchTreeBuilder class

AnalyticsEventsSearchTreeBuilder::AnalyticsEventsSearchTreeBuilder(
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    cachedEventTypesTree(makeNode(NodeType::root, {}))
{
    using Engine = nx::vms::rules::Engine;
    const auto engine = systemContext->vmsRulesEngine();
    connect(engine, &Engine::rulesReset,
        this, &AnalyticsEventsSearchTreeBuilder::updateEventTypesTree);
    connect(engine, &Engine::ruleAddedOrUpdated,
        this, &AnalyticsEventsSearchTreeBuilder::updateEventTypesTree);
    connect(engine, &Engine::ruleRemoved,
        this, &AnalyticsEventsSearchTreeBuilder::updateEventTypesTree);

    auto notifyAboutResourceListChanges =
        [this](const QnResourceList& resources)
        {
            if (std::any_of(resources.cbegin(), resources.cend(), &isManageableResource))
                updateEventTypesTree();
        };

    auto genericResourceListener = new core::SessionResourcesSignalListener<QnResource>(
        systemContext,
        this);
    genericResourceListener->setOnAddedHandler(notifyAboutResourceListChanges);
    genericResourceListener->setOnRemovedHandler(notifyAboutResourceListChanges);
    genericResourceListener->start();

    auto camerasListener = new core::SessionResourcesSignalListener<QnVirtualCameraResource>(
        systemContext,
        this);
    camerasListener->addOnCustomSignalHandler(
        &QnVirtualCameraResource::compatibleEventTypesMaybeChanged,
        [this]() { updateEventTypesTree(); });
    camerasListener->start();

    auto serversListener = new core::SessionResourcesSignalListener<QnMediaServerResource>(
        systemContext,
        this);
    serversListener->addOnCustomSignalHandler(
        &QnMediaServerResource::analyticsDescriptorsChanged,
        [this]() { updateEventTypesTree(); });
    serversListener->start();
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

void AnalyticsEventsSearchTreeBuilder::updateEventTypesTree()
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
                        cachedEventTypesTree = calculateEventTypesTree();
                    }
                }
                emit eventTypesTreeChanged();
            });
    }
}

NodePtr AnalyticsEventsSearchTreeBuilder::calculateEventTypesTree() const
{
    using namespace nx::vms::event;

    QSet<api::analytics::EventTypeId> actuallyUsedEventTypes;
    const auto engine = systemContext()->vmsRulesEngine();
    for (const auto& [_, rule]: engine->rules())
    {
        for (const auto& filter: rule->eventFiltersByType(kAnalyticsEventType))
        {
            actuallyUsedEventTypes.insert(
                filter->fieldByType<rules::AnalyticsEventTypeField>()->typeId());
        }
    }

    actuallyUsedEventTypes.remove({});

    // Early exit if no analytics rules are configured.
    if (actuallyUsedEventTypes.isEmpty())
        return NodePtr(new Node(NodeType::root, {}));

    // TODO: #sivanov Shouldn't we filter out cameras here the same way?
    const auto devices = resourcePool()->getAllCameras(nx::Uuid(), /*ignoreDesktopCameras*/ true);

    const std::shared_ptr<AbstractState> taxonomyState = systemContext()->analyticsTaxonomyState();
    if (!taxonomyState)
        return NodePtr();

    const StateHelper stateHelper(taxonomyState);
    auto tree = buildEventTypesTree(
        stateHelper.compatibleEventTypeTreeUnion(devices));

    return AnalyticsEntitiesTreeBuilder::filterTreeInclusive(tree,
        [actuallyUsedEventTypes](NodePtr node)
        {
            return actuallyUsedEventTypes.contains(node->entityId);
        });
}

//-------------------------------------------------------------------------------------------------
// AnalyticsObjectsSearchTreeBuilder class

AnalyticsObjectsSearchTreeBuilder::AnalyticsObjectsSearchTreeBuilder(
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext)
{
    auto notifyAboutResourceListChanges =
        [this](const QnResourceList& resources)
        {
            if (std::any_of(resources.cbegin(), resources.cend(), &isManageableResource))
                emit objectTypesTreeChanged();
        };

    auto genericResourceListener = new core::SessionResourcesSignalListener<QnResource>(
        systemContext,
        this);
    genericResourceListener->setOnAddedHandler(notifyAboutResourceListChanges);
    genericResourceListener->setOnRemovedHandler(notifyAboutResourceListChanges);
    genericResourceListener->start();

    auto camerasListener = new core::SessionResourcesSignalListener<QnVirtualCameraResource>(
        systemContext,
        this);
    camerasListener->addOnSignalHandler(
        &QnVirtualCameraResource::compatibleObjectTypesMaybeChanged,
        [this](auto /*param*/) { emit objectTypesTreeChanged(); });
    camerasListener->start();

    auto serversListener = new core::SessionResourcesSignalListener<QnMediaServerResource>(
        systemContext,
        this);
    serversListener->addOnSignalHandler(
        &QnMediaServerResource::analyticsDescriptorsChanged,
        [this](auto /*param*/) { emit objectTypesTreeChanged(); });
    serversListener->start();
}

AnalyticsEntitiesTreeBuilder::NodePtr AnalyticsObjectsSearchTreeBuilder::objectTypesTree() const
{
    const auto devices = resourcePool()->getAllCameras(nx::Uuid(), /*ignoreDesktopCameras*/ true);
    const std::shared_ptr<AbstractState> taxonomyState = systemContext()->analyticsTaxonomyState();
    if (!taxonomyState)
        return NodePtr();

    StateHelper stateHelper(taxonomyState);
    return buildObjectTypesTree(stateHelper.compatibleObjectTypeTreeUnion(devices));
}

} // namespace nx::vms::client::core
