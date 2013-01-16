#include "popup_business_action_widget.h"
#include "ui_popup_business_action_widget.h"

#include <events/popup_business_action.h>

#include <ui/workbench/workbench_context.h>

QnPopupBusinessActionWidget::QnPopupBusinessActionWidget(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::QnPopupBusinessActionWidget)
{
    ui->setupUi(this);
}

QnPopupBusinessActionWidget::~QnPopupBusinessActionWidget()
{
    delete ui;
}

void QnPopupBusinessActionWidget::loadParameters(const QnBusinessParams &params) {
    ui->adminsCheckBox->setChecked(BusinessActionParameters::getUserGroup(params) > 0);
}

QnBusinessParams QnPopupBusinessActionWidget::parameters() const {
    QnBusinessParams params;
    BusinessActionParameters::setUserGroup(&params, ui->adminsCheckBox->isChecked() ? 1 : 0);
    return params;
}

QString QnPopupBusinessActionWidget::description() const {
    return QString();
}
