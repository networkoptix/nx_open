#include "analytics_events_tree.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>

#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/rule.h>

#include <nx/analytics/properties.h>
#include <nx/analytics/descriptor_manager.h>

#include <nx/utils/std/algorithm.h>

namespace nx::vms::client::desktop {

using namespace nx::analytics;

using Node = AnalyticsEventsTreeBuilder::Node;
using NodePtr = AnalyticsEventsTreeBuilder::NodePtr;
using NodeType = AnalyticsEventsTreeBuilder::NodeType;
using NodeFilter = AnalyticsEventsTreeBuilder::NodeFilter;

namespace {

NodePtr makeNode(NodeType nodeType, const QString& text = QString())
{
    return NodePtr(new Node(nodeType, text));
}

NodePtr buildTree(QnCommonModule* commonModule, ScopedEventTypeIds scopedEventTypeIds)
{
    const auto engineDescriptorManager = commonModule->analyticsEngineDescriptorManager();
    const auto groupDescriptorManager = commonModule->analyticsGroupDescriptorManager();
    const auto eventTypeDescriptorManager = commonModule->analyticsEventTypeDescriptorManager();

    auto root = makeNode(NodeType::root);

    for (const auto& [engineId, eventTypeIdsByGroup] : scopedEventTypeIds)
    {
        const auto engineDescriptor = engineDescriptorManager->descriptor(engineId);
        if (!NX_ASSERT(engineDescriptor))
            continue;

        auto engine = makeNode(NodeType::engine, engineDescriptor->name);
        engine->engineId = engineId;
        root->children.push_back(engine);

        for (const auto& [groupId, eventTypeIds] : eventTypeIdsByGroup)
        {
            NodePtr parentNode = engine;

            const auto groupDescriptor = groupDescriptorManager->descriptor(groupId);
            if (groupDescriptor)
            {
                auto group = makeNode(NodeType::group, groupDescriptor->name);
                group->engineId = engineId;
                group->eventTypeId = groupDescriptor->id;
                engine->children.push_back(group);
                parentNode = group;
            }

            for (const auto& eventTypeId : eventTypeIds)
            {
                const auto eventTypeDescriptor =
                    eventTypeDescriptorManager->descriptor(eventTypeId);

                if (!eventTypeDescriptor || eventTypeDescriptor->isHidden())
                    continue;

                auto eventType = makeNode(NodeType::eventType, eventTypeDescriptor->name);
                eventType->engineId = engineId;
                eventType->eventTypeId = eventTypeDescriptor->id;
                parentNode->children.push_back(eventType);
            }
        }
    }
    return AnalyticsEventsTreeBuilder::filterTree(root);
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
// AnalyticsEventsTreeBuilder class

NodePtr AnalyticsEventsTreeBuilder::filterTree(NodePtr root, NodeFilter excludeNodes)
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

NodePtr AnalyticsEventsTreeBuilder::eventTypesForRulesPurposes(
    QnCommonModule* commonModule,
    const QnVirtualCameraResourceList& devices)
{
    return buildTree(commonModule,
        commonModule->analyticsEventTypeDescriptorManager()
            ->compatibleEventTypeIdsIntersection(devices));
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
                emit eventTypesTreeChanged();
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
    for (const auto& rule : commonModule()->eventRuleManager()->rules())
    {
        if (rule->eventType() == EventType::analyticsSdkEvent)
            actuallyUsedEventTypes.insert(rule->eventParams().getAnalyticsEventTypeId());
    }

    // Early exit if no analytics rules are configured.
    if (actuallyUsedEventTypes.isEmpty())
        return NodePtr(new Node(NodeType::root));

    // TODO: #GDM Shouldn't we filter out cameras here the same way?
    const auto devices = resourcePool()->getAllCameras({}, /*ignoreDesktopCameras*/ true);
    auto unionTree = buildTree(commonModule(),
        commonModule()->analyticsEventTypeDescriptorManager()->compatibleEventTypeIdsUnion(devices));

    return AnalyticsEventsTreeBuilder::filterTree(unionTree,
        [actuallyUsedEventTypes](NodePtr node)
        {
            return node->nodeType == NodeType::eventType &&
                !actuallyUsedEventTypes.contains(node->eventTypeId);
        });
}

} // namespace nx::vms::client::desktop
