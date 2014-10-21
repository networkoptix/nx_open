#include "camera_input_business_event_widget.h"
#include "ui_camera_input_business_event_widget.h"

#include <core/resource/camera_resource.h>

#include <business/events/camera_input_business_event.h>

#include <utils/common/scoped_value_rollback.h>

QnCameraInputBusinessEventWidget::QnCameraInputBusinessEventWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::CameraInputBusinessEventWidget)
{
    ui->setupUi(this);

    connect(ui->relayComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));
}

QnCameraInputBusinessEventWidget::~QnCameraInputBusinessEventWidget()
{
}

void QnCameraInputBusinessEventWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, ui->relayComboBox);
    setTabOrder(ui->relayComboBox, after);
}

void QnCameraInputBusinessEventWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields & QnBusiness::EventResourcesField) {
        QSet<QString> inputPorts;
        bool inited = false;

        QnVirtualCameraResourceList cameras = model->eventResources().filtered<QnVirtualCameraResource>();
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            QStringList cameraInputs = camera->getInputPortList();
            if (!inited) {
                inputPorts = cameraInputs.toSet();
                inited = true;
            } else {
                inputPorts = inputPorts.intersect(cameraInputs.toSet());
            }
        }

        ui->relayComboBox->clear();
        ui->relayComboBox->addItem(tr("<automatic>"), QString());
        foreach (QString relay, inputPorts)
            ui->relayComboBox->addItem(relay, relay);

    }

    if (fields & QnBusiness::EventParamsField) {
        QString text = model->eventParams().inputPortId;
        if (ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString() != text)
            ui->relayComboBox->setCurrentIndex(ui->relayComboBox->findData(text));
    }
}

void QnCameraInputBusinessEventWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessActionParameters params;
    params.inputPortId = ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString();
    model()->setActionParams(params);
}
