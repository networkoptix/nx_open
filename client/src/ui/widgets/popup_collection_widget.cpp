#include "popup_collection_widget.h"
#include "ui_popup_collection_widget.h"

#include <ui/widgets/popup_widget.h>

QnPopupCollectionWidget::QnPopupCollectionWidget(QWidget *parent) :
    QWidget(parent, Qt::Popup /*| Qt::FramelessWindowHint*/ /* Qt::Popup */),
    ui(new Ui::QnPopupCollectionWidget)
{
    ui->setupUi(this);
    setGeometry(0, 0, 1, 1);
}

QnPopupCollectionWidget::~QnPopupCollectionWidget()
{
    delete ui;
}

void QnPopupCollectionWidget::add() {
    QnPopupWidget* w = new QnPopupWidget(this);
    ui->verticalLayout->addWidget(w);

}

void QnPopupCollectionWidget::showEvent(QShowEvent *event) {
    QRect pgeom = static_cast<QWidget *>(parent())->geometry();
    QRect geom = geometry();

    qDebug() << pgeom << geom;
    setGeometry(pgeom.width() - geom.width(), pgeom.height() - geom.height(), geom.width(), geom.height());
    base_type::showEvent(event);
}
