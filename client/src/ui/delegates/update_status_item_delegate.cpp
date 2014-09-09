#include "update_status_item_delegate.h"

#include <QtWidgets/QStyleOptionProgressBar>
#include <QtWidgets/QApplication>

#include <ui/models/server_updates_model.h>
#include <ui/style/noptix_style.h>

namespace {
    const int defaultWidth = 200;
}

QnUpdateStatusItemDelegate::QnUpdateStatusItemDelegate(QWidget *parent) :
    QStyledItemDelegate(parent)
{}

QnUpdateStatusItemDelegate::~QnUpdateStatusItemDelegate() {}

void QnUpdateStatusItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    int state = index.data(QnServerUpdatesModel::StageRole).toInt();

    switch (state) {
    case QnPeerUpdateStage::Download:
    case QnPeerUpdateStage::Push:
    case QnPeerUpdateStage::Install:
        paintProgressBar(painter, option, index);
        break;
    default:
        base_type::paint(painter, option, index);
        break;
    }
}


void QnUpdateStatusItemDelegate::paintProgressBar(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QnStyleOptionProgressBar progressBarStyleOption;
    progressBarStyleOption.rect = option.rect.adjusted(4, 4, -4, -4);

    const int progress = index.data(QnServerUpdatesModel::ProgressRole).toInt();

    progressBarStyleOption.minimum = 0;
    progressBarStyleOption.maximum = 100;
    progressBarStyleOption.textAlignment = Qt::AlignCenter;
    progressBarStyleOption.progress = progress ;
    progressBarStyleOption.text = tr("%1%").arg(progress);
    progressBarStyleOption.textVisible = true;

    QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarStyleOption, painter, option.widget);
}


QSize QnUpdateStatusItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const  {
    QSize result  = base_type::sizeHint(option, index);
    result.setWidth(qMax(result.width(), defaultWidth));
    return result;
}



