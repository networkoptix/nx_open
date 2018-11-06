#include "time_synchronization_servers_delegate.h"

#include "../models/time_synchronization_servers_model.h"

#include <nx/client/desktop/ui/common/color_theme.h>
#include <ui/style/helper.h>
#include <utils/common/scoped_painter_rollback.h>

namespace nx::client::desktop {

using Model = TimeSynchronizationServersModel;

namespace {

static constexpr int kMinimumDateTimeWidth = 84;
static constexpr int kRowHeight = 24;
static constexpr int kExtraTextMargin = 5;

bool isRowSelected(const QModelIndex& index)
{
    const auto checkboxIndex = index.sibling(index.row(), Model::CheckboxColumn);
    return checkboxIndex.data(Qt::CheckStateRole) == Qt::Checked;
}

} // namespace

TimeSynchronizationServersDelegate::TimeSynchronizationServersDelegate(QObject* parent):
    base_type(parent)
{
}

void TimeSynchronizationServersDelegate::paint(
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

    /* Obtain sub-element rectangles: */
    QRect textRect = style->subElementRect(
        QStyle::SE_ItemViewItemText,
        &option,
        option.widget);
    textRect.setLeft(textRect.left());
    QRect iconRect = style->subElementRect(
        QStyle::SE_ItemViewItemDecoration,
        &option,
        option.widget);
    iconRect.setLeft(iconRect.left());

    const bool selected = isRowSelected(index);

    /* Paint background: */
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    /* Draw icon: */
    const QIcon::Mode iconMode = selected ? QIcon::Selected : QIcon::Normal;
    if (option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        option.icon.paint(painter, iconRect, option.decorationAlignment, iconMode, QIcon::On);

    /* Draw text: */
    const QString extraInfo = index.data(Model::IpAddressRole).toString();
    const QColor textColor = selected
        ? colorTheme()->color("light4")
        : colorTheme()->color("light10");
    const QColor ipAddressColor = selected
        ? colorTheme()->color("light10")
        : colorTheme()->color("dark17");

    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; /* As in Qt */
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

QSize TimeSynchronizationServersDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    QSize size = base_type::sizeHint(option, index);
    switch (index.column())
    {
        case Model::DateColumn:
        case Model::OsTimeColumn:
        case Model::VmsTimeColumn:
            size.setWidth(std::max(size.width(), kMinimumDateTimeWidth));
            break;

        default:
            break;
    }
    size.setHeight(kRowHeight);
    return size;
}

} // namespace nx::client::desktop
