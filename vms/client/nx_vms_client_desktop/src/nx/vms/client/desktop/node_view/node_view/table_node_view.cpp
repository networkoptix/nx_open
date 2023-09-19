// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "table_node_view.h"

#include "../details/node/view_node.h"
#include "../details/node_view_model.h"
#include "../details/node_view_store.h"
#include "../details/node_view_state.h"
#include "../details/node_view_state_patch.h"
#include "../details/node/view_node_helper.h"
#include "../details/node/view_node_data_builder.h"
#include "../node_view/node_view_state_reducer.h"
#include "../node_view/node_view_item_delegate.h"

namespace {

using namespace nx::vms::client::desktop;
using HeaderDataProvider = node_view::TableNodeView::HeaderDataProvider;

class TableNodeViewModel: public node_view::details::NodeViewModel
{
    using base_type = node_view::details::NodeViewModel;

public:
    TableNodeViewModel(int columnCount, QObject* parent);

    void setHeaderDataProvider(HeaderDataProvider&& provider);

    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    HeaderDataProvider m_headerDataProvider;
};

TableNodeViewModel::TableNodeViewModel(int columnCount, QObject* parent):
    base_type(columnCount, parent)
{
}

void TableNodeViewModel::setHeaderDataProvider(HeaderDataProvider&& provider)
{
    m_headerDataProvider = provider;

    if (const int count = columnCount())
        emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);

    if (const int count = rowCount())
        emit headerDataChanged(Qt::Vertical, 0, count - 1);
}

QVariant TableNodeViewModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    QVariant result;
    return m_headerDataProvider && m_headerDataProvider(section, orientation, role, result)
        ? result
        : base_type::headerData(section, orientation, role);
}

} // namespace

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

struct TableNodeView::Private: public QObject
{
    Private(TableNodeView* ownerd, int columnCount);
    void updateCheckedIndices();
    void setHasUserChanges(bool value);

    TableNodeView* const q;
    NodeViewStorePtr store;
    TableNodeViewModel model;

    bool hasUserChanges = false;
    IndicesList checkedIndices;
};

TableNodeView::Private::Private(TableNodeView* owner, int columnCount):
    q(owner),
    store(new NodeViewStore()),
    model(columnCount, store.data())
{
    connect(store.data(), &NodeViewStore::patchApplied, &model, &TableNodeViewModel::applyPatch);
    connect(store.data(), &NodeViewStore::patchApplied, this, &Private::updateCheckedIndices);
}

void TableNodeView::Private::updateCheckedIndices()
{
    TableNodeView::IndicesList current;
    ViewNodeHelper::ViewNodeHelper::forEachNode(store->state().rootNode,
        [this, &current](const NodePtr& node)
        {
            const auto& data = node->data();
            for (const int column: data.usedColumns())
            {
                if (data.hasData(column, ViewNodeHelper::makeUserActionRole(Qt::CheckStateRole)))
                    current.append(model.index(node, column));
            }
        });

    if (current == checkedIndices)
        return;

    checkedIndices = current;

    setHasUserChanges(!checkedIndices.isEmpty());
}

void TableNodeView::Private::setHasUserChanges(bool value)
{
    if (hasUserChanges == value)
        return;

    hasUserChanges = value;
    emit q->hasUserChangesChanged();
}

//--------------------------------------------------------------------------------------------------

TableNodeView::TableNodeView(int columnCount, QWidget* parent):
    base_type(parent),
    d(new Private(this, columnCount))
{
    setItemDelegate(new NodeViewItemDelegate(this));
    setModel(&d->model);
    connect(&d->model, &details::NodeViewModel::dataChangeRequestOccured,
        this, &TableNodeView::handleUserDataChangeRequested);
}

TableNodeView::~TableNodeView()
{
}

void TableNodeView::applyPatch(const details::NodeViewStatePatch& patch)
{
    d->store->applyPatch(patch);
}

const details::NodeViewState& TableNodeView::state() const
{
    return d->store->state();
}

void TableNodeView::setHeaderDataProvider(HeaderDataProvider&& provider)
{
    d->model.setHeaderDataProvider(std::move(provider));
}

TableNodeView::IndicesList TableNodeView::userCheckChangedIndices() const
{
    return d->checkedIndices;
}

bool TableNodeView::hasUserChanges() const
{
    return !d->checkedIndices.isEmpty();
}

void TableNodeView::applyUserChanges()
{
    applyPatch(NodeViewStateReducer::applyUserChangesPatch(d->store->state()));
}

const details::NodeViewStorePtr& TableNodeView::store()
{
    return d->store;
}

void TableNodeView::handleUserDataChangeRequested(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
    if (role != Qt::CheckStateRole)
        return;

    const auto node = details::ViewNodeHelper::nodeFromIndex(index);
    const auto checkedState = value.value<Qt::CheckState>();
    d->store->applyPatch(NodeViewStateReducer::setNodeChecked(
        d->store->state(), node->path(), {index.column()}, checkedState, true));

    static const QVector<int> kCheckStateRole(Qt::CheckStateRole, 1);
    d->model.dataChanged(index, index, kCheckStateRole);
}

} // node_view
} // namespace nx::vms::client::desktop
