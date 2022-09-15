// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_input_business_event_widget.h"
#include "ui_camera_input_business_event_widget.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/event/events/camera_input_event.h>

using namespace nx::vms::client::desktop;

QnCameraInputBusinessEventWidget::QnCameraInputBusinessEventWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::CameraInputBusinessEventWidget)
{
    ui->setupUi(this);
    connect(ui->relayComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));

    /* This aligner will be linked with parent widget event controls aligner. */
    auto aligner = new Aligner(this);
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
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::eventResources))
    {
        QnIOPortDataList inputPorts;
        bool inited = false;

        auto cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(model()->eventResources());
        foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
            QnIOPortDataList cameraInputs = camera->ioPortDescriptions(Qn::PT_Input);
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
        ui->relayComboBox->addItem('<' + tr("automatic") + '>', QString());
        for (const auto& relayInput: inputPorts)
            ui->relayComboBox->addItem(relayInput.getName(), relayInput.id);
    }

    if (fields.testFlag(Field::eventResources) || fields.testFlag(Field::eventParams))
    {
        QString text = model()->eventParams().inputPortId;
        if (ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString() != text)
            ui->relayComboBox->setCurrentIndex(ui->relayComboBox->findData(text));

        if (ui->relayComboBox->currentIndex() == -1)
        {
            // Item not found. Switch to '<automatic>'.
            auto eventParams = model()->eventParams();
            eventParams.inputPortId.clear();
            model()->setEventParams(eventParams);
            ui->relayComboBox->setCurrentIndex(0);
        }
    }

}

void QnCameraInputBusinessEventWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    auto eventParams = model()->eventParams();
    eventParams.inputPortId = ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString();
    model()->setEventParams(eventParams);
}
