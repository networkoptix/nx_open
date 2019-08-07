#include "analytics_events_tree.h"

#include <common/common_module.h>

#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/rule.h>

#include <nx/analytics/descriptor_manager.h>

#include <nx/utils/std/algorithm.h>

namespace nx::vms::client::desktop {

using namespace nx::analytics;

struct AnalyticsEventsTreeBuilder::Private: public QnCommonModuleAware
{
    Private(QnCommonModule* commonModule):
        QnCommonModuleAware(commonModule)
    {
    }

    NodePtr makeNode(NodeType nodeType, const QString& text = QString())
    {
        return NodePtr(new Node(nodeType, text));
    }

    static NodePtr filterEngine(NodePtr engine, NodeFilter excludeNodes)
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

    NodePtr buildTree(ScopedEventTypeIds scopedEventTypeIds)
    {
        const auto engineDescriptorManager = commonModule()->analyticsEngineDescriptorManager();
        const auto groupDescriptorManager = commonModule()->analyticsGroupDescriptorManager();
        const auto eventTypeDescriptorManager = commonModule()->analyticsEventTypeDescriptorManager();

        auto root = makeNode(NodeType::root);

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
                    group->eventTypeId = groupDescriptor->id;
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
                    eventType->eventTypeId = eventTypeDescriptor->id;
                    parentNode->children.push_back(eventType);
                }
            }
        }
        return AnalyticsEventsTreeBuilder::filterTree(root);
    }
};

AnalyticsEventsTreeBuilder::AnalyticsEventsTreeBuilder(QnCommonModule* commonModule):
    d(new Private(commonModule))
{
}

AnalyticsEventsTreeBuilder::~AnalyticsEventsTreeBuilder()
{
}

AnalyticsEventsTreeBuilder::NodePtr AnalyticsEventsTreeBuilder::filterTree(
    NodePtr root, NodeFilter excludeNodes)
{
    if (root->nodeType == NodeType::engine)
        return Private::filterEngine(root, excludeNodes);

    NX_ASSERT(root->nodeType == NodeType::root);
    auto& engines = root->children;

    for (auto engine: engines)
        Private::filterEngine(engine, excludeNodes);

    // Cleanup empty engines.
    nx::utils::remove_if(engines, [](const auto& engine) { return engine->children.empty(); });

    // Make the single engine the root if possible.
    if (engines.size() == 1)
        return engines[0];

    return root;
}

AnalyticsEventsTreeBuilder::NodePtr AnalyticsEventsTreeBuilder::eventTypesForRulesPurposes(
    const QnVirtualCameraResourceList& devices) const
{
    return d->buildTree(d->commonModule()->analyticsEventTypeDescriptorManager()
        ->compatibleEventTypeIdsIntersection(devices));
}

AnalyticsEventsTreeBuilder::NodePtr AnalyticsEventsTreeBuilder::eventTypesForSearchPurposes(
    const QnVirtualCameraResourceList& devices) const
{
    using namespace nx::vms::event;

    QSet<EventTypeId> actuallyUsedEventTypes;
    for (const auto& rule: d->commonModule()->eventRuleManager()->rules())
    {
        if (rule->eventType() == EventType::analyticsSdkEvent)
            actuallyUsedEventTypes.insert(rule->eventParams().getAnalyticsEventTypeId());
    }

    // Early exit if no analytics rules are configured.
    if (actuallyUsedEventTypes.isEmpty())
        return NodePtr(new Node(NodeType::root));

    // TODO: #GDM Shouldn't we filter out cameras here the same way?
    auto unionTree = d->buildTree(d->commonModule()->analyticsEventTypeDescriptorManager()
        ->compatibleEventTypeIdsUnion(devices));

    return filterTree(unionTree,
        [actuallyUsedEventTypes](NodePtr node)
        {
            return node->nodeType == NodeType::eventType &&
                !actuallyUsedEventTypes.contains(node->eventTypeId);
        });
}

} // namespace nx::vms::client::desktop
