// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_dialog_item_delegate.h"

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_views/data/resource_extra_status.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <ui/common/indents.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

using NodeType = nx::vms::client::desktop::ResourceTree::NodeType;

} // namespace

namespace nx::vms::client::desktop {

ResourceDialogItemDelegate::ResourceDialogItemDelegate(QObject* parent):
    base_type(parent)
{
}

bool ResourceDialogItemDelegate::showRecordingIndicator() const
{
    return m_showRecordingIndicator;
}

void ResourceDialogItemDelegate::setShowRecordingIndicator(bool show)
{
    m_showRecordingIndicator = show;
}

QSize ResourceDialogItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    if (isSpacer(index))
        return QSize(0, nx::style::Metrics::kViewSpacerRowHeight);

    if (isSeparator(index))
        return QSize(0, nx::style::Metrics::kViewSeparatorRowHeight);

    return base_type::sizeHint(option, index);
}

void ResourceDialogItemDelegate::paintContents(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    QStyleOptionViewItem option(styleOption);
    initStyleOption(&option, index);

    if (index.column() == 0)
    {
        paintItemText(painter, option, index);

        const auto isHighlightedData = index.data(ResourceDialogItemRole::IsItemHighlightedRole);
        const auto isValidResourceData = index.data(ResourceDialogItemRole::IsValidResourceRole);

        const auto isHighlighted = !isHighlightedData.isNull() && isHighlightedData.toBool();
        const auto isValidResource = isValidResourceData.isNull() || isValidResourceData.toBool();

        const auto iconMode = isHighlighted && isValidResource ?  QIcon::Selected : QIcon::Normal;
        paintItemIcon(painter, option, index, iconMode);
    }
    else
    {
        base_type::paintContents(painter, styleOption, index);
    }
}

void ResourceDialogItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{

    static const auto kTextColor = core::colorTheme()->color("light10");
    static const auto kSelectedTextColor = core::colorTheme()->color("light7");

    static const auto kExtraTextColor = core::colorTheme()->color("dark13");
    static const auto kSelectedExtraTextColor = core::colorTheme()->color("light17");

    base_type::initStyleOption(option, index);

    option->palette.setColor(QPalette::Text, kTextColor);
    option->palette.setColor(QPalette::HighlightedText, kSelectedTextColor);

    if (index.column() == 0)
    {
        option->font = fontConfig()->font("resourceTree");
        option->fontMetrics = QFontMetrics(option->font);

        const auto itemIsSelected = option->state.testFlag(QStyle::State_Selected);
        option->palette.setColor(QPalette::PlaceholderText,
            itemIsSelected || index.data(Qn::ForceExtraInfoRole).toBool() ? kSelectedExtraTextColor
                                                                          : kExtraTextColor);
    }

    static const auto kInvalidTextColor = core::colorTheme()->color("red_core");
    static const auto kCheckedItemTextColor =
        core::colorTheme()->color("resourceTree.mainTextSelected");
    static const auto kCheckedItemExtraTextColor =
        core::colorTheme()->color("resourceTree.extraTextSelected");

    const auto isHighlighted = index.data(ResourceDialogItemRole::IsItemHighlightedRole).toBool();
    if (isHighlighted)
    {
        option->palette.setColor(QPalette::Text, kCheckedItemTextColor);
        option->palette.setColor(QPalette::HighlightedText, kCheckedItemTextColor);
        option->palette.setColor(QPalette::BrightText, kCheckedItemTextColor);
        option->palette.setColor(QPalette::PlaceholderText, kCheckedItemExtraTextColor);
    }

    const auto isValidData = index.data(ResourceDialogItemRole::IsValidResourceRole);
    const auto isValid = isValidData.isNull() || isValidData.toBool();
    if (index.column() == resource_selection_view::ResourceColumn && !isValid)
    {
        option->palette.setColor(QPalette::Text, kInvalidTextColor);
        option->palette.setColor(QPalette::HighlightedText, kInvalidTextColor);
    }
}

void ResourceDialogItemDelegate::paintItemText(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    const auto style = option.widget ? option.widget->style() : QApplication::style();

    const auto textColor = option.state.testFlag(QStyle::State_Selected)
        ? option.palette.color(QPalette::HighlightedText)
        : option.palette.color(QPalette::Text);
    const auto extraTextColor = option.palette.color(QPalette::PlaceholderText);

    const auto textFont = option.font;
    auto extraTextFont = option.font;
    extraTextFont.setWeight(QFont::Normal);

    static constexpr int kExtraTextSpacing = 6;
    const int kTextFlags = option.displayAlignment | Qt::TextSingleLine;

    const int textPadding = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget)
        .adjusted(textPadding, 0, -textPadding, 0);

    const auto showExtraInfo = index.data(Qn::ForceExtraInfoRole).toBool()
        || appContext()->localSettings()->resourceInfoLevel() == Qn::RI_FullInfo;

    // Draw background.
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Draw semi transparent if item is disabled.
    QnScopedPainterOpacityRollback opacityRollback(painter);
    if (!option.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);

    if (!option.features.testFlag(QStyleOptionViewItem::HasDisplay))
        return; //< What about icon?

    // Draw text.
    QRect textBoundingRect;
    if (auto text = option.text; !text.isEmpty())
    {
        QnScopedPainterPenRollback penRollback(painter, textColor);
        QnScopedPainterFontRollback fontRollback(painter, textFont);

        text = option.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width());
        painter->drawText(textRect, kTextFlags, text, &textBoundingRect);
    }

    textRect.setLeft(textBoundingRect.right() + kExtraTextSpacing);
    if (!textRect.isValid())
        return;

    if (!option.displayAlignment.testFlag(Qt::AlignLeft))
        return;

    // Draw extra text.
    if (auto extraText = showExtraInfo ? index.data(Qn::ExtraInfoRole).toString() : QString();
        !extraText.isEmpty())
    {
        QnScopedPainterPenRollback penRollback(painter, extraTextColor);
        QnScopedPainterFontRollback fontRollback(painter, extraTextFont);

        const auto fontMetrics = QFontMetrics(extraTextFont);
        extraText = fontMetrics.elidedText(extraText, Qt::ElideRight, textRect.width());
        painter->drawText(textRect, kTextFlags, extraText);
    }
}

void ResourceDialogItemDelegate::paintItemIcon(
    QPainter* painter,
    const QStyleOptionViewItem& optionk,
    const QModelIndex& index,
    QIcon::Mode mode) const
{
    QStyleOptionViewItem option = optionk;

    if (!option.features.testFlag(QStyleOptionViewItem::HasDecoration))
        return;

    QnScopedPainterOpacityRollback opacityRollback(painter);
    if (!option.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);

    const auto style = option.widget ? option.widget->style() : QApplication::style();
    const QRect iconRect = style->subElementRect(
        QStyle::SE_ItemViewItemDecoration, &option, option.widget);

    const auto isValidResourceData = index.data(ResourceDialogItemRole::IsValidResourceRole);
    if (!isValidResourceData.isNull() && !isValidResourceData.toBool() && !option.icon.isNull())
    {
        const auto invalidIconColor = core::colorTheme()->color("red_core");
        const auto iconSize = option.icon.actualSize(iconRect.size());
        const auto colorizedIcon = QIcon(
            core::Skin::colorize(option.icon.pixmap(iconSize), invalidIconColor));
        option.icon = colorizedIcon;
    }

    option.icon.paint(painter, iconRect, option.decorationAlignment, mode, QIcon::On);

    const auto isWarningStyle = index.data(IsItemWarningStyleRole).toBool();
    if (isWarningStyle)
    {
        const auto warningIcon = qnSkin->icon("tree/buggy.svg");
        warningIcon.paint(painter, iconRect.translated(-style::Metrics::kDefaultIconSize, 0));
    }
    else if (showRecordingIndicator())
    {
        paintRecordingIndicator(painter, iconRect, index);
    }
}

void ResourceDialogItemDelegate::paintRecordingIndicator(
    QPainter* painter,
    const QRect& iconRect,
    const QModelIndex& index) const
{
    NX_ASSERT(index.model());
    if (!index.model())
        return;

    const auto extraStatus = index.data(Qn::ResourceExtraStatusRole).value<ResourceExtraStatus>();

    if (extraStatus == ResourceExtraStatus())
        return;

    const auto nodeHasChildren =
        [](const QModelIndex& index){ return index.model()->rowCount(index) > 0; };

    QRect indicatorRect(iconRect);

    // Check if there are too much icons for this indentation level.
    const auto shiftIconLeft =
        [&indicatorRect]
        {
            indicatorRect.moveLeft(indicatorRect.left() - style::Metrics::kDefaultIconSize);
            return indicatorRect.left() >= 0;
        };

    // Leave space for expand/collapse control if needed.
    if (nodeHasChildren(index))
        shiftIconLeft();

    // Draw recording status icon.
    const auto recordingIcon = QnResourceIconCache::cameraRecordingStatusIcon(extraStatus);
    if (!recordingIcon.isNull() && shiftIconLeft())
        recordingIcon.paint(painter, indicatorRect);
}

} // namespace nx::vms::client::desktop
