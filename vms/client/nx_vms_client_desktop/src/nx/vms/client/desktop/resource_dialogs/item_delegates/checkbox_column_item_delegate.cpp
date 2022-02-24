// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "checkbox_column_item_delegate.h"

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

CheckBoxColumnItemDelegate::CheckBoxColumnItemDelegate(QObject* parent):
    base_type(parent)
{
}

CheckBoxColumnItemDelegate::CheckMarkType
    CheckBoxColumnItemDelegate::checkMarkType() const
{
    return m_checkMarkType;
}

void CheckBoxColumnItemDelegate::setCheckMarkType(CheckMarkType type)
{
    m_checkMarkType = type;
}

void CheckBoxColumnItemDelegate::paintContents(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    static const auto kUncheckedCheckboxColor = colorTheme()->color("dark10");
    static const auto kUncheckedHoveredCheckboxColor = colorTheme()->color("dark16");
    static const auto kCheckedCheckboxColor = colorTheme()->color("resourceTree.mainTextSelected");

    const auto checkStateData = index.data(Qt::CheckStateRole);
    if (checkStateData.isNull())
        return;

    QStyleOptionViewItem checkMarkOption(styleOption);
    if (styleOption.state.testFlag(QStyle::State_Enabled))
    {
        if (checkStateData.value<Qt::CheckState>() != Qt::Unchecked)
            checkMarkOption.palette.setColor(QPalette::Text, kCheckedCheckboxColor);
        else if (styleOption.state.testFlag(QStyle::State_MouseOver))
            checkMarkOption.palette.setColor(QPalette::Text, kUncheckedHoveredCheckboxColor);
        else
            checkMarkOption.palette.setColor(QPalette::Text, kUncheckedCheckboxColor);
    }

    checkMarkOption.displayAlignment = Qt::AlignHCenter | Qt::AlignVCenter;

    switch (checkStateData.value<Qt::CheckState>())
    {
        case Qt::Checked:
            checkMarkOption.state.setFlag(QStyle::State_On, true);
            break;

        case Qt::PartiallyChecked:
            checkMarkOption.state.setFlag(QStyle::State_NoChange, true);
            break;

        case Qt::Unchecked:
            checkMarkOption.state.setFlag(QStyle::State_Off, true);
            break;
    }

    const auto widget = styleOption.widget;
    const auto style = widget ? widget->style() : QApplication::style();
    switch (checkMarkType())
    {
        case CheckBox:
            checkMarkOption.rect =
                style->subElementRect(QStyle::SE_RadioButtonIndicator, &checkMarkOption, widget);
            style->drawPrimitive(
                QStyle::PE_IndicatorItemViewItemCheck, &checkMarkOption, painter, widget);
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
