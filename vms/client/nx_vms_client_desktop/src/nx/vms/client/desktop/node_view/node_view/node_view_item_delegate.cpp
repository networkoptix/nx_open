// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "node_view_item_delegate.h"

#include "../details/node/view_node_helper.h"
#include "../details/node/view_node_constants.h"

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>
#include <utils/common/scoped_painter_rollback.h>


namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

namespace {

    using namespace nx::vms::client::desktop::node_view::details;

    void drawSwitch(
        QPainter* painter,
        QStyleOptionViewItem option)
    {
        const auto style = Style::instance();
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
        const auto progress = ViewNodeHelper(index).progressValue(index.column());
        if (progress < 0)
            return;

        static const QColor kChartColor = core::colorTheme()->color("brand");

        const QnScopedPainterPenRollback penRollback(painter, kChartColor);
        const QnScopedPainterBrushRollback brushRollback(painter, kChartColor);
        auto rect = option.rect.adjusted(style::Metrics::kStandardPadding, 1, -1, -1);
        rect.setWidth(qFuzzyIsNull(progress) ? 2 : rect.width() * progress / 100.0);
        painter->drawRect(rect);
    }

} // namespace

NodeViewItemDelegate::NodeViewItemDelegate(QObject* parent): base_type(parent)
{
}

void NodeViewItemDelegate::paint(
    QPainter* painter, const QStyleOptionViewItem& styleOption, const QModelIndex& index) const
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

    tryDrawProgressBackground(painter, option, index);

    // Sets opacity if disabled.
    QnScopedPainterOpacityRollback opacityRollback(painter);
    if (!option.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);

    base_type::paint(painter, styleOption, index);
}

void NodeViewItemDelegate::initStyleOption(
    QStyleOptionViewItem* option, const QModelIndex& index) const
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

} // namespace node_view
} // namespace nx::vms::client::desktop
