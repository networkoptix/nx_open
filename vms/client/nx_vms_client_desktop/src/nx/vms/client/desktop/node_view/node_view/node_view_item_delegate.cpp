#include "node_view_item_delegate.h"

#include "../details/node/view_node_helpers.h"

//#include <ui/style/helper.h>
#include <ui/style/nx_style.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

using namespace nx::vms::client::desktop::node_view::details;

void drawSwitch(
    QPainter* painter,
    QStyleOptionViewItem option)
{
    const auto style = QnNxStyle::instance();
    if (!style)
        return;

    /* Draw background and focus marker: */
    option.features &= ~(QStyleOptionViewItem::HasDisplay
        | QStyleOptionViewItem::HasDecoration
        | QStyleOptionViewItem::HasCheckIndicator);
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter, option.widget);

    /* Draw switch without its own focus marker: */
    option.state &= ~QStyle::State_HasFocus;
    style->drawSwitch(painter, &option, option.widget);
}

void drawSeparator(
    QPainter* painter,
    QStyleOptionViewItem option)
{
    const int y = option.rect.top() + option.rect.height() / 2;
    const QnScopedPainterPenRollback penRollback(
        painter, option.palette.color(QPalette::Midlight));
    painter->drawLine(0, y, option.rect.right(), y);
}

void tryDrawProgressBackground(
    QPainter* painter,
    QStyleOptionViewItem option,
    const QModelIndex& index)
{
    const auto progress = progressValue(index);
    if (progress <= 0)
        return;

    const QColor progressColor(Qt::red);
    const QnScopedPainterPenRollback penRollback(painter, progressColor);
    const QnScopedPainterBrushRollback brushRollback(painter, progressColor);
    const auto sourceRect = option.rect;
    const auto size = QSize(sourceRect.width() * progress / 100.0, sourceRect.height());
    const auto targetRect = QRect(sourceRect.topLeft(), size);
    painter->drawRect(targetRect);
}

}

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

NodeViewItemDelegate::NodeViewItemDelegate(QObject* parent):
    base_type(parent)
{
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
        drawSeparator(painter, option);
        return;
    }

    if (useSwitchStyleForCheckbox(index))
    {
        drawSwitch(painter, option);
        return;
    }

    tryDrawProgressBackground(painter, option, index);

    base_type::paint(painter, styleOption, index);
}

void NodeViewItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    if (isSeparator(index))
        option->features = QStyleOptionViewItem::None;
    else
        option->features |= QStyleOptionViewItem::HasDisplay;


    if (option->checkState == Qt::Checked)
        option->state |= QStyle::State_On;
    else
        option->state |= QStyle::State_Off;
}

} // namespace node_view
} // namespace nx::vms::client::desktop
