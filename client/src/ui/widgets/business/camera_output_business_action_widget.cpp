#include "camera_output_business_action_widget.h"
#include "ui_camera_output_business_action_widget.h"

#include <events/camera_output_business_action.h>

QnCameraOutputBusinessActioWidget::QnCameraOutputBusinessActioWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnCameraOutputBusinessActioWidget)
{
    ui->setupUi(this);
    connect(ui->autoResetCheckBox, SIGNAL(toggled(bool)), ui->autoResetSpinBox, SLOT(setEnabled(bool)));

}

QnCameraOutputBusinessActioWidget::~QnCameraOutputBusinessActioWidget()
{
    delete ui;
}

void QnCameraOutputBusinessActioWidget::loadParameters(const QnBusinessParams &params) {
    ui->relayIdLineEdit->setText(BusinessActionParameters::getRelayOutputId(params));
    int autoReset = BusinessActionParameters::getRelayAutoResetTimeout(params);
    ui->autoResetCheckBox->setChecked(autoReset > 0);
    if (autoReset > 0)
        ui->autoResetSpinBox->setValue(autoReset);
}

QnBusinessParams QnCameraOutputBusinessActioWidget::parameters() const {
    QnBusinessParams params;
    BusinessActionParameters::setRelayOutputId(&params, ui->relayIdLineEdit->text());
    BusinessActionParameters::setRelayAutoResetTimeout(&params, ui->autoResetCheckBox->isChecked()
                                                       ? ui->autoResetSpinBox->value()
                                                       : 0);
    return params;
}

QString QnCameraOutputBusinessActioWidget::description() const {
    //TODO: #GDM remove me
    return QString();
}
