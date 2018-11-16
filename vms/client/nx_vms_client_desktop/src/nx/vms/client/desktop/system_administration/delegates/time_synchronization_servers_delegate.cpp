#include "time_synchronization_servers_delegate.h"

#include <cmath>

#include <nx/client/core/utils/human_readable.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/style/helper.h>
#include <utils/common/scoped_painter_rollback.h>

#include "../models/time_synchronization_servers_model.h"

namespace nx::vms::client::desktop {

using Model = TimeSynchronizationServersModel;

namespace {

using namespace std::chrono_literals;

static constexpr int kMinimumDateWidth = 84;
static constexpr int kMinimumTimeWidth = 128;
static constexpr int kRowHeight = 24;
static constexpr int kExtraTextMargin = 5;
static constexpr int kTimeOffsetFontSize = 11;

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

static const QList<std::chrono::seconds> kOffsetThresholds = {2s, 5s, 10s, 20s, 30s, 1min, 5min, 30min};

inline const QColor& offsetColor(std::chrono::seconds offset)
{
    if (offset.count() < 0)
        offset = -offset;

    int index = std::lower_bound(kOffsetThresholds.begin(), kOffsetThresholds.end(), offset)
        - kOffsetThresholds.begin();

    return kOffsetColors[qBound(0, index, kOffsetColors.size() - 1)];
}

QString offsetString(std::chrono::seconds offset)
{
    if (!offset.count())
        return QString();

    using nx::vms::client::core::HumanReadable;
    QString result = HumanReadable::timeSpan(offset,
        HumanReadable::TimeSpanUnit::DaysAndTime,
        HumanReadable::SuffixFormat::Short,
        QString(" "),
        HumanReadable::kNoSuppressSecondUnit);

    if (offset.count() > 0)
        result = "+" + result;

    return result;
 }

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

    const bool selected = isRowSelected(index);

    // Paint background
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Draw icon
    const QIcon::Mode iconMode = selected ? QIcon::Selected : QIcon::Normal;
    if (option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        option.icon.paint(painter, iconRect, option.decorationAlignment, iconMode, QIcon::On);

    // Draw text
    const QString extraInfo = index.data(Model::IpAddressRole).toString();
    const QColor textColor = selected
        ? colorTheme()->color("light4")
        : colorTheme()->color("light10");
    const QColor ipAddressColor = selected
        ? colorTheme()->color("light10")
        : colorTheme()->color("dark17");

    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; // As in Qt
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

    // Obtain sub-element rectangles
    QRect textRect = style->subElementRect(
        QStyle::SE_ItemViewItemText,
        &option,
        option.widget);
    textRect.setLeft(textRect.left());

    const bool selected = isRowSelected(index);

    // Paint background
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Draw text
    const QColor textColor = selected
        ? colorTheme()->color("light4")
        : colorTheme()->color("light10");

    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1; // As in Qt
    const int textEnd = textRect.right() - textPadding + 1;

    QPoint textPos = textRect.topLeft()
        + QPoint(textPadding, option.fontMetrics.ascent()
            + std::ceil((textRect.height() - option.fontMetrics.height()) / 2.0));

    // Prepare offset string
    auto timeOffset = index.data(Model::TimeOffsetRole).toLongLong();
    timeOffset -= index.sibling(m_baseRow, index.column()).data(Model::TimeOffsetRole).toLongLong();

    const bool showOffset = (m_baseRow >= 0) && index.data(Model::ServerOnlineRole).toBool()
        && index.sibling(m_baseRow, index.column()).data(Model::ServerOnlineRole).toBool();

    auto roundedOffset = std::chrono::round<std::chrono::seconds>(std::chrono::milliseconds(timeOffset));

    const QString extraInfo = showOffset ? offsetString(roundedOffset) : "";
    const QColor extraColor = offsetColor(roundedOffset);

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
            option.font.setPixelSize(kTimeOffsetFontSize);

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
            size.setWidth(std::max(size.width(), kMinimumDateWidth));
            break;
        case Model::OsTimeColumn:
        case Model::VmsTimeColumn:
            size.setWidth(std::max(size.width(), kMinimumTimeWidth));
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

} // namespace nx::vms::client::desktop
