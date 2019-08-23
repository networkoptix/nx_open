#include "analytics_events_tree.h"

#include <nx/analytics/descriptor_manager.h>

#include <nx/utils/std/algorithm.h>

namespace nx::vms::client::desktop {

using namespace nx::analytics;

struct AnalyticsEventsTreeBuilder::Private
{
    Private(QnCommonModule* commonModule, Flags flags):
        flags(flags),
        eventTypeDescriptorManager(new EventTypeDescriptorManager(commonModule)),
        engineDescriptorManager(new EngineDescriptorManager(commonModule)),
        groupDescriptorManager(new GroupDescriptorManager(commonModule))
    {
    }

    Flags flags;
    QScopedPointer<EventTypeDescriptorManager> eventTypeDescriptorManager;
    QScopedPointer<EngineDescriptorManager> engineDescriptorManager;
    QScopedPointer<GroupDescriptorManager> groupDescriptorManager;

    NodePtr makeNode(NodeType nodeType, const QString& text = QString())
    {
        return NodePtr(new Node(nodeType, text));
    }

    /**
     * Remove hidden event types. Remove groups where no event types left (especially actual after
     * intersection). Remove engines where no groups left. If there is only one engine left, make
     * it the root.
     */
    NodePtr stripNodes(NodePtr tree)
    {
        NX_ASSERT(tree->nodeType == NodeType::root);

        auto& engines = tree->children;

        // Cleanup empty groups inside each engine.
        if (flags.testFlag(HideEmptyGroups))
        {
            for (auto engine: engines)
            {
                NX_ASSERT(engine->nodeType == NodeType::engine);
                nx::utils::remove_if(engine->children,
                    [](const auto& node)
                    {
                        return node->nodeType == NodeType::group && node->children.empty();
                    });
            }
        }

        // Cleanup empty engines.
        nx::utils::remove_if(engines, [](const auto& engine) { return engine->children.empty(); });

        // Make the single engine the root if possible.
        if (engines.size() == 1)
            return engines[0];

        return tree;
    }

    NodePtr buildTree(ScopedEventTypeIds scopedEventTypeIds)
    {
        auto root = makeNode(NodeType::root);

        for (const auto& [engineId, eventTypeIdsByGroup]: scopedEventTypeIds)
        {
            const auto engineDescriptor = engineDescriptorManager->descriptor(engineId);
            if (!NX_ASSERT(engineDescriptor))
                continue;

            // TODO1: multipleEngines logic. Hide engine menu if it is the only one.
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
        return stripNodes(root);
    }

};

AnalyticsEventsTreeBuilder::AnalyticsEventsTreeBuilder(
    QnCommonModule* commonModule,
    Flags flags)
    :
    d(new Private(commonModule, flags))
{
}

AnalyticsEventsTreeBuilder::~AnalyticsEventsTreeBuilder()
{
}

AnalyticsEventsTreeBuilder::NodePtr AnalyticsEventsTreeBuilder::compatibleTreeIntersection(
    const QnVirtualCameraResourceList& devices) const
{
    return d->buildTree(d->eventTypeDescriptorManager->compatibleEventTypeIdsIntersection(devices));
}

AnalyticsEventsTreeBuilder::NodePtr AnalyticsEventsTreeBuilder::compatibleTreeUnion(
    const QnVirtualCameraResourceList& devices) const
{
    return d->buildTree(d->eventTypeDescriptorManager->compatibleEventTypeIdsUnion(devices));
}

} // namespace nx::vms::client::desktop
