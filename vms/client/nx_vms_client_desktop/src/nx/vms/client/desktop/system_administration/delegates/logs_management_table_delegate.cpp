// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_table_delegate.h"

#include <QtGui/QPainter>
#include <QtGui/QGuiApplication>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/core/skin/color_theme.h>

#include "../models/logs_management_model.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr int kExtraTextMargin = 5;
static constexpr qreal kOpacityForDisabledCheckbox = 0.3;

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
    switch (index.column())
    {
        case Model::NameColumn:
            paintNameColumn(painter, styleOption, index);
            break;

        case Model::CheckBoxColumn:
            paintCheckBoxColumn(painter, styleOption, index);
            break;

        default:
            base_type::paint(painter, styleOption, index);
            break;
    }
}

void LogsManagementTableDelegate::paintNameColumn(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    QStyle* style = option.widget->style();
    const int kOffset = -2;

    // Obtain sub-element rectangles.
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

    // Paint background.
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Draw icon.
    if (option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        option.icon.paint(painter, iconRect, option.decorationAlignment, QIcon::Normal, QIcon::On);

    // Draw text.
    const QString extraInfo = index.data(Model::IpAddressRole).toString();
    const QColor textColor = core::colorTheme()->color("light10");
    const QColor ipAddressColor = core::colorTheme()->color("dark17");

    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; //< As in Qt.
    const int textEnd = textRect.right() - textPadding + 1;

    QPoint textPos = textRect.topLeft()
        + QPoint(textPadding, option.fontMetrics.ascent()
            + std::ceil((textRect.height() - option.fontMetrics.height()) / 2.0));

    if (textEnd > textPos.x())
    {
        const auto devicePixelRatio = painter->device()->devicePixelRatio();

        const auto main = m_textPixmapCache.pixmap(
            option.text,
            option.font,
            textColor,
            devicePixelRatio,
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
                devicePixelRatio,
                textEnd - textPos.x(),
                option.textElideMode);

            if (!extra.pixmap.isNull())
                painter->drawPixmap(textPos + extra.origin, extra.pixmap);
        }
    }
}

void LogsManagementTableDelegate::paintCheckBoxColumn(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    QStyle* style = option.widget->style();

    // Paint background.
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    const auto checkStateData = index.data(Qt::CheckStateRole);
    if (checkStateData.isNull())
        return;

    QStyleOptionViewItem checkMarkOption(styleOption);
    checkMarkOption.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;

    checkMarkOption.features = QStyleOptionViewItem::HasCheckIndicator;

    switch (checkStateData.value<Qt::CheckState>())
    {
        case Qt::Checked:
            checkMarkOption.state |= QStyle::State_On;
            break;
        case Qt::PartiallyChecked:
            checkMarkOption.state |= QStyle::State_NoChange;
            break;
        case Qt::Unchecked:
            checkMarkOption.state |= QStyle::State_Off;
            break;
    }

    painter->save();
    if (!index.data(LogsManagementModel::EnabledRole).toBool())
        painter->setOpacity(kOpacityForDisabledCheckbox);

    const auto widget = styleOption.widget;
    checkMarkOption.rect =
        style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &checkMarkOption, widget);
    style->drawPrimitive(
        QStyle::PE_IndicatorItemViewItemCheck, &checkMarkOption, painter, widget);
    painter->restore();
}

} // namespace nx::vms::client::desktop
