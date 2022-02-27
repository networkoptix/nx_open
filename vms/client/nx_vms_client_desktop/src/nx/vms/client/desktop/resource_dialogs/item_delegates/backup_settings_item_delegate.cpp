// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_settings_item_delegate.h"

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStylePainter>

#include <client/client_globals.h>

#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/indents.h>
#include <nx/vms/client/desktop/style/style.h>
#include <ui/common/palette.h>

#include <utils/common/scoped_painter_rollback.h>

#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/common/widgets/tree_view.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_common.h>
#include <nx/vms/client/desktop/resource_properties/server/widgets/backup_settings_picker_widget.h>
#include <nx/vms/client/desktop/style/skin.h>

namespace {

static constexpr int kDropdownArrowSpacing = 4;
static constexpr int kWarningIconSpacing = 4;

} // namespace

namespace nx::vms::client::desktop {

using namespace backup_settings_view;
using namespace nx::vms::api;

BackupSettingsItemDelegate::BackupSettingsItemDelegate(
    ItemViewHoverTracker* hoverTracker,
    QObject* parent)
    :
    base_type(parent),
    m_hoverTracker(hoverTracker),
    m_warningPixmap(qnSkin->pixmap("tree/buggy.png"))
{
    NX_ASSERT(m_hoverTracker);
    initDropdownColorTable();
}

void BackupSettingsItemDelegate::setActiveDropdownIndex(const QModelIndex& index)
{
    m_activeDropdownIndex = index;
}

QSize BackupSettingsItemDelegate::sizeHint(
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    using namespace nx::style;

    if (isSeparator(index) || isSpacer(index))
        return base_type::sizeHint(styleOption, index);

    if (isDropdownColumn(index))
    {
        QStyleOptionViewItem option = styleOption;
        initStyleOption(&option, index);

        QString placeholderText;
        switch (index.column())
        {
            case ContentTypesColumn:
                placeholderText = BackupSettingsPickerWidget::backupContentTypesPlaceholderText();
                break;

            case QualityColumn:
                placeholderText = BackupSettingsPickerWidget::backupQualityPlaceholderText();
                break;

            default:
                NX_ASSERT(false, "Unexpected dropdown column");
                break;
        }

        const auto contentsWidth = option.fontMetrics.size(Qt::TextSingleLine, option.text).width()
            + Metrics::kStandardPadding * 2
            + Metrics::kArrowSize + kDropdownArrowSpacing;

        const auto pickerPlaceholderWidth =
            option.fontMetrics.size(Qt::TextSingleLine, placeholderText).width()
            + Metrics::kStandardPadding * 2
            + Metrics::kArrowSize + Metrics::kStandardPadding;

        return QSize(std::max(contentsWidth, pickerPlaceholderWidth), Metrics::kViewRowHeight);
    }
    return base_type::sizeHint(styleOption, index);
}

void BackupSettingsItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    if (index.column() == ContentTypesColumn
        && !index.siblingAtColumn(ResourceColumn).data().isNull()
        && !isBackupSupported(index))
    {
        // TODO: #vbreus Make the base class use palette from the style option correctly.
        QStyledItemDelegate::paint(painter, styleOption, index);
        return;
    }

    base_type::paint(painter, styleOption, index);

    if (isSeparator(index.siblingAtColumn(ResourceColumn)))
        return;

    if (index.column() == SwitchColumn)
        paintSwitch(painter, styleOption, index);

    if (isDropdownColumn(index) && hasDropdownData(index))
        paintDropdown(painter, styleOption, index);

    if (index.column() == WarningIconColumn)
        paintWarningIcon(painter, styleOption, index);
}

void BackupSettingsItemDelegate::paintSwitch(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    const auto style = Style::instance();
    if (!style)
        return;

    auto option = styleOption;

    const auto backupEnabledData = index.data(BackupEnabledRole);
    if (backupEnabledData.isNull())
        return;

    const auto backupEnabled = backupEnabledData.toBool();

    option.state.setFlag(QStyle::State_On, backupEnabled);
    option.state.setFlag(QStyle::State_Enabled, true);
    style->drawSwitch(painter, &option, option.widget);
}

void BackupSettingsItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    if (!isDropdownColumn(index))
        return;

    const auto textColor = m_dropdownColorTable.value(dropdownStateFlags(*option, index));

    option->font.setWeight(QFont::Normal);
    option->fontMetrics = QFontMetrics(option->font);
    option->palette.setColor(QPalette::Text, textColor);
    option->palette.setColor(QPalette::HighlightedText, textColor);

    if (index.column() == ContentTypesColumn)
    {
        const auto contentTypesData = index.data(BackupContentTypesRole);
        if (contentTypesData.isNull())
            return;

        const auto contentTypes = static_cast<BackupContentTypes>(contentTypesData.toInt());
        option->text = backupContentTypesToString(contentTypes);
    }
    if (index.column() == QualityColumn)
    {
        const auto qualityData = index.data(BackupQualityRole);
        if (qualityData.isNull())
            return;

        const auto quality = static_cast<CameraBackupQuality>(qualityData.toInt());
        option->text = backupQualityToString(quality);
    }
}

void BackupSettingsItemDelegate::paintWarningIcon(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    const auto nothingToBackupWarningData =
        index.siblingAtColumn(ContentTypesColumn).data(NothingToBackupWarningRole);

    if (nothingToBackupWarningData.isNull())
        return;

    const auto itemRect = styleOption.rect;
    const auto iconSize = m_warningPixmap.size() / m_warningPixmap.devicePixelRatio();

    const auto iconTopLeft = styleOption.rect.topLeft()
        + QPoint(itemRect.width() - iconSize.width(), (itemRect.height() - iconSize.height()) / 2);

    painter->drawPixmap(iconTopLeft, m_warningPixmap);
}

void BackupSettingsItemDelegate::paintDropdown(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    using namespace nx::style;

    QStyleOptionViewItem option = styleOption;
    initStyleOption(&option, index);

    if (option.text.isEmpty())
        return;

    QnScopedPainterPenRollback penRollback(painter, option.palette.color(QPalette::Text));

    QRect boundingRect;
    painter->drawText(
        option.rect.adjusted(Metrics::kStandardPadding, 0, 0, 0),
        Qt::AlignLeft | Qt::AlignVCenter,
        option.text,
        &boundingRect);

    option.rect.setLeft(boundingRect.right() + kDropdownArrowSpacing);
    option.rect.setRight(option.rect.left() + Metrics::kArrowSize);

    if (hasDropdownMenu(index))
    {
        const auto arrowDirection = m_activeDropdownIndex == index
            ? QStyle::PE_IndicatorArrowUp
            : QStyle::PE_IndicatorArrowDown;

        QStyle* style = option.widget ? option.widget->style() : QApplication::style();
        style->drawPrimitive(arrowDirection, &option, painter);
    }
}

BackupSettingsItemDelegate::DropdownStateFlags BackupSettingsItemDelegate::dropdownStateFlags(
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    const auto resourceIndex = index.siblingAtColumn(ResourceColumn);
    const auto backupEnabledData = index.siblingAtColumn(SwitchColumn).data(BackupEnabledRole);
    const bool backupEnabled = !backupEnabledData.isNull() && backupEnabledData.toBool();
    const auto isNewAddedCamerasRow = resourceIndex.data(NewAddedCamerasItemRole).toBool();

    DropdownStateFlags dropdownState;
    dropdownState.setFlag(DropdownStateFlag::enabled, backupEnabled || isNewAddedCamerasRow);
    dropdownState.setFlag(DropdownStateFlag::hovered,
        hasDropdownMenu(index) && m_hoverTracker->hoveredIndex() == index);

    dropdownState.setFlag(DropdownStateFlag::selected,
        styleOption.state.testFlag(QStyle::State_Selected));

    dropdownState.setFlag(DropdownStateFlag::warning,
        !index.data(NothingToBackupWarningRole).isNull());

    return dropdownState;
}

void BackupSettingsItemDelegate::initDropdownColorTable()
{
    m_dropdownColorTable.insert({},
        colorTheme()->color("dark14"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::selected},
        colorTheme()->color("light13"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::hovered},
        colorTheme()->color("dark14"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::selected, DropdownStateFlag::hovered},
        colorTheme()->color("light13"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled},
        colorTheme()->color("light10"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::selected},
        colorTheme()->color("light7"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::hovered},
        colorTheme()->color("light6"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::selected, DropdownStateFlag::hovered},
        colorTheme()->color("light4"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::warning},
        colorTheme()->color("yellow_d2"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::selected, DropdownStateFlag::warning},
        colorTheme()->color("yellow_d1"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::hovered, DropdownStateFlag::warning},
        colorTheme()->color("yellow_core"));

    m_dropdownColorTable.insert({DropdownStateFlag::enabled, DropdownStateFlag::selected,
        DropdownStateFlag::hovered, DropdownStateFlag::warning},
            colorTheme()->color("yellow_core"));
}

} // namespace nx::vms::client::desktop
