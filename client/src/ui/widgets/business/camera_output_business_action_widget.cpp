#include "camera_output_business_action_widget.h"
#include "ui_camera_output_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <core/resource/camera_resource.h>

#include <utils/common/scoped_value_rollback.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

QnCameraOutputBusinessActionWidget::QnCameraOutputBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::CameraOutputBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->autoResetCheckBox, SIGNAL(toggled(bool)), ui->autoResetSpinBox, SLOT(setEnabled(bool)));

    connect(ui->relayComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->autoResetCheckBox, SIGNAL(toggled(bool)), this, SLOT(paramsChanged()));
    connect(ui->autoResetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(paramsChanged()));

    setHelpTopic(this, Qn::EventsActions_CameraOutput_Help);
}

QnCameraOutputBusinessActionWidget::~QnCameraOutputBusinessActionWidget()
{
}

void QnCameraOutputBusinessActionWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, ui->relayComboBox);
    setTabOrder(ui->relayComboBox, ui->autoResetCheckBox);
    setTabOrder(ui->autoResetCheckBox, ui->autoResetSpinBox);
    setTabOrder(ui->autoResetSpinBox, after);
}

void QnCameraOutputBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields & QnBusiness::ActionTypeField) {
        bool instant = (model->actionType() == QnBusiness::CameraOutputOnceAction);
        if (instant) {
            ui->autoResetCheckBox->setEnabled(false);
            ui->autoResetCheckBox->setChecked(true);
            ui->autoResetSpinBox->setValue(30);
            ui->autoResetSpinBox->setEnabled(false);
        }
    }

    if (fields & QnBusiness::ActionResourcesField) {
        QnIOPortDataList outputPorts;
        bool inited = false;

        QnVirtualCameraResourceList cameras = model->actionResources().filtered<QnVirtualCameraResource>();
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            QnIOPortDataList cameraOutputs = camera->getRelayOutputList();
            if (!inited) {
                outputPorts = cameraOutputs;
                inited = true;
            } else {
                for (auto itr = outputPorts.begin(); itr != outputPorts.end();)
                {
                    const QnIOPortData& value = *itr;
                    bool found = false;
                    for (const auto& other: cameraOutputs) {
                        if (other.id == value.id && other.getName() == value.getName()) {
                            found = true;
                            break;
                        }
                    }
                    if (found)
                        ++itr;
                    else
                        itr = outputPorts.erase(itr);
                }
            }
        }

        ui->relayComboBox->clear();
        ui->relayComboBox->addItem(tr("<automatic>"), QString());
        for (const auto& relayOutput: outputPorts)
            ui->relayComboBox->addItem(relayOutput.getName(), relayOutput.id);
    }

    if (fields & QnBusiness::ActionParamsField) {
        QnBusinessActionParameters params = model->actionParams();

        QString text = params.relayOutputId;
        if (ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString() != text)
            ui->relayComboBox->setCurrentIndex(ui->relayComboBox->findData(text));

        bool instant = (model->actionType() == QnBusiness::CameraOutputOnceAction);
        if (!instant) {
            int autoReset = params.relayAutoResetTimeout / 1000;
            ui->autoResetCheckBox->setChecked(autoReset > 0);
            if (autoReset > 0)
                ui->autoResetSpinBox->setValue(autoReset);
        }
    }
}

void QnCameraOutputBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessActionParameters params = model()->actionParams();
    params.relayOutputId = ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString();
    params.relayAutoResetTimeout = ui->autoResetCheckBox->isChecked()
                                                         ? ui->autoResetSpinBox->value() * 1000
                                                         : 0;
    model()->setActionParams(params);

}
