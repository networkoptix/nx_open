#include "camera_output_business_action_widget.h"
#include "ui_camera_output_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <core/resource/camera_resource.h>

#include <utils/common/scoped_value_rollback.h>

QnCameraOutputBusinessActionWidget::QnCameraOutputBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnCameraOutputBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->autoResetCheckBox, SIGNAL(toggled(bool)), ui->autoResetSpinBox, SLOT(setEnabled(bool)));

    connect(ui->relayComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->autoResetCheckBox, SIGNAL(toggled(bool)), this, SLOT(paramsChanged()));
    connect(ui->autoResetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(paramsChanged()));

}

QnCameraOutputBusinessActionWidget::~QnCameraOutputBusinessActionWidget()
{
}

void QnCameraOutputBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    if (fields & QnBusiness::ActionTypeField) {
        bool instant = (model->actionType() == BusinessActionType::CameraOutputInstant);
        if (instant) {
            ui->autoResetCheckBox->setEnabled(false);
            ui->autoResetCheckBox->setChecked(true);
            ui->autoResetSpinBox->setValue(30);
            ui->autoResetSpinBox->setEnabled(false);
        }
    }

    if (fields & QnBusiness::ActionResourcesField) {
        QSet<QString> totalRelays;
        bool inited = false;

        QnVirtualCameraResourceList cameras = model->actionResources().filtered<QnVirtualCameraResource>();
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            QStringList cameraRelays = camera->getRelayOutputList();
            if (!inited) {
                totalRelays = cameraRelays.toSet();
                inited = true;
            } else {
                totalRelays = totalRelays.intersect(cameraRelays.toSet());
            }
        }

        ui->relayComboBox->clear();
        ui->relayComboBox->addItem(tr("<automatic>"), QString());
        foreach (QString relay, totalRelays)
            ui->relayComboBox->addItem(relay, relay);

    }

    if (fields & QnBusiness::ActionParamsField) {
        QnBusinessParams params = model->actionParams();

        QString text = QnBusinessActionParameters::getRelayOutputId(params);
        if (ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString() != text)
            ui->relayComboBox->setCurrentIndex(ui->relayComboBox->findData(text));

        bool instant = (model->actionType() == BusinessActionType::CameraOutputInstant);
        if (!instant) {
            int autoReset = QnBusinessActionParameters::getRelayAutoResetTimeout(params) / 1000;
            ui->autoResetCheckBox->setChecked(autoReset > 0);
            if (autoReset > 0)
                ui->autoResetSpinBox->setValue(autoReset);
        }
    }
}

void QnCameraOutputBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessParams params;
    QnBusinessActionParameters::setRelayOutputId(&params, ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString());
    QnBusinessActionParameters::setRelayAutoResetTimeout(&params, ui->autoResetCheckBox->isChecked()
                                                         ? ui->autoResetSpinBox->value() * 1000
                                                         : 0);
    model()->setActionParams(params);

}
