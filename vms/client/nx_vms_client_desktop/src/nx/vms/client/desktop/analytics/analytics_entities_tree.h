// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QFuture>
#include <QtCore/QMutex>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API AnalyticsEntitiesTreeBuilder final
{
public:
    struct Node;
    using NodePtr = QSharedPointer<Node>;

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(NodeType,
        root,
        engine,
        group,
        eventType,
        objectType
    )

    struct Node
    {
        using EntityTypeId = QString;
        static_assert(std::is_same<nx::vms::api::analytics::GroupId, EntityTypeId>::value);
        static_assert(std::is_same<nx::vms::api::analytics::EventTypeId, EntityTypeId>::value);
        static_assert(std::is_same<nx::vms::api::analytics::ObjectTypeId, EntityTypeId>::value);

        const NodeType nodeType;
        QWeakPointer<Node> parent;
        QString text;
        nx::vms::api::analytics::EngineId engineId;

        /** Can contain group id, event type id or object type id. */
        EntityTypeId entityId;
        std::vector<NodePtr> children;

        explicit Node(NodeType nodeType, QWeakPointer<Node> parent, const QString& text = QString()):
            nodeType(nodeType),
            parent(parent),
            text(text)
        {
        }
    };

    struct UnresolvedEntity
    {
        QnUuid engineId;
        QString entityId;
    };

    using UnresolvedEntities = std::vector<UnresolvedEntity>;

    using NodeFilter = std::function<bool(NodePtr)>;

    /**
     * Filter tree nodes, removing empty Groups and Engines. If there is only one engine left,
     * makes it the new root.
     * @param root Single engine node or root node with a list of engines.
     */
    static NodePtr cleanupTree(NodePtr root);

    /**
     * Filter tree nodes, using the provided rule. Result will be cleaned up (@see cleanupTree).
     * @param root Single engine node or root node with a list of engines.
     * @param excludeNode Filter function must return true for nodes, which are to be filtered out
     *     from the tree.
     */
    static NodePtr filterTreeExclusive(NodePtr root, NodeFilter excludeNode);

    /**
     * Filter tree nodes, using the provided rule. Result will be cleaned up (@see cleanupTree).
     * @param root Single engine node or root node with a list of engines.
     * @param includeNode Filter function must return true for nodes, which are to be kept in the
     *     tree with all their children.
     */
    static NodePtr filterTreeInclusive(NodePtr root, NodeFilter includeNode);

    /**
     * Tree of the Event type ids. Root nodes are Engines, then Groups and Event types as leaves.
     * Includes all Event types, which can theoretically be available on these Devices, so all
     * compatible Engines are used.
     * Event types are intersected, empty Groups and Engines are removed from the output.
     */
    static NodePtr eventTypesForRulesPurposes(
        SystemContext* systemContext,
        const QnVirtualCameraResourceList& devices,
        const UnresolvedEntities& additionalUnresolvedEventTypes);
};

class AnalyticsEventsSearchTreeBuilder: public QObject, public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit AnalyticsEventsSearchTreeBuilder(
        SystemContext* systemContext,
        QObject* parent = nullptr);
    ~AnalyticsEventsSearchTreeBuilder();

    /**
     * Tree of the Event type ids. Root nodes are Engines, then Groups and Event types as leaves.
     * Includes all Event types, which can theoretically be available in the System, so all
     * compatible Engines are used. Includes only those Event types, which are actually used in the
     * existing VMS Rules. Empty Groups and Engines are removed from the output.
     */
    AnalyticsEntitiesTreeBuilder::NodePtr eventTypesTree() const;

signals:
    void eventTypesTreeChanged();

private:
    void updateEventTypesTree();
    AnalyticsEntitiesTreeBuilder::NodePtr calculateEventTypesTree() const;

private:
    std::atomic_bool dirty = false;
    using EventTypesTreeFuture = QFuture<void>;
    EventTypesTreeFuture eventTypesTreeFuture;
    mutable QMutex mutex;
    AnalyticsEntitiesTreeBuilder::NodePtr cachedEventTypesTree;
};

class AnalyticsObjectsSearchTreeBuilder: public QObject, public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit AnalyticsObjectsSearchTreeBuilder(
        SystemContext* systemContext,
        QObject* parent = nullptr);

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
