// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_settings_item_delegate.h"

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStylePainter>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/tree_view.h>
#include <nx/vms/client/desktop/resource_dialogs/backup_settings_view_common.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_properties/server/widgets/backup_settings_picker_widget.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>
#include <ui/common/indents.h>
#include <ui/common/palette.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

static constexpr int kDropdownArrowSpacing = 4;

QString dropdownText(const QModelIndex& index)
{
    using namespace nx::vms::client::desktop;
    using namespace backup_settings_view;
    using namespace nx::vms::api;

    if (!index.isValid())
        return {};

    if (index.column() == ContentTypesColumn)
    {
        if (const auto data = index.data(BackupContentTypesRole); !data.isNull())
            return backupContentTypesToString(static_cast<BackupContentTypes>(data.toInt()));
    }

    if (index.column() == QualityColumn)
    {
        if (const auto data = index.data(BackupQualityRole); !data.isNull())
            return backupQualityToString(static_cast<CameraBackupQuality>(data.toInt()));
    }

    return {};
}

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

    if (isSeparator(index) || isSpacer(index) || !isDropdownColumn(index))
        return base_type::sizeHint(styleOption, index);

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

    const auto calcultateTextWidth =
        [this, styleOption, index](const QString& text)
        {
            const auto itr = m_dropdownTextWidthCache.constFind(text);
            if (itr != m_dropdownTextWidthCache.cend())
                return itr.value();

            QStyleOptionViewItem dropdownStyleOption = styleOption;
            initStyleOption(&dropdownStyleOption, index);

            const auto result = styleOption.fontMetrics.size(Qt::TextSingleLine, text).width();
            m_dropdownTextWidthCache.insert(text, result);
            return result;
        };

    const auto style = styleOption.widget ? styleOption.widget->style() : QApplication::style();

    const auto textWidth = std::max(
        calcultateTextWidth(dropdownText(index)),
        calcultateTextWidth(placeholderText));

    const auto columnWidth = textWidth
        + style->pixelMetric(QStyle::PM_MenuButtonIndicator)
        + Metrics::kStandardPadding * 2;

    return QSize(columnWidth, Metrics::kViewRowHeight);
}

void BackupSettingsItemDelegate::paintContents(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    base_type::paintContents(painter, styleOption, index);

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

    if (isDropdownColumn(index))
    {
        const auto textColor = m_dropdownColorTable.value(dropdownStateFlags(*option, index));
        option->font.setWeight(QFont::Normal);
        option->fontMetrics = QFontMetrics(option->font);
        option->palette.setColor(QPalette::Text, textColor);
        option->palette.setColor(QPalette::HighlightedText, textColor);
    }
}

void BackupSettingsItemDelegate::paintWarningIcon(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    const auto isWarningStyleData = index.data(IsItemWarningStyleRole);
    if (isWarningStyleData.isNull() || !isWarningStyleData.toBool())
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

    option.text = dropdownText(index);
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

    const auto isWarningStyleData = index.data(IsItemWarningStyleRole);
    dropdownState.setFlag(DropdownStateFlag::warning,
        !isWarningStyleData.isNull() && isWarningStyleData.toBool());

    return dropdownState;
}

void BackupSettingsItemDelegate::initDropdownColorTable()
{
    m_dropdownColorTable.insert({},
        core::colorTheme()->color("dark14"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::selected},
        core::colorTheme()->color("light13"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::hovered},
        core::colorTheme()->color("dark14"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::selected, DropdownStateFlag::hovered},
        core::colorTheme()->color("light13"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled},
        core::colorTheme()->color("light10"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::selected},
        core::colorTheme()->color("light7"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::hovered},
        core::colorTheme()->color("light6"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::selected, DropdownStateFlag::hovered},
        core::colorTheme()->color("light4"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::warning},
        core::colorTheme()->color("yellow_d2"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::selected, DropdownStateFlag::warning},
        core::colorTheme()->color("yellow_d1"));

    m_dropdownColorTable.insert(
        {DropdownStateFlag::enabled, DropdownStateFlag::hovered, DropdownStateFlag::warning},
        core::colorTheme()->color("yellow_core"));

    m_dropdownColorTable.insert({DropdownStateFlag::enabled, DropdownStateFlag::selected,
        DropdownStateFlag::hovered, DropdownStateFlag::warning},
            core::colorTheme()->color("yellow_core"));
}

} // namespace nx::vms::client::desktop
