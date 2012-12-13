#include "empty_business_action_widget.h"
#include "ui_empty_business_action_widget.h"

QnEmptyBusinessActionWidget::QnEmptyBusinessActionWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnEmptyBusinessActionWidget)
{
    ui->setupUi(this);
}

QnEmptyBusinessActionWidget::~QnEmptyBusinessActionWidget()
{
    delete ui;
}
