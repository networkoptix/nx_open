#include "camera_input_business_event_widget.h"
#include "ui_camera_input_business_event_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/event/events/camera_input_event.h>

#include <ui/common/aligner.h>
#include <utils/common/scoped_value_rollback.h>

QnCameraInputBusinessEventWidget::QnCameraInputBusinessEventWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::CameraInputBusinessEventWidget)
{
    ui->setupUi(this);
    connect(ui->relayComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));

    /* This aligner will be linked with parent widget event controls aligner. */
    auto aligner = new QnAligner(this);
    aligner->addWidget(ui->label);
}

QnCameraInputBusinessEventWidget::~QnCameraInputBusinessEventWidget()
{
}

void QnCameraInputBusinessEventWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, ui->relayComboBox);
    setTabOrder(ui->relayComboBox, after);
}

void QnCameraInputBusinessEventWidget::at_model_dataChanged(Fields fields) {
    if (!model())
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields.testFlag(Field::eventResources))
    {
        QnIOPortDataList inputPorts;
        bool inited = false;

        auto cameras = resourcePool()->getResources<QnVirtualCameraResource>(model()->eventResources());
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            QnIOPortDataList cameraInputs = camera->getInputPortList();
            if (!inited) {
                inputPorts = cameraInputs;
                inited = true;
            } else {
                for (auto itr = inputPorts.begin(); itr != inputPorts.end();)
                {
                    const QnIOPortData& value = *itr;
                    bool found = false;
                    for (const auto& other: cameraInputs) {
                        if (other.id == value.id && other.getName() == value.getName()) {
                            found = true;
                            break;
                        }
                    }
                    if (found)
                        ++itr;
                    else
                        itr = inputPorts.erase(itr);
                }
            }
        }

        ui->relayComboBox->clear();
        ui->relayComboBox->addItem(L'<' + tr("automatic") + L'>', QString());
        for (const auto& relayInput: inputPorts)
            ui->relayComboBox->addItem(relayInput.getName(), relayInput.id);
    }

    if (fields.testFlag(Field::eventParams))
    {
        QString text = model()->eventParams().inputPortId;
        if (ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString() != text)
            ui->relayComboBox->setCurrentIndex(ui->relayComboBox->findData(text));
    }
}

void QnCameraInputBusinessEventWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    auto eventParams = model()->eventParams();
    eventParams.inputPortId = ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString();
    model()->setEventParams(eventParams);
}
