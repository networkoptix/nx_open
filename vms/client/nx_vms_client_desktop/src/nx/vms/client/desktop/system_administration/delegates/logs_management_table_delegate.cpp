// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_table_delegate.h"

#include <QtGui/QPainter>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

#include "../models/logs_management_model.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr int kExtraTextMargin = 5;

} // namespace

using Model = LogsManagementModel;

LogsManagementTableDelegate::LogsManagementTableDelegate(QObject* parent):
    base_type(parent)
{
}

void LogsManagementTableDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    if (index.column() != Model::NameColumn)
    {
        base_type::paint(painter, styleOption, index);
        return;
    }

    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    QStyle* style = option.widget->style();
    const int kOffset = -2;
    // Obtain sub-element rectangles
    QRect textRect = style->subElementRect(
        QStyle::SE_ItemViewItemText,
        &option,
        option.widget);
    textRect.setLeft(textRect.left() + kOffset);
    QRect iconRect = style->subElementRect(
        QStyle::SE_ItemViewItemDecoration,
        &option,
        option.widget);
    iconRect.translate(kOffset, 0);

    // Paint background
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Draw icon
    if (option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        option.icon.paint(painter, iconRect, option.decorationAlignment, QIcon::Normal, QIcon::On);

    // Draw text
    const QString extraInfo = index.data(Model::IpAddressRole).toString();
    const QColor textColor = colorTheme()->color("light10");
    const QColor ipAddressColor = colorTheme()->color("dark17");

    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; //< As in Qt.
    const int textEnd = textRect.right() - textPadding + 1;

    QPoint textPos = textRect.topLeft()
        + QPoint(textPadding, option.fontMetrics.ascent()
            + std::ceil((textRect.height() - option.fontMetrics.height()) / 2.0));

    if (textEnd > textPos.x())
    {
        const auto main = m_textPixmapCache.pixmap(
            option.text,
            option.font,
            textColor,
            textEnd - textPos.x() + 1,
            option.textElideMode);

        if (!main.pixmap.isNull())
        {
            painter->drawPixmap(textPos + main.origin, main.pixmap);
            textPos.rx() += main.origin.x() + main.size().width() + kExtraTextMargin;
        }

        if (textEnd > textPos.x() && !main.elided() && !extraInfo.isEmpty())
        {
            option.font.setWeight(QFont::Normal);

            const auto extra = m_textPixmapCache.pixmap(
                extraInfo,
                option.font,
                ipAddressColor,
                textEnd - textPos.x(),
                option.textElideMode);

            if (!extra.pixmap.isNull())
                painter->drawPixmap(textPos + extra.origin, extra.pixmap);
        }
    }
}

} // namespace nx::vms::client::desktop
