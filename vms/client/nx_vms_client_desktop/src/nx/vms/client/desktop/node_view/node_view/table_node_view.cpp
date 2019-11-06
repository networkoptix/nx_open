#include "table_node_view.h"

#include "../details/node/view_node.h"
#include "../details/node_view_model.h"
#include "../details/node_view_store.h"
#include "../details/node_view_state.h"
#include "../details/node_view_state_patch.h"
#include "../details/node/view_node_helpers.h"
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
    TableNodeView* const q;
    NodeViewStore store;
    TableNodeViewModel model;
};

TableNodeView::Private::Private(TableNodeView* owner, int columnCount):
    q(owner),
    model(columnCount, &store)
{
    connect(&store, &NodeViewStore::patchApplied, &model, &TableNodeViewModel::applyPatch);
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
    d->store.applyPatch(patch);
}

const details::NodeViewState& TableNodeView::state() const
{
    return d->store.state();
}

void TableNodeView::setHeaderDataProvider(HeaderDataProvider&& provider)
{
    d->model.setHeaderDataProvider(std::move(provider));
}

const details::NodeViewStore& TableNodeView::store() const
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

    const auto node = details::nodeFromIndex(index);
    const auto checkedState = value.value<Qt::CheckState>();
    d->store.applyPatch(NodeViewStateReducer::setNodeChecked(
        d->store.state(), node->path(), {index.column()}, checkedState, true));

    static const QVector<int> kCheckStateRole(Qt::CheckStateRole, 1);
    d->model.dataChanged(index, index, kCheckStateRole);
}

} // node_view
} // namespace nx::vms::client::desktop

