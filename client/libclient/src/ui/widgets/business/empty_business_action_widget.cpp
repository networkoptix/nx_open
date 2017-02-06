#include "empty_business_action_widget.h"
#include "ui_empty_business_action_widget.h"

QnEmptyBusinessActionWidget::QnEmptyBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::EmptyBusinessActionWidget)
{
    ui->setupUi(this);
}

QnEmptyBusinessActionWidget::~QnEmptyBusinessActionWidget()
{
}
