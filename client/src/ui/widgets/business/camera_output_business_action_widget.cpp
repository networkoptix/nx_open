#include "camera_output_business_action_widget.h"
#include "ui_camera_output_business_action_widget.h"

#include <core/resource/camera_resource.h>

#include <events/camera_output_business_action.h>

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
    delete ui;
}

void QnCameraOutputBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    if (fields & QnBusiness::ActionResourcesField) {
        QSet<QString> total_relays;
        bool inited = false;

        QnVirtualCameraResourceList cameras = model->actionResources().filtered<QnVirtualCameraResource>();
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            QStringList camera_relays = camera->getRelayOutputList();
            if (!inited) {
                total_relays = camera_relays.toSet();
                inited = true;
            } else {
                total_relays = total_relays.intersect(camera_relays.toSet());
            }
        }

        ui->relayComboBox->clear();
        ui->relayComboBox->addItem(tr("<automatic>"), QString());
        foreach (QString relay, total_relays)
            ui->relayComboBox->addItem(relay, relay);

    }

    if (fields & QnBusiness::ActionParamsField) {
        QnBusinessParams params = model->actionParams();

        QString text = BusinessActionParameters::getRelayOutputId(params);
        if (ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString() != text)
            ui->relayComboBox->setCurrentIndex(ui->relayComboBox->findData(text));
        int autoReset = BusinessActionParameters::getRelayAutoResetTimeout(params) / 1000;
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
    BusinessActionParameters::setRelayOutputId(&params, ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString());
    BusinessActionParameters::setRelayAutoResetTimeout(&params, ui->autoResetCheckBox->isChecked()
                                                       ? ui->autoResetSpinBox->value() * 1000
                                                       : 0);
    model()->setActionParams(params);

}
