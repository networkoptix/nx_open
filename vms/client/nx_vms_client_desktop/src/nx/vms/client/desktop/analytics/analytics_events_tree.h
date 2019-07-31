#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>

#include <core/resource/resource_fwd.h>

#include <nx/analytics/types.h>

class QnCommonModule;

namespace nx::vms::client::desktop {

class AnalyticsEventsTreeBuilder final
{
public:
    enum Flag {
        NoFlags = 0,
        HideEmptyGroups = 1 << 0
    };
    Q_DECLARE_FLAGS(Flags, Flag);

    explicit AnalyticsEventsTreeBuilder(
        QnCommonModule* commonModule,
        Flags flags = HideEmptyGroups);
    ~AnalyticsEventsTreeBuilder();

    struct Node;
    using NodePtr = QSharedPointer<Node>;

    enum class NodeType {
        root,
        engine,
        group,
        eventType
    };

    struct Node {
        const NodeType nodeType;
        QString text;
        nx::analytics::EngineId engineId;
        nx::analytics::EventTypeId eventTypeId; //< GroupId can also be here.
        std::vector<NodePtr> children;

        explicit Node(NodeType nodeType, const QString& text):
            nodeType(nodeType),
            text(text)
        {
        }
    };

    /**
     * Tree of the event type ids. Root nodes are engines, then groups and event types as leaves.
     * Includes all event types, which can theoretically be available on these devices, so all
     * compatible engines are used.
     * Event types are intersected, empty groups and engines are removed from the output.
     */
    NodePtr compatibleTreeIntersection(const QnVirtualCameraResourceList& devices) const;

    /**
     * Tree of the event type ids. Root nodes are engines, then groups and event types as leaves.
     * Includes all event types, which can theoretically be available on these devices, so all
     * compatible engines are used.
     * Event types are united, empty groups and engines are removed from the output.
     */
    NodePtr compatibleTreeUnion(const QnVirtualCameraResourceList& devices) const;

private:
    struct Private;
    QScopedPointer<Private> d;
};


} // namespace nx::vms::client::desktop
