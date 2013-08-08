#include "camera_output_business_action_widget.h"
#include "ui_camera_output_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <core/resource/camera_resource.h>

#include <utils/common/scoped_value_rollback.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

QnCameraOutputBusinessActionWidget::QnCameraOutputBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnCameraOutputBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->autoResetCheckBox, SIGNAL(toggled(bool)), ui->autoResetSpinBox, SLOT(setEnabled(bool)));

    connect(ui->relayComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->autoResetCheckBox, SIGNAL(toggled(bool)), this, SLOT(paramsChanged()));
    connect(ui->autoResetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(paramsChanged()));

    setHelpTopic(this, Qn::EventsActions_InstantOutput_Help);
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
        QnBusinessActionParameters params = model->actionParams();

        QString text = params.getRelayOutputId();
        if (ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString() != text)
            ui->relayComboBox->setCurrentIndex(ui->relayComboBox->findData(text));

        bool instant = (model->actionType() == BusinessActionType::CameraOutputInstant);
        if (!instant) {
            int autoReset = params.getRelayAutoResetTimeout() / 1000;
            ui->autoResetCheckBox->setChecked(autoReset > 0);
            if (autoReset > 0)
                ui->autoResetSpinBox->setValue(autoReset);
        }
    }
}

void QnCameraOutputBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessActionParameters params;
    params.setRelayOutputId(ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString());
    params.setRelayAutoResetTimeout(ui->autoResetCheckBox->isChecked()
                                                         ? ui->autoResetSpinBox->value() * 1000
                                                         : 0);
    model()->setActionParams(params);

}
