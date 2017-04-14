#include "styled_combo_box_delegate.h"

#include <QtWidgets/QApplication>

#include <ui/style/helper.h>
#include <utils/common/scoped_painter_rollback.h>

QnStyledComboBoxDelegate::QnStyledComboBoxDelegate(QObject* parent) : base_type(parent)
{
}

void QnStyledComboBoxDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    /* Init style option: */
    QStyleOptionViewItem itemOption(option);
    initStyleOption(&itemOption, index);

    /* Fill background: */
    painter->fillRect(itemOption.rect, itemOption.palette.midlight());

    if (isSeparator(index))
    {
        /* Draw separator line: */
        int y = itemOption.rect.top() + itemOption.rect.height() / 2;
        QnScopedPainterPenRollback penRollback(painter, itemOption.palette.button().color());
        painter->drawLine(style::Metrics::kMenuItemHPadding, y, itemOption.rect.right() - style::Metrics::kMenuItemHPadding, y);
    }
    else
    {
        /* Standard styled painting: */
        QStyle* style = itemOption.widget ? itemOption.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter, itemOption.widget);
    }
}

QSize QnStyledComboBoxDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (isSeparator(index))
        return style::Metrics::kSeparatorSize + QSize(style::Metrics::kMenuItemHPadding * 2, 0);

    return base_type::sizeHint(option, index);
}

bool QnStyledComboBoxDelegate::isSeparator(const QModelIndex& index)
{
    return index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1Literal("separator");
}

void QnStyledComboBoxDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);
    option->state &= ~QStyle::State_HasFocus;

    auto style = option->widget ? option->widget->style() : QApplication::style();
    bool noHover = style->styleHint(QStyle::SH_ComboBox_ListMouseTracking, option, option->widget)
                || style->styleHint(QStyle::SH_ComboBox_Popup, option, option->widget);
    if (noHover)
        option->state &= ~QStyle::State_MouseOver;

    if (option->widget && option->widget->parentWidget())
        option->palette = option->widget->parentWidget()->palette();
}
