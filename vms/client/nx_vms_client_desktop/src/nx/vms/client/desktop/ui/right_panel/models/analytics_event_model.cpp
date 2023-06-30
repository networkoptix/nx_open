// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_event_model.h"

#include <common/common_module.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/trace/trace.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {

class AnalyticsEventModel::Private
{
    using NodePtr = core::AnalyticsEntitiesTreeBuilder::NodePtr;
    using Node = core::AnalyticsEntitiesTreeBuilder::Node;

    AnalyticsEventModel* const q;
    NodePtr rootNode;

public:
    Private(AnalyticsEventModel* q, core::AnalyticsEventsSearchTreeBuilder* builder): q(q)
    {
        const auto updateModelOperation =
            [this, builder]
            {
                NX_VERBOSE(this, "Rebuilding analytics event tree begin");
                NX_TRACE_SCOPE("RebuildAnalyticsEventTree");
                const ScopedReset reset(this->q);
                rootNode = builder->eventTypesTree();
                NX_VERBOSE(this, "Rebuilding analytics event tree completed");
            };

        connect(
            builder,
            &core::AnalyticsEventsSearchTreeBuilder::eventTypesTreeChanged,
            q,
            updateModelOperation);

        rootNode = builder->eventTypesTree();
    }

    const Node* getNode(const QModelIndex& index, CheckIndexOptions checkOptions = {}) const
    {
        if (!index.isValid())
            return rootNode.get();

        return NX_ASSERT(q->checkIndex(index, checkOptions))
            ? reinterpret_cast<Node*>(index.internalPointer())
            : nullptr;
    }
};

AnalyticsEventModel::AnalyticsEventModel(
    core::AnalyticsEventsSearchTreeBuilder* builder,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, builder))
{
}

AnalyticsEventModel::~AnalyticsEventModel()
{
    // Required here for forward-declared scoped pointer destruction.
}

QModelIndex AnalyticsEventModel::index(int row, int column, const QModelIndex& parent) const
{
    return (row >= 0 && row < rowCount(/*parent is checked here*/ parent) && column == 0)
        ? createIndex(row, column, d->getNode(parent)->children[row].get())
        : QModelIndex();
}

QModelIndex AnalyticsEventModel::parent(const QModelIndex& index) const
{
    const auto childNode = d->getNode(index, CheckIndexOption::DoNotUseParent);
    if (!childNode || !childNode->parent)
        return {};

    const auto node = childNode->parent.lock();
    const auto parentNode = node->parent.lock();

    if (!parentNode)
        return {};

    const auto row = std::find(parentNode->children.cbegin(), parentNode->children.cend(), node)
        - parentNode->children.cbegin();

    return createIndex(row, 0, node.get());
}

int AnalyticsEventModel::rowCount(const QModelIndex& parent) const
{
    const auto node = d->getNode(parent);
    return node ? (int) node->children.size() : 0;
}

int AnalyticsEventModel::columnCount(const QModelIndex& parent) const
{
    return NX_ASSERT(d->getNode(parent)) ? 1 : 0;
}

QVariant AnalyticsEventModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    const auto node = d->getNode(index);
    if (!node)
        return {};

    switch(role)
    {
        case NameRole:
            return node->text;
        case IdRole:
            return node->entityId;
        case EnabledRole:
            return node->nodeType == core::AnalyticsEntitiesTreeBuilder::NodeType::eventType;
        default:
            return {};
    }
}

QHash<int, QByteArray> AnalyticsEventModel::roleNames() const
{
    static const QHash<int, QByteArray> roleNames{
        {NameRole, "name"},
        {IdRole, "id"},
        {EnabledRole, "enabled"}};

    return roleNames;
}

} // namespace nx::vms::client::desktop
