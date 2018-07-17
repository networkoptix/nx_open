#include "resource_node_view_item_delegate.h"

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

namespace nx {
namespace client {
namespace desktop {

ResourceNodeViewItemDelegate::ResourceNodeViewItemDelegate(QTreeView* owner, QObject* parent):
    base_type(owner, parent)
{
}

void ResourceNodeViewItemDelegate::paint(
    QPainter *painter,
    const QStyleOptionViewItem &styleOption,
    const QModelIndex &index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    if (option.features.testFlag(QStyleOptionViewItem::HasCheckIndicator))
    {
        const auto style = option.widget ? option.widget->style() : QApplication::style();
//        mainColor.setAlphaF(option.palette.color(QPalette::Text).alphaF());
        option.palette.setColor(QPalette::Text, /*mainColor*/Qt::white);
        style->drawControl(QStyle::CE_ItemViewItem, &option, painter, option.widget);
    }
    else
    {
        base_type::paint(painter, option, index);
    }
}

} // namespace desktop
} // namespace client
} // namespace nx

