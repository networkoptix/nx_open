#include "popup_widget.h"
#include "ui_popup_widget.h"

QnPopupWidget::QnPopupWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPopupWidget)
{
    ui->setupUi(this);
}

QnPopupWidget::~QnPopupWidget()
{
    delete ui;
}
