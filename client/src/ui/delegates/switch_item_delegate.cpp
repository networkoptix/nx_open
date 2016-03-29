#include "switch_item_delegate.h"

#include <ui/style/helper.h>
#include <ui/style/nx_style.h>

QnSwitchItemDelegate::QnSwitchItemDelegate(QObject *parent)
    : base_type(parent)
{
}

void QnSwitchItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (QnNxStyle *nxStyle = QnNxStyle::instance())
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        nxStyle->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
        nxStyle->drawSwitch(painter, &opt, opt.widget);
    }
    else
    {
        base_type::paint(painter, option, index);
    }
}

QSize QnSwitchItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)

    return style::Metrics::kSwitchSize;
}

void QnSwitchItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    base_type::initStyleOption(option, index);

    if (!QnNxStyle::instance())
        return;

    if (option->checkState == Qt::Checked)
        option->state |= QStyle::State_On;
    else
        option->state |= QStyle::State_Off;
}
