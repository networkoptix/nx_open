#include "popup_collection_widget.h"
#include "ui_popup_collection_widget.h"

#include <ui/widgets/popup_widget.h>

QnPopupCollectionWidget::QnPopupCollectionWidget(QWidget *parent) :
    QWidget(parent, Qt::Tool | Qt::FramelessWindowHint /* Qt::Popup */),
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
