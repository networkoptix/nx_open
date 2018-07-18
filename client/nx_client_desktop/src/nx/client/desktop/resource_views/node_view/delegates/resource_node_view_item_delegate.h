#pragma once

#include <nx/client/desktop/resource_views/node_view/delegates/node_view_item_delegate.h>

class QnResourceItemColors;

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

    const QnResourceItemColors& colors() const;
    void setColors(const QnResourceItemColors& colors);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
