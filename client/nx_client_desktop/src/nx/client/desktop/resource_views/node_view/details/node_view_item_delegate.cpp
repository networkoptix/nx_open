#include "node_view_item_delegate.h"

#include <QtWidgets/QTreeView>

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_helpers.h>

#include <utils/common/scoped_painter_rollback.h>

namespace nx {
namespace client {
namespace desktop {
namespace details {

NodeViewItemDelegate::NodeViewItemDelegate(QTreeView* owner, QObject* parent):
    base_type(parent),
    m_owner(owner)
{
}

QTreeView* NodeViewItemDelegate::owner() const
{
    return m_owner;
}

void NodeViewItemDelegate::paint(
    QPainter *painter,
    const QStyleOptionViewItem &styleOption,
    const QModelIndex &index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    if (option.features == QStyleOptionViewItem::None)
    {
        int y = option.rect.top() + option.rect.height() / 2;
        QnScopedPainterPenRollback penRollback(painter, option.palette.color(QPalette::Midlight));
        painter->drawLine(0, y, option.rect.right(), y);
    }
    else
    {
        base_type::paint(painter, styleOption, index);
    }
}

void NodeViewItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    const auto node = NodeViewModel::nodeFromIndex(details::getLeafIndex(index));
    if (!node)
        return;

    const bool isSeparator = node->data(node_view::nameColumn, node_view::separatorRole).toBool();
    if (isSeparator)
        option->features = QStyleOptionViewItem::None;
    else
        option->features |= QStyleOptionViewItem::HasDisplay;
}

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx
