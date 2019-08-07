#pragma once

#include <functional>

#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>

#include <core/resource/resource_fwd.h>

#include <nx/analytics/types.h>

class QnCommonModule;

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API AnalyticsEventsTreeBuilder final
{
public:
    explicit AnalyticsEventsTreeBuilder(
        QnCommonModule* commonModule);
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

        explicit Node(NodeType nodeType, const QString& text = QString()):
            nodeType(nodeType),
            text(text)
        {
        }
    };

    using NodeFilter = std::function<bool(NodePtr)>;

    /**
     * Filter tree nodes, using the provided rule. Filter function must return true for nodes, which
     * are to be filtered out from the tree. Empty Groups and Engines will be cleaned up.
     */
    static NodePtr filterTree(NodePtr root, NodeFilter excludeNodes = {});

    /**
     * Tree of the Event type ids. Root nodes are Engines, then Groups and Event types as leaves.
     * Includes all Event types, which can theoretically be available on these Devices, so all
     * compatible Engines are used.
     * Event types are intersected, empty Groups and Engines are removed from the output.
     */
    NodePtr eventTypesForRulesPurposes(const QnVirtualCameraResourceList& devices) const;

    /**
     * Tree of the Event type ids. Root nodes are Engines, then Groups and Event types as leaves.
     * Includes all Event types, which can theoretically be available on these Devices, so all
     * compatible Engines are used. Includes only those Event types, which are actually used in the
     * existing VMS Rules. Empty Groups and Engines are removed from the output.
     */
    NodePtr eventTypesForSearchPurposes(const QnVirtualCameraResourceList& devices) const;

private:
    struct Private;
    QScopedPointer<Private> d;
};


} // namespace nx::vms::client::desktop
