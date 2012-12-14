#include "empty_business_event_widget.h"
#include "ui_empty_business_event_widget.h"

QnEmptyBusinessEventWidget::QnEmptyBusinessEventWidget(QWidget *parent) :
    QnAbstractBusinessEventWidget(parent),
    ui(new Ui::QnEmptyBusinessEventWidget)
{
    ui->setupUi(this);
}

QnEmptyBusinessEventWidget::~QnEmptyBusinessEventWidget()
{
    delete ui;
}
