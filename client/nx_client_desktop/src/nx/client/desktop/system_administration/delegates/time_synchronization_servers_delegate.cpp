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

static const QList<QColor> kOffsetColors = {
    QColor::fromRgb(0x91, 0xa7, 0xb2),
    QColor::fromRgb(0xac, 0xac, 0x87),
    QColor::fromRgb(0xc6, 0xb1, 0x5c),
    QColor::fromRgb(0xe0, 0xb7, 0x30),
    QColor::fromRgb(0xc6, 0xb1, 0x5c),
    QColor::fromRgb(0xf8, 0x99, 0x0f),
    QColor::fromRgb(0xf6, 0x74, 0x19),
    QColor::fromRgb(0xf3, 0x50, 0x22),
    QColor::fromRgb(0xf0, 0x2c, 0x2c)
};

static const QList<int> kOffsetThresholds = {2, 5, 10, 20, 30, 60, 300, 1800};

inline const QColor& offsetColor(qint64 mSecs)
{
    auto offset = mSecs;
    if (offset < 0)
        offset = -offset;
    offset = (offset + 500) / 1000;

    int index = std::upper_bound(kOffsetThresholds.begin(), kOffsetThresholds.end(), offset)
        - kOffsetThresholds.begin();

    return kOffsetColors[qBound(0, index, kOffsetColors.size() - 1)];
}

QString offsetString(qint64 mSecs)
{
    QString result;
    if (mSecs > 0)
    {
        result = '+';
    }
    else
    {
        result = '-';
        mSecs = -mSecs;
    }
    
    auto rest = (mSecs + 500) / 1000;
    if (!rest)
        return QString();
    
    auto secs = rest % 60;
    rest /= 60;
    
    auto mins = rest % 60;
    rest = rest /= 60;

    auto hours = rest % 24;
    rest = rest / 24;

    auto days = rest;

    if (days)
        result += days + "d ";
    if (hours)
        result += hours + "h ";
    if (mins)
        result += mins + "m ";
    result += QString::number(secs) + "s"; // Show "0s" to indicate that the offset value is not rounded?

    return result;
}

bool isRowSelected(const QModelIndex& index)
{
    const auto checkboxIndex = index.sibling(index.row(), Model::CheckboxColumn);
    return checkboxIndex.data(Qt::CheckStateRole) == Qt::Checked;
}

} // namespace

TimeSynchronizationServersDelegate::TimeSynchronizationServersDelegate(QObject* parent):
    base_type(parent),
    m_baseRow(-1)
{
}

void TimeSynchronizationServersDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    if (index.column() == Model::NameColumn)
    {
        paintName(painter, styleOption, index);
    }
    else if (index.column() == Model::OsTimeColumn || index.column() == Model::VmsTimeColumn)
    {
        paintTime(painter, styleOption, index);
    }
    else
    {
        base_type::paint(painter, styleOption, index);
    }
}

void TimeSynchronizationServersDelegate::paintName(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    QStyle* style = option.widget->style();
    const int kOffset = -8;
    /* Obtain sub-element rectangles: */
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

void TimeSynchronizationServersDelegate::paintTime(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    QStyle* style = option.widget->style();

    /* Obtain sub-element rectangles: */
    QRect textRect = style->subElementRect(
        QStyle::SE_ItemViewItemText,
        &option,
        option.widget);
    textRect.setLeft(textRect.left());

    const bool selected = isRowSelected(index);

    /* Paint background: */
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    /* Draw text: */
    const QColor textColor = selected
        ? colorTheme()->color("light4")
        : colorTheme()->color("light10");
    
    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; /* As in Qt */
    const int textEnd = textRect.right() - textPadding + 1;

    QPoint textPos = textRect.topLeft()
        + QPoint(textPadding, option.fontMetrics.ascent()
            + std::ceil((textRect.height() - option.fontMetrics.height()) / 2.0));

    /* Prepare offset string.  */
    auto timeOffset = index.data(Model::TimeOffsetRole).toLongLong();
    timeOffset -= index.sibling(m_baseRow, index.column()).data(Model::TimeOffsetRole).toLongLong();

    const bool showOffset = (m_baseRow >= 0) && index.data(Model::ServerOnlineRole).toBool()
        && index.sibling(m_baseRow, index.column()).data(Model::ServerOnlineRole).toBool();

    const QString extraInfo = showOffset ? offsetString(timeOffset) : "";
    const QColor extraColor = offsetColor(timeOffset);

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
                extraColor,
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

void TimeSynchronizationServersDelegate::setBaseRow(int row)
{
    m_baseRow = row;
}

} // namespace nx::client::desktop
