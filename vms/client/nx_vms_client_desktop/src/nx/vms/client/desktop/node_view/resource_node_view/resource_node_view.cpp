#include "resource_node_view.h"

#include "details/resource_node_view_item_delegate.h"

namespace {

static constexpr int kDefaultColumnsCount = 1;

} // namespace

namespace nx::vms::client::desktop {
namespace node_view {

struct ResourceNodeView::Private
{
    Private(ResourceNodeView* owner);

    ResourceNodeViewItemDelegate itemDelegate;
};

ResourceNodeView::Private::Private(ResourceNodeView* owner):
    itemDelegate(owner)
{
}

//-------------------------------------------------------------------------------------------------

ResourceNodeView::ResourceNodeView(QWidget* parent):
    base_type(kDefaultColumnsCount, parent),
    d(new Private(this))
{
    setItemDelegate(&d->itemDelegate);
}

ResourceNodeView::~ResourceNodeView()
{
}

} // namespace node_view
} // namespace nx::vms::client::desktop
