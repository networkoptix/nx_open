#include "popup_widget.h"
#include "ui_popup_widget.h"

QnPopupWidget::QnPopupWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPopupWidget)
{
    ui->setupUi(this);
    int r = qrand();
    if (r % 2)
        ui->label_2->setVisible(false);
    else
        ui->label_3->setVisible(false);
}

QnPopupWidget::~QnPopupWidget()
{
    delete ui;
}
