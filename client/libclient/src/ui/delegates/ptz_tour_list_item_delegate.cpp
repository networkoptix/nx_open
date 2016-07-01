#include "ptz_tour_list_item_delegate.h"

#include <common/common_globals.h>

QnPtzTourListItemDelegate::QnPtzTourListItemDelegate(QObject *parent) :
    QStyledItemDelegate(parent)
{
}

QnPtzTourListItemDelegate::~QnPtzTourListItemDelegate() {
}

void QnPtzTourListItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const  {
    base_type::initStyleOption(option, index);
    if (!index.data(Qn::ValidRole).toBool()) {
        QColor clr = index.data(Qt::BackgroundRole).value<QColor>();
        option->palette.setColor(QPalette::Highlight, clr.lighter());
    }
    //TODO: #GDM #PTZ invalid rows highlight
}
