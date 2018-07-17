#pragma once

#include <nx/client/desktop/resource_views/node_view/delegates/node_view_item_delegate.h>

namespace nx {
namespace client {
namespace desktop {

class ResourceNodeViewItemDelegate: public NodeViewItemDelegate
{
    using base_type = NodeViewItemDelegate;

public:
    ResourceNodeViewItemDelegate(QTreeView* owner, QObject* parent);

    virtual void paint(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override;

private:
    QTreeView const * m_owner;
};

} // namespace desktop
} // namespace client
} // namespace nx
