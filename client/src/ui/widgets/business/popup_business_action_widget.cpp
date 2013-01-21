#include "popup_business_action_widget.h"
#include "ui_popup_business_action_widget.h"

#include <events/popup_business_action.h>

#include <ui/workbench/workbench_context.h>

#include <utils/common/scoped_value_rollback.h>

QnPopupBusinessActionWidget::QnPopupBusinessActionWidget(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::QnPopupBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->adminsCheckBox, SIGNAL(toggled(bool)), this, SLOT(paramsChanged()));
}

QnPopupBusinessActionWidget::~QnPopupBusinessActionWidget()
{
    delete ui;
}

void QnPopupBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    if (fields & QnBusiness::ActionParamsField)
        ui->adminsCheckBox->setChecked(BusinessActionParameters::getUserGroup(model->actionParams()) > 0);
}

void QnPopupBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessParams params;
    BusinessActionParameters::setUserGroup(&params, ui->adminsCheckBox->isChecked() ? 1 : 0);
    model()->setActionParams(params);
}
