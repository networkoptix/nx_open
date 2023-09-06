// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_node_view_item_delegate.h"

#include "../resource_view_node_helpers.h"
#include "../../details/node/view_node_helper.h"

#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <ui/common/text_pixmap_cache.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

ResourceNodeViewItemDelegate::ResourceNodeViewItemDelegate(QObject* parent):
    base_type(parent),
    m_textPixmapCache(new QnTextPixmapCache())
{
}

ResourceNodeViewItemDelegate::~ResourceNodeViewItemDelegate()
{
}

void ResourceNodeViewItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    if (isResourceColumn(index))
    {
        QStyleOptionViewItem option(styleOption);
        initStyleOption(&option, index);

        const auto color = index.data(Qt::ForegroundRole);
        const bool useCustomColor = color.isValid();

        const auto textColor = useCustomColor
            ? color.value<QColor>()
            : core::colorTheme()->color("resourceTree.mainText");
        const auto extraColor = useCustomColor
            ? textColor
            : core::colorTheme()->color("resourceTree.extraText");

        paintItemText(painter, option, index, textColor, extraColor);
        paintItemIcon(painter, option, index, QIcon::Normal);
    }
    else
    {
        base_type::paint(painter, styleOption, index);
    }
}

void ResourceNodeViewItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    // Font options initialization.
    option->font.setWeight(QFont::DemiBold);
    option->fontMetrics = QFontMetrics(option->font);

    if (!option->state.testFlag(QStyle::State_Enabled))
        option->state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
}

void ResourceNodeViewItemDelegate::paintItemText(
    QPainter* painter,
    QStyleOptionViewItem& option,
    const QModelIndex& index,
    const QColor& mainColor,
    const QColor& extraColor) const
{
    const auto style = option.widget ? option.widget->style() : QApplication::style();
    auto baseColor = mainColor;
    if (option.features.testFlag(QStyleOptionViewItem::HasCheckIndicator))
    {
        baseColor.setAlphaF(option.palette.color(QPalette::Text).alphaF());
        option.palette.setColor(QPalette::Text, baseColor);
        style->drawControl(QStyle::CE_ItemViewItem, &option, painter, option.widget);
    }

    // Sub-element rectangles obtaining.
    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);

    // Background painting.
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Sets opacity if disabled.
    QnScopedPainterOpacityRollback opacityRollback(painter);
    if (!option.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);

    // Text drawing.
    const auto nodeText = ViewNodeHelper(index).text(index.column());
    const auto nodeExtraText = extraText(index);

    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; /* As in Qt */
    const int textEnd = textRect.right() - textPadding + 1;

    static constexpr int kExtraTextMargin = 5;
    QPoint textPos = textRect.topLeft() + QPoint(textPadding, option.fontMetrics.ascent()
        + qCeil((textRect.height() - option.fontMetrics.height()) / 2.0));

    if (textEnd > textPos.x())
    {
        const auto devicePixelRatio = painter->device()->devicePixelRatio();

        const auto main = m_textPixmapCache->pixmap(nodeText, option.font, baseColor,
            devicePixelRatio, textEnd - textPos.x() + 1, option.textElideMode);

        if (!main.pixmap.isNull())
        {
            painter->drawPixmap(textPos + main.origin, main.pixmap);
            textPos.rx() += main.origin.x() + main.size().width() + kExtraTextMargin;
        }

        if (textEnd > textPos.x() && !main.elided() && !nodeExtraText.isEmpty())
        {
            option.font.setWeight(QFont::Normal);

            const auto extra = m_textPixmapCache->pixmap(nodeExtraText, option.font,
                extraColor, devicePixelRatio, textEnd - textPos.x(), option.textElideMode);

            if (!extra.pixmap.isNull())
                painter->drawPixmap(textPos + extra.origin, extra.pixmap);
        }
    }
}

void ResourceNodeViewItemDelegate::paintItemIcon(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index,
    QIcon::Mode mode) const
{
    if (!option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        return;

    const auto style = option.widget ? option.widget->style() : QApplication::style();
    const QRect iconRect = style->subElementRect(
        QStyle::SE_ItemViewItemDecoration, &option, option.widget);
    option.icon.paint(painter, iconRect, option.decorationAlignment, mode, QIcon::On);
}

} // namespace node_view
} // namespace nx::vms::client::desktop
