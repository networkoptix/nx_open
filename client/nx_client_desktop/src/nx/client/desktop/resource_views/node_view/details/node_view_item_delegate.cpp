#include "node_view_item_delegate.h"

#include <QtWidgets/QTreeView>

#include <nx/client/desktop/resource_views/node_view/node/view_node_helpers.h>
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
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    if (option.features == QStyleOptionViewItem::None)
    {
        const int y = option.rect.top() + option.rect.height() / 2;
        const QnScopedPainterPenRollback penRollback(
            painter, option.palette.color(QPalette::Midlight));
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

    if (node_view::helpers::isSeparator(index))
        option->features = QStyleOptionViewItem::None;
    else
        option->features |= QStyleOptionViewItem::HasDisplay;
}

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx
