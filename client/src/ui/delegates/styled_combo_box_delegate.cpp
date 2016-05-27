#include "styled_combo_box_delegate.h"

#include <ui/style/helper.h>
#include <utils/common/scoped_painter_rollback.h>

QnStyledComboBoxDelegate::QnStyledComboBoxDelegate(QObject* parent) : base_type(parent)
{
}

void QnStyledComboBoxDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (isSeparator(index))
    {
        int y = option.rect.top() + option.rect.height() / 2;
        QnScopedPainterPenRollback penRollback(painter, option.palette.color(QPalette::Midlight));
        painter->drawLine(0, y, option.rect.right(), y);
    }
    else
    {
        base_type::paint(painter, option, index);
    }
}

QSize QnStyledComboBoxDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (isSeparator(index))
        return style::Metrics::kSeparatorSize;

    return base_type::sizeHint(option, index);
}

bool QnStyledComboBoxDelegate::isSeparator(const QModelIndex& index)
{
    return index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1Literal("separator");
}
