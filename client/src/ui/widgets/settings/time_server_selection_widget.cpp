#include "time_server_selection_widget.h"
#include "ui_time_server_selection_widget.h"

QnTimeServerSelectionWidget::QnTimeServerSelectionWidget(QWidget *parent /*= NULL*/):
    QnAbstractPreferencesWidget(parent),
    ui(new Ui::TimeServerSelectionWidget)
{
    ui->setupUi(this);

}


QnTimeServerSelectionWidget::~QnTimeServerSelectionWidget()
{

}


void QnTimeServerSelectionWidget::updateFromSettings()
{

}

void QnTimeServerSelectionWidget::submitToSettings()
{

}

bool QnTimeServerSelectionWidget::hasChanges() const 
{
    return false;
}
