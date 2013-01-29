#include "popup_business_action_widget.h"
#include "ui_popup_business_action_widget.h"

#include <business/actions/popup_business_action.h>

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/scoped_value_rollback.h>

QnPopupBusinessActionWidget::QnPopupBusinessActionWidget(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::QnPopupBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->adminsCheckBox, SIGNAL(toggled(bool)), this, SLOT(paramsChanged()));
    connect(ui->settingsButton, SIGNAL(clicked()), this, SLOT(at_settingsButton_clicked()));
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

void QnPopupBusinessActionWidget::at_settingsButton_clicked() {
    menu()->trigger(Qn::OpenPopupSettingsAction);
}
