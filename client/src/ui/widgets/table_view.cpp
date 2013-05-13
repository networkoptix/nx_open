#include "table_view.h"

QnTableView::QnTableView(QWidget *parent) :
    base_type(parent)
{
}


QnTableView::~QnTableView() {
    return;
}

bool QnTableView::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) {
    if (trigger == QAbstractItemView::SelectedClicked
            && this->editTriggers() & QAbstractItemView::DoubleClicked)
        return base_type::edit(index, QAbstractItemView::DoubleClicked, event);
    return base_type::edit(index, trigger, event);
}
