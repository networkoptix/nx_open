#pragma once

#include "resource_node_view_item_delegate.h"

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

class ResourceNodeViewItemSelectionDelegate: public ResourceNodeViewItemDelegate
{
    Q_OBJECT
    using base_type = ResourceNodeViewItemDelegate;

public:
    ResourceNodeViewItemSelectionDelegate(
        QTreeView* owner,
        const details::ColumnSet& selectionColumns,
        QObject* parent = nullptr);
    virtual ~ResourceNodeViewItemSelectionDelegate() override;

    virtual void paint(
        QPainter *painter,
        const QStyleOptionViewItem &styleOption,
        const QModelIndex &index) const override;

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

private:
    details::ColumnSet m_selectionColumns;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
