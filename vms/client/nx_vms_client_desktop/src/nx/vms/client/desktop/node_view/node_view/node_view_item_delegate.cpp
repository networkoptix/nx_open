#include "node_view_item_delegate.h"

#include "../details/node/view_node_helper.h"
#include "../details/node/view_node_constants.h"

#include <ui/style/helper.h>
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
    const QColor& chartColor,
    const QModelIndex& index)
{
    const auto progress = ViewNodeHelper(index).progressValue(index.column());
    if (progress < 0)
        return;

    const QnScopedPainterPenRollback penRollback(painter, chartColor);
    const QnScopedPainterBrushRollback brushRollback(painter, chartColor);
    auto rect = option.rect.adjusted(style::Metrics::kStandardPadding, 1, -1, -1);
    rect.setWidth(qFuzzyIsNull(progress) ? 2 : rect.width() * progress / 100.0);
    painter->drawRect(rect);
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

    if (ViewNodeHelper(index).useSwitchStyleForCheckbox(index.column()))
    {
        drawSwitch(painter, option);
        return;
    }

    tryDrawProgressBackground(painter, option, m_colors.chartColor, index);

    // Sets opacity if disabled.
    QnScopedPainterOpacityRollback opacityRollback(painter);
    if (!option.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);

    base_type::paint(painter, styleOption, index);
}

void NodeViewItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    if (ViewNodeHelper(index).isSeparator())
        option->features = QStyleOptionViewItem::None;
    else
        option->features |= QStyleOptionViewItem::HasDisplay;

    if (option->checkState == Qt::Checked)
        option->state |= QStyle::State_On;
    else
        option->state |= QStyle::State_Off;

    if (!ViewNodeHelper(index).hoverable())
        option->state &= ~QStyle::State_MouseOver;

    const auto useItalicFont = index.data(useItalicFontRole);
    if (useItalicFont.isValid())
        option->font.setItalic(useItalicFont.toBool());
}

const NodeViewStatsColors& NodeViewItemDelegate::colors() const
{
    return m_colors;
}

void NodeViewItemDelegate::setColors(const NodeViewStatsColors& value)
{
    m_colors = value;
}

} // namespace node_view
} // namespace nx::vms::client::desktop
