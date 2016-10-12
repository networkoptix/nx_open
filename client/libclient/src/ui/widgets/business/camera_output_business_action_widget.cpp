#include "camera_output_business_action_widget.h"
#include "ui_camera_output_business_action_widget.h"

#include <business/business_action_parameters.h>
#include <business/actions/camera_output_business_action.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/scoped_value_rollback.h>

QnCameraOutputBusinessActionWidget::QnCameraOutputBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::CameraOutputBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->autoResetCheckBox, &QCheckBox::toggled, ui->autoResetSpinBox,
        &QWidget::setEnabled);
    connect(ui->autoResetCheckBox, &QCheckBox::clicked, this,
        &QnCameraOutputBusinessActionWidget::paramsChanged);

    connect(ui->relayComboBox, QnComboboxCurrentIndexChanged, this,
        &QnCameraOutputBusinessActionWidget::paramsChanged);
    connect(ui->autoResetSpinBox, QnSpinboxIntValueChanged, this,
        &QnCameraOutputBusinessActionWidget::paramsChanged);

    connect(ui->autoResetCheckBox, &QCheckBox::toggled, this,
        [this](bool checked)
        {
            // Prolonged type of event has changed. In case of instant
            // action event state should be updated
            if (checked && (model()->eventType() == QnBusiness::UserDefinedEvent))
                model()->setEventState(QnBusiness::UndefinedState);
        });

    setHelpTopic(this, Qn::EventsActions_CameraOutput_Help);
}

QnCameraOutputBusinessActionWidget::~QnCameraOutputBusinessActionWidget()
{}

void QnCameraOutputBusinessActionWidget::updateTabOrder(QWidget *before, QWidget *after)
{
    setTabOrder(before, ui->relayComboBox);
    setTabOrder(ui->relayComboBox, ui->autoResetCheckBox);
    setTabOrder(ui->autoResetCheckBox, ui->autoResetSpinBox);
    setTabOrder(ui->autoResetSpinBox, after);
}

void QnCameraOutputBusinessActionWidget::at_model_dataChanged(QnBusiness::Fields fields)
{
    if (!model())
        return;

    QScopedValueRollback<bool> rollback(m_updating, true);

    if (fields.testFlag(QnBusiness::EventTypeField))
    {
        bool hasToggleState = QnBusiness::hasToggleState(model()->eventType());
        if (!hasToggleState)
            ui->autoResetCheckBox->setChecked(true);
        setReadOnly(ui->autoResetCheckBox, !hasToggleState);
    }

    if (fields.testFlag(QnBusiness::ActionResourcesField))
    {
        QnIOPortDataList outputPorts;
        bool inited = false;

        auto cameras = qnResPool->getResources<QnVirtualCameraResource>(model()->actionResources());
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
        ui->relayComboBox->addItem(tr("<automatic>"), QString());
        for (const auto& relayOutput : outputPorts)
            ui->relayComboBox->addItem(relayOutput.getName(), relayOutput.id);
    }

    if (fields.testFlag(QnBusiness::ActionParamsField))
    {
        QnBusinessActionParameters params = model()->actionParams();

        QString text = params.relayOutputId;
        if (ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString() != text)
            ui->relayComboBox->setCurrentIndex(ui->relayComboBox->findData(text));

        int autoReset = params.durationMs / 1000;
        ui->autoResetCheckBox->setChecked(autoReset > 0);
        if (autoReset > 0)
        {
            ui->autoResetSpinBox->setValue(autoReset);
        }
    }
}

void QnCameraOutputBusinessActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QnBusinessActionParameters params = model()->actionParams();
    params.relayOutputId = ui->relayComboBox->itemData(ui->relayComboBox->currentIndex()).toString();
    params.durationMs = ui->autoResetCheckBox->isChecked()
        ? ui->autoResetSpinBox->value() * 1000
        : 0;
    model()->setActionParams(params);
}
