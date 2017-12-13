#include "camera_output_business_action_widget.h"
#include "ui_camera_output_business_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <nx/vms/event/action_parameters.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/scoped_value_rollback.h>

QnCameraOutputBusinessActionWidget::QnCameraOutputBusinessActionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::CameraOutputBusinessActionWidget)
{
    ui->setupUi(this);

    ui->timeDuration->addDurationSuffix(QnTimeStrings::Suffix::Minutes);
    ui->timeDuration->addDurationSuffix(QnTimeStrings::Suffix::Hours);

    connect(ui->fixedDurationCheckBox, &QCheckBox::toggled, this,
        [this](bool checked)
        {
            ui->timeDuration->setEnabled(checked);

            // Prolonged type of event has changed. In case of instant
            // action event state should be updated
            if (checked && (model()->eventType() == nx::vms::event::userDefinedEvent))
                model()->setEventState(nx::vms::event::EventState::undefined);

            emit paramsChanged();
        });

    ui->fixedDurationCheckBox->setCheckable(true);

    connect(ui->relayComboBox, QnComboboxCurrentIndexChanged, this,
        &QnCameraOutputBusinessActionWidget::paramsChanged);

    connect(ui->timeDuration, SIGNAL(valueChanged()), this, SLOT(paramsChanged()));

    setHelpTopic(this, Qn::EventsActions_CameraOutput_Help);
}

QnCameraOutputBusinessActionWidget::~QnCameraOutputBusinessActionWidget()
{}

void QnCameraOutputBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->relayComboBox);
    setTabOrder(ui->relayComboBox, ui->fixedDurationCheckBox);
    setTabOrder(ui->fixedDurationCheckBox, ui->timeDuration);
    setTabOrder(ui->timeDuration, after);
}

void QnCameraOutputBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model())
        return;

    QScopedValueRollback<bool> rollback(m_updating, true);

    if (fields.testFlag(Field::eventType))
    {
        bool hasToggleState = nx::vms::event::hasToggleState(model()->eventType());
        if (!hasToggleState)
            ui->fixedDurationCheckBox->setChecked(true);
        setReadOnly(ui->fixedDurationCheckBox, !hasToggleState);
    }

    if (fields.testFlag(Field::actionResources))
    {
        QnIOPortDataList outputPorts;
        bool inited = false;

        auto cameras = resourcePool()->getResources<QnVirtualCameraResource>(model()->actionResources());
        foreach(const QnVirtualCameraResourcePtr &camera, cameras)
        {
            QnIOPortDataList cameraOutputs = camera->getRelayOutputList();
            if (!inited)
            {
                outputPorts = cameraOutputs;
                inited = true;
            }
            else
            {
                for (auto itr = outputPorts.begin(); itr != outputPorts.end();)
                {
                    const QnIOPortData& value = *itr;
                    bool found = false;
                    for (const auto& other : cameraOutputs)
                    {
                        if (other.id == value.id && other.getName() == value.getName())
                        {
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
        ui->relayComboBox->addItem(L'<' + tr("automatic") + L'>', QString());
        for (const auto& relayOutput : outputPorts)
            ui->relayComboBox->addItem(relayOutput.getName(), relayOutput.id);
    }

    if (fields.testFlag(Field::actionParams))
    {
        const auto params = model()->actionParams();
        QString text = params.relayOutputId;
        if (ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString() != text)
            ui->relayComboBox->setCurrentIndex(ui->relayComboBox->findData(text));

        int fixedDuration = params.durationMs / 1000;
        ui->fixedDurationCheckBox->setChecked(fixedDuration > 0);
        if (fixedDuration > 0)
        {
            ui->timeDuration->setValue(fixedDuration);
        }
    }
}

void QnCameraOutputBusinessActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    auto params = model()->actionParams();
    params.relayOutputId = ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString();
    params.durationMs = ui->fixedDurationCheckBox->isChecked()
        ? ui->timeDuration->value() * 1000
        : 0;

    model()->setActionParams(params);
}
