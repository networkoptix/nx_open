// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_selection_dialog_item_delegate.h"

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/indents.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/utils/log/assert.h>

namespace {

static const int kDecoratorWidth = nx::style::Metrics::kViewRowHeight;

} // namespace

namespace nx::vms::client::desktop {

ResourceSelectionDialogItemDelegate::ResourceSelectionDialogItemDelegate(QObject* parent):
    base_type(parent)
{
}

ResourceSelectionDialogItemDelegate::CheckMarkType
    ResourceSelectionDialogItemDelegate::checkMarkType() const
{
    return m_checkMarkType;
}

void ResourceSelectionDialogItemDelegate::setCheckMarkType(CheckMarkType type)
{
    m_checkMarkType = type;
}

void ResourceSelectionDialogItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    if (isSeparator(index))
    {
        base_type::paint(painter, styleOption, index);
        return;
    }

    const bool checked = index.data(Qt::CheckStateRole).value<Qt::CheckState>() != Qt::Unchecked;

    const bool highlighted = checked;
    const auto extraColor = highlighted
        ? colorTheme()->color("resourceTree.extraTextSelected")
        : colorTheme()->color("resourceTree.extraText");
    const auto mainColor = highlighted
        ? colorTheme()->color("resourceTree.mainTextSelected")
        : colorTheme()->color("resourceTree.mainText");

    auto option = styleOption;
    initStyleOption(&option, index);
    paintItemText(painter, option, index, mainColor, extraColor, colorTheme()->color("red_l2"));
    paintItemIcon(painter, option, index, highlighted ? QIcon::Selected : QIcon::Normal);
    paintItemCheckMark(painter, option, index, mainColor);
}

void ResourceSelectionDialogItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    // Bypass default checkbox painting. It will be painted at the right side of item instead.
    option->features.setFlag(QStyleOptionViewItem::HasCheckIndicator, false);
}

int ResourceSelectionDialogItemDelegate::decoratorWidth(const QModelIndex& index) const
{
    return kDecoratorWidth;
}

void ResourceSelectionDialogItemDelegate::paintItemCheckMark(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index, const
    QColor& checkedColor) const
{
    const auto checkStateData = index.data(Qt::CheckStateRole);
    if (checkStateData.isNull())
        return;

    QStyleOptionViewItem checkMarkOption(option);
    checkMarkOption.rect.setLeft(checkMarkOption.rect.right() - checkMarkOption.rect.height());

    QnScopedPainterOpacityRollback opacityRollback(painter);
    if (!option.state.testFlag(QStyle::State_Enabled))
        painter->setOpacity(painter->opacity() * style::Hints::kDisabledItemOpacity);

    const auto widget = option.widget;
    if (widget)
    {
        auto indentsProperty = widget->property(style::Properties::kSideIndentation);
        if (!indentsProperty.isNull())
        {
            const auto rightIndent = indentsProperty.value<QnIndents>().right();
            checkMarkOption.rect.adjust(-rightIndent, 0, -rightIndent, 0);
        }
    }

    const auto checkState = checkStateData.value<Qt::CheckState>();
    switch (checkState)
    {
        case Qt::Checked:
            checkMarkOption.state.setFlag(QStyle::State_On, true);
            checkMarkOption.palette.setColor(QPalette::Text, checkedColor);
            break;

        case Qt::PartiallyChecked:
            checkMarkOption.state.setFlag(QStyle::State_NoChange, true);
            break;

        case Qt::Unchecked:
            checkMarkOption.state.setFlag(QStyle::State_Off, true);
            checkMarkOption.palette.setCurrentColorGroup(QPalette::Disabled);
            break;
    }

    const auto style = widget ? widget->style() : QApplication::style();
    switch (checkMarkType())
    {
        case CheckBox:
            style->drawPrimitive(
                QStyle::PE_IndicatorViewItemCheck, &checkMarkOption, painter, widget);
            break;

        case RadioButton:
            checkMarkOption.rect =
                style->subElementRect(QStyle::SE_RadioButtonIndicator, &checkMarkOption, widget);
            style->drawPrimitive(
                QStyle::PE_IndicatorRadioButton, &checkMarkOption, painter, widget);
            break;

        default:
            NX_ASSERT(false);
            break;
    }
}

} // namespace nx::vms::client::desktop
