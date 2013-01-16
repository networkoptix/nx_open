#include "popup_widget.h"
#include "ui_popup_widget.h"

QnPopupWidget::QnPopupWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPopupWidget)
{
    ui->setupUi(this);
    int r = qrand();
    ui->label->setText(ui->label->text() + QString::number(r));
}

QnPopupWidget::~QnPopupWidget()
{
    delete ui;
}
