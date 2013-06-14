#include "popup_business_action_widget.h"
#include "ui_popup_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/scoped_value_rollback.h>

QnPopupBusinessActionWidget::QnPopupBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnPopupBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->adminsCheckBox, SIGNAL(toggled(bool)), this, SLOT(paramsChanged()));
    connect(ui->settingsButton, SIGNAL(clicked()), this, SLOT(at_settingsButton_clicked()));
}

QnPopupBusinessActionWidget::~QnPopupBusinessActionWidget()
{
}

void QnPopupBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    if (fields & QnBusiness::ActionParamsField)
        ui->adminsCheckBox->setChecked(model->actionParams().getUserGroup() == QnBusinessActionParameters::AdminOnly);
}

void QnPopupBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessActionParameters params;
    params.setUserGroup(ui->adminsCheckBox->isChecked()
                                             ? QnBusinessActionParameters::AdminOnly
                                             : QnBusinessActionParameters::EveryOne);
    model()->setActionParams(params);
}

void QnPopupBusinessActionWidget::at_settingsButton_clicked() {
    menu()->trigger(Qn::PreferencesNotificationTabAction);
}
