#pragma once

#include <functional>

#include <QtCore/QString>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>

#include <common/common_module_aware.h>

#include <core/resource/resource_fwd.h>

#include <nx/analytics/types.h>

class QnCommonModule;

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API AnalyticsEntitiesTreeBuilder final
{
public:
    struct Node;
    using NodePtr = QSharedPointer<Node>;

    enum class NodeType
    {
        root,
        engine,
        group,
        eventType,
        objectType
    };

    struct Node
    {
        using EntityTypeId = QString;
        static_assert(std::is_same<nx::analytics::GroupId, EntityTypeId>::value);
        static_assert(std::is_same<nx::analytics::EventTypeId, EntityTypeId>::value);
        static_assert(std::is_same<nx::analytics::ObjectTypeId, EntityTypeId>::value);

        const NodeType nodeType;
        QString text;
        nx::analytics::EngineId engineId;

        /** Can contain group id, event type id or object type id. */
        EntityTypeId entityId;
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
    static NodePtr eventTypesForRulesPurposes(
        QnCommonModule* commonModule,
        const QnVirtualCameraResourceList& devices);
};

class AnalyticsEventsSearchTreeBuilder: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit AnalyticsEventsSearchTreeBuilder(QObject* parent = nullptr);

    /**
     * Tree of the Event type ids. Root nodes are Engines, then Groups and Event types as leaves.
     * Includes all Event types, which can theoretically be available in the System, so all
     * compatible Engines are used. Includes only those Event types, which are actually used in the
     * existing VMS Rules. Empty Groups and Engines are removed from the output.
     */
    AnalyticsEntitiesTreeBuilder::NodePtr eventTypesTree() const;

signals:
    void eventTypesTreeChanged();
};

class AnalyticsObjectsSearchTreeBuilder : public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit AnalyticsObjectsSearchTreeBuilder(QObject* parent = nullptr);

    /**
     * Tree of the Object type ids. Root nodes are Engines, then Groups and Object types as leaves.
     * Includes all Object types, which can theoretically be available in the System, so all
     * compatible Engines are used. Empty Groups and Engines are removed from the output.
     */
    AnalyticsEntitiesTreeBuilder::NodePtr objectTypesTree() const;

signals:
    void objectTypesTreeChanged();
};

} // namespace nx::vms::client::desktop
