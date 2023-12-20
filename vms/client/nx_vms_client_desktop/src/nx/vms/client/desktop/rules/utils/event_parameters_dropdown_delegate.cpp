// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameters_dropdown_delegate.h"

#include <QtWidgets/QApplication>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <utils/common/scoped_painter_rollback.h>

bool isSeparator(const QModelIndex& index)
{
    return index.data(Qt::AccessibleDescriptionRole).toString() == "separator";
}

namespace nx::vms::client::desktop {

EventParametersDropDownDelegate::EventParametersDropDownDelegate(QObject* parent):
    base_type(parent)
{
}

void EventParametersDropDownDelegate::paint(
    QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem itemOption(option);
    base_type::initStyleOption(&itemOption, index);

    if (isSeparator(index))
    {
        // Draw separator line.
        int y = itemOption.rect.top() + itemOption.rect.height() / 2;
        QnScopedPainterPenRollback penRollback(painter, itemOption.palette.button().color());
        painter->drawLine(style::Metrics::kMenuItemHPadding,
            y,
            itemOption.rect.right() - style::Metrics::kMenuItemHPadding,
            y);
    }
    else
    {
        QStyle* style = itemOption.widget ? itemOption.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter, itemOption.widget);
    }
}

QString EventParametersDropDownDelegate::displayText(
    const QVariant& value, const QLocale& locale) const
{
    return value.toString()
        .replace(rules::TextWithFields::kStartOfSubstitutionSymbol, "")
        .replace(rules::TextWithFields::kEndOfSubstitutionSymbol, "");
}

} // namespace nx::vms::client::desktop
