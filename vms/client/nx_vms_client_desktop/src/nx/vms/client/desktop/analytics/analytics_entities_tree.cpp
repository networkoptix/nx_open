#include "analytics_entities_tree.h"

#include <type_traits>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/rule.h>

#include <nx/analytics/properties.h>
#include <nx/analytics/descriptor_manager.h>
#include <nx/analytics/helpers.h>

#include <nx/utils/std/algorithm.h>

#include <nx/utils/data_structures/map_helper.h>

namespace nx::vms::client::desktop {

using namespace nx::analytics;
using namespace nx::vms::api;

using Node = AnalyticsEntitiesTreeBuilder::Node;
using NodePtr = AnalyticsEntitiesTreeBuilder::NodePtr;
using NodeType = AnalyticsEntitiesTreeBuilder::NodeType;
using NodeFilter = AnalyticsEntitiesTreeBuilder::NodeFilter;

namespace {

NodePtr makeNode(NodeType nodeType, const QString& text = QString())
{
    return NodePtr(new Node(nodeType, text));
}

template<typename Entity>
auto managerByEntityType(QnCommonModule* commonModule)
{
    if constexpr (std::is_same_v<Entity, analytics::EventTypeDescriptor>)
        return commonModule->analyticsEventTypeDescriptorManager();
    else if constexpr (std::is_same_v<Entity, analytics::ObjectTypeDescriptor>)
        return commonModule->analyticsObjectTypeDescriptorManager();
    else if constexpr (std::is_same_v<Entity, analytics::GroupDescriptor>)
        return commonModule->analyticsGroupDescriptorManager();
    else if constexpr (std::is_same_v<Entity, analytics::EngineDescriptor>)
        return commonModule->analyticsEngineDescriptorManager();
    else if constexpr (std::is_same_v<Entity, analytics::PluginDescriptor>)
        return commonModule->analyticsPluginDescriptorManager();
    else
        return nullptr;
}


template<typename Entity>
ScopedEntityTypeIds resolveEntities(
    QnCommonModule* commonModule,
    const AnalyticsEntitiesTreeBuilder::UnresolvedEntities& entities)
{
    const auto manager = managerByEntityType<Entity>(commonModule);
    if (!NX_ASSERT(manager))
        return ScopedEntityTypeIds();

    ScopedEventTypeIds result;
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
    ScopedEventTypeIds scopedEventTypeIds,
    const AnalyticsEntitiesTreeBuilder::UnresolvedEntities& additionalEntities = {})
{
    const auto engineDescriptorManager = commonModule->analyticsEngineDescriptorManager();
    const auto groupDescriptorManager = commonModule->analyticsGroupDescriptorManager();
    const auto eventTypeDescriptorManager = commonModule->analyticsEventTypeDescriptorManager();

    auto root = makeNode(NodeType::root);

    const auto resolvedEntities =
        resolveEntities<analytics::EventTypeDescriptor>(commonModule, additionalEntities);

    MapHelper::merge(&scopedEventTypeIds, resolvedEntities, mergeEntityIds);

    for (const auto& [engineId, eventTypeIdsByGroup]: scopedEventTypeIds)
    {
        const auto engineDescriptor = engineDescriptorManager->descriptor(engineId);
        if (!NX_ASSERT(engineDescriptor))
            continue;

        auto engine = makeNode(NodeType::engine, engineDescriptor->name);
        engine->engineId = engineId;
        root->children.push_back(engine);

        for (const auto& [groupId, eventTypeIds]: eventTypeIdsByGroup)
        {
            NodePtr parentNode = engine;

            const auto groupDescriptor = groupDescriptorManager->descriptor(groupId);
            if (groupDescriptor)
            {
                auto group = makeNode(NodeType::group, groupDescriptor->name);
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

                auto eventType = makeNode(NodeType::eventType, eventTypeDescriptor->name);
                eventType->engineId = engineId;
                eventType->entityId = eventTypeDescriptor->id;
                parentNode->children.push_back(eventType);
            }
        }
    }
    return AnalyticsEntitiesTreeBuilder::filterTree(root);
}

NodePtr buildObjectTypesTree(QnCommonModule* commonModule, ScopedObjectTypeIds scopedObjectTypeIds)
{
    const auto engineDescriptorManager = commonModule->analyticsEngineDescriptorManager();
    const auto groupDescriptorManager = commonModule->analyticsGroupDescriptorManager();
    const auto objectTypeDescriptorManager = commonModule->analyticsObjectTypeDescriptorManager();

    auto root = makeNode(NodeType::root);

    for (const auto& [engineId, objectTypeIdsByGroup]: scopedObjectTypeIds)
    {
        const auto engineDescriptor = engineDescriptorManager->descriptor(engineId);
        if (!NX_ASSERT(engineDescriptor))
            continue;

        auto engine = makeNode(NodeType::engine, engineDescriptor->name);
        engine->engineId = engineId;
        root->children.push_back(engine);

        for (const auto& [groupId, objectTypeIds]: objectTypeIdsByGroup)
        {
            NodePtr parentNode = engine;

            const auto groupDescriptor = groupDescriptorManager->descriptor(groupId);
            if (groupDescriptor)
            {
                auto group = makeNode(NodeType::group, groupDescriptor->name);
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

                auto objectType = makeNode(NodeType::objectType, objectTypeDescriptor->name);
                objectType->engineId = engineId;
                objectType->entityId = objectTypeDescriptor->id;
                parentNode->children.push_back(objectType);
            }
        }
    }
    return AnalyticsEntitiesTreeBuilder::filterTree(root);
}

NodePtr filterEngine(NodePtr engine, NodeFilter excludeNodes)
{
    NX_ASSERT(engine->nodeType == NodeType::engine);

    // Remove immediate children if they should be filtered out.
    if (excludeNodes)
    {
        nx::utils::remove_if(engine->children, excludeNodes);

        // Remove event types in groups.
        for (auto child: engine->children)
        {
            if (child->nodeType == NodeType::group)
                nx::utils::remove_if(child->children, excludeNodes);
        }
    }

    // Cleanup empty groups.
    nx::utils::remove_if(engine->children,
        [](const auto& node)
        {
            return node->nodeType == NodeType::group && node->children.empty();
        });

    return engine;
}

} // namespace

//-------------------------------------------------------------------------------------------------
// AnalyticsEntitiesTreeBuilder class

NodePtr AnalyticsEntitiesTreeBuilder::filterTree(NodePtr root, NodeFilter excludeNodes)
{
    if (root->nodeType == NodeType::engine)
        return filterEngine(root, excludeNodes);

    NX_ASSERT(root->nodeType == NodeType::root);
    auto& engines = root->children;

    for (auto engine: engines)
        filterEngine(engine, excludeNodes);

    // Cleanup empty engines.
    nx::utils::remove_if(engines, [](const auto& engine) { return engine->children.empty(); });

    // Make the single engine the root if possible.
    if (engines.size() == 1)
        return engines[0];

    return root;
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
    QnCommonModuleAware(parent)
{
    using RuleManager = nx::vms::event::RuleManager;
    const auto ruleManager = commonModule()->eventRuleManager();
    connect(ruleManager, &RuleManager::rulesReset,
        this, &AnalyticsEventsSearchTreeBuilder::eventTypesTreeChanged);
    connect(ruleManager, &RuleManager::ruleAddedOrUpdated,
        this, &AnalyticsEventsSearchTreeBuilder::eventTypesTreeChanged);
    connect(ruleManager, &RuleManager::ruleRemoved,
        this, &AnalyticsEventsSearchTreeBuilder::eventTypesTreeChanged);

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            const auto flags = resource->flags();
            if (flags.testFlag(Qn::server_live_cam))
            {
                if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
                {
                    connect(
                        camera.get(),
                        &QnVirtualCameraResource::compatibleEventTypesMaybeChanged,
                        this,
                        [this](const QnVirtualCameraResourcePtr& /*camera*/)
                        {
                            emit eventTypesTreeChanged();
                        });

                    emit eventTypesTreeChanged();
                }
            }
            else if (flags.testFlag(Qn::server) && !flags.testFlag(Qn::fake))
            {
                connect(resource.get(), &QnResource::propertyChanged, this,
                    [this](const QnResourcePtr& /*res*/, const QString& key)
                    {
                        // TODO: #GDM Validate if other properties matter.
                        if (key == nx::analytics::kEventTypeDescriptorsProperty)
                            emit eventTypesTreeChanged();
                    });
                emit eventTypesTreeChanged();
            }
        });

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            const auto flags = resource->flags();
            if (flags.testFlag(Qn::server_live_cam))
            {
                resource->disconnect(this);
                emit eventTypesTreeChanged();
            }
            else if (flags.testFlag(Qn::server) && !flags.testFlag(Qn::fake))
            {
                resource->disconnect(this);
                emit eventTypesTreeChanged();
            }
        });
}

NodePtr AnalyticsEventsSearchTreeBuilder::eventTypesTree() const
{
    using namespace nx::vms::event;

    QSet<EventTypeId> actuallyUsedEventTypes;
    for (const auto& rule: commonModule()->eventRuleManager()->rules())
    {
        if (rule->eventType() == EventType::analyticsSdkEvent)
            actuallyUsedEventTypes.insert(rule->eventParams().getAnalyticsEventTypeId());
    }

    // Early exit if no analytics rules are configured.
    if (actuallyUsedEventTypes.isEmpty())
        return NodePtr(new Node(NodeType::root));

    // TODO: #GDM Shouldn't we filter out cameras here the same way?
    const auto devices = resourcePool()->getAllCameras({}, /*ignoreDesktopCameras*/ true);
    const auto manager = commonModule()->analyticsEventTypeDescriptorManager();
    auto tree = buildEventTypesTree(commonModule(), manager->compatibleEventTypeIdsUnion(devices));

    return AnalyticsEntitiesTreeBuilder::filterTree(tree,
        [actuallyUsedEventTypes](NodePtr node)
        {
            return node->nodeType == NodeType::eventType &&
                !actuallyUsedEventTypes.contains(node->entityId);
        });
}

//-------------------------------------------------------------------------------------------------
// AnalyticsObjectsSearchTreeBuilder class

AnalyticsObjectsSearchTreeBuilder::AnalyticsObjectsSearchTreeBuilder(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            const auto flags = resource->flags();
            if (flags.testFlag(Qn::server_live_cam))
            {
                if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
                {
                    connect(
                        camera.get(),
                        &QnVirtualCameraResource::compatibleObjectTypesMaybeChanged,
                        this,
                        [this](const QnVirtualCameraResourcePtr& /*camera*/)
                        {
                            emit objectTypesTreeChanged();
                        });

                    emit objectTypesTreeChanged();
                }
            }
            else if (flags.testFlag(Qn::server) && !flags.testFlag(Qn::fake))
            {
                connect(resource.get(), &QnResource::propertyChanged, this,
                    [this](const QnResourcePtr& /*res*/, const QString& key)
                    {
                        // TODO: #GDM Validate if other properties matter.
                        if (key == nx::analytics::kObjectTypeDescriptorsProperty)
                            emit objectTypesTreeChanged();
                    });
                emit objectTypesTreeChanged();
            }
        });

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            const auto flags = resource->flags();
            if (flags.testFlag(Qn::server_live_cam))
            {
                resource->disconnect(this);
                emit objectTypesTreeChanged();
            }
            else if (flags.testFlag(Qn::server) && !flags.testFlag(Qn::fake))
            {
                resource->disconnect(this);
                emit objectTypesTreeChanged();
            }
        });
}

AnalyticsEntitiesTreeBuilder::NodePtr AnalyticsObjectsSearchTreeBuilder::objectTypesTree() const
{
    const auto devices = resourcePool()->getAllCameras({}, /*ignoreDesktopCameras*/ true);
    const auto manager = commonModule()->analyticsObjectTypeDescriptorManager();
    return buildObjectTypesTree(commonModule(), manager->compatibleObjectTypeIdsUnion(devices));
}

} // namespace nx::vms::client::desktop
