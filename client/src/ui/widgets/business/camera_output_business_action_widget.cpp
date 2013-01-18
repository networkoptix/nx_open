#include "camera_output_business_action_widget.h"
#include "ui_camera_output_business_action_widget.h"

#include <events/camera_output_business_action.h>

#include <utils/common/scoped_value_rollback.h>

QnCameraOutputBusinessActionWidget::QnCameraOutputBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnCameraOutputBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->autoResetCheckBox, SIGNAL(toggled(bool)), ui->autoResetSpinBox, SLOT(setEnabled(bool)));

    connect(ui->relayIdLineEdit, SIGNAL(editingFinished()), this, SLOT(paramsChanged()));
    connect(ui->autoResetCheckBox, SIGNAL(toggled(bool)), this, SLOT(paramsChanged()));
    connect(ui->autoResetSpinBox, SIGNAL(editingFinished()), this, SLOT(paramsChanged()));

}

QnCameraOutputBusinessActionWidget::~QnCameraOutputBusinessActionWidget()
{
    delete ui;
}

void QnCameraOutputBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    if (fields & QnBusiness::ActionParamsField) {
        QnBusinessParams params = model->actionParams();

        ui->relayIdLineEdit->setText(BusinessActionParameters::getRelayOutputId(params));
        int autoReset = BusinessActionParameters::getRelayAutoResetTimeout(params);
        ui->autoResetCheckBox->setChecked(autoReset > 0);
        if (autoReset > 0)
            ui->autoResetSpinBox->setValue(autoReset);
    }

    //TODO: #GDM update on resource change
}

void QnCameraOutputBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessParams params;
    BusinessActionParameters::setRelayOutputId(&params, ui->relayIdLineEdit->text());
    BusinessActionParameters::setRelayAutoResetTimeout(&params, ui->autoResetCheckBox->isChecked()
                                                       ? ui->autoResetSpinBox->value()
                                                       : 0);
    model()->setActionParams(params);

}
