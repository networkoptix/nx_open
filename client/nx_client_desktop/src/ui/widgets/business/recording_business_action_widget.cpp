#include "recording_business_action_widget.h"
#include "ui_recording_business_action_widget.h"

#include <nx/vms/event/action_parameters.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/scoped_value_rollback.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

static constexpr int kMsecPerSecond = 1000;

QnRecordingBusinessActionWidget::QnRecordingBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::RecordingBusinessActionWidget)
{
    ui->setupUi(this);

    for (int i = Qn::QualityLowest; i <= Qn::QualityHighest; i++) {
        ui->qualityComboBox->addItem(Qn::toDisplayString((Qn::StreamQuality)i), i);
    }

    static constexpr int kMaxPreRecordingSecs = 60;
    ui->beforeLabel->setVisible(true);
    ui->beforeSpinBox->setVisible(true);
    ui->beforeSpinBox->setMaximum(kMaxPreRecordingSecs);

    connect(ui->qualityComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->fpsSpinBox, SIGNAL(editingFinished()), this, SLOT(paramsChanged()));
    connect(ui->fpsSpinBox, SIGNAL(valueChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->beforeSpinBox, SIGNAL(valueChanged(int)), this, SLOT(paramsChanged()));
    connect(ui->afterSpinBox, SIGNAL(valueChanged(int)), this, SLOT(paramsChanged()));

    connect(ui->fixedDurationCheckBox, &QCheckBox::toggled, this,
        [this](bool checked)
        {
            ui->fixedDuration->setEnabled(checked);
            //ui->fixedDurationSuffixLabel->setEnabled(checked);

            // Prolonged type of event has changed. In case of instant
            // action event state should be updated.
            if (checked && (model()->eventType() == nx::vms::event::userDefinedEvent))
                model()->setEventState(nx::vms::event::EventState::undefined);

            emit paramsChanged();
        });

    ui->fixedDuration->addDurationSuffix(QnTimeStrings::Suffix::Minutes);
    ui->fixedDuration->addDurationSuffix(QnTimeStrings::Suffix::Hours);

    connect(ui->fixedDuration, SIGNAL(valueChanged()), this, SLOT(paramsChanged()));
}

QnRecordingBusinessActionWidget::~QnRecordingBusinessActionWidget()
{
}

void QnRecordingBusinessActionWidget::updateTabOrder(QWidget *before, QWidget *after)
{
    setTabOrder(before,                 ui->qualityComboBox);
    setTabOrder(ui->qualityComboBox,    ui->fpsSpinBox);
    setTabOrder(ui->fpsSpinBox,         ui->beforeLabel);
    setTabOrder(ui->beforeLabel,        ui->afterSpinBox);
    setTabOrder(ui->afterSpinBox,       ui->fixedDuration);
    setTabOrder(ui->fixedDuration, after);
}

void QnRecordingBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model())
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    int maxFps = 0;

    if (fields.testFlag(Field::eventType))
    {
        bool hasToggleState = nx::vms::event::hasToggleState(model()->eventType());
        if (!hasToggleState)
            ui->fixedDurationCheckBox->setChecked(true);
        setReadOnly(ui->fixedDurationCheckBox, !hasToggleState);
    }

    if (fields.testFlag(Field::actionResources))
    {
        auto cameras = resourcePool()->getResources<QnVirtualCameraResource>(model()->actionResources());
        for (const auto& camera: cameras)
            maxFps = (maxFps == 0 ? camera->getMaxFps() : qMax(maxFps, camera->getMaxFps()));

        ui->fpsSpinBox->setEnabled(maxFps > 0);
        ui->fpsSpinBox->setMaximum(maxFps);
        if (ui->fpsSpinBox->value() == 0 && maxFps > 0)
            ui->fpsSpinBox->setValue(maxFps);
        ui->fpsSpinBox->setMinimum(maxFps > 0 ? 1 : 0);
    }

    if (fields.testFlag(Field::actionParams))
    {
        const auto params = model()->actionParams();
        int quality = ui->qualityComboBox->findData((int) params.streamQuality);
        if (quality >= 0)
            ui->qualityComboBox->setCurrentIndex(quality);

        ui->fpsSpinBox->setValue(params.fps);
        ui->afterSpinBox->setValue(params.recordAfter);
        ui->beforeSpinBox->setValue(params.recordBeforeMs / kMsecPerSecond);

        int fixedDuration = params.durationMs / 1000;
        ui->fixedDurationCheckBox->setChecked(fixedDuration > 0);
        if (fixedDuration > 0)
            ui->fixedDuration->setValue(fixedDuration);

        const bool kFixedDuration = ui->fixedDurationCheckBox->isChecked();

        bool fixedEnabled = ui->fixedDurationCheckBox->isEnabled();
        ui->beforeSpinBox->setEnabled(!kFixedDuration);
        ui->beforeLabel->setEnabled(!kFixedDuration);
        ui->afterSpinBox->setEnabled(!kFixedDuration);
        ui->afterLabel->setEnabled(!kFixedDuration);
    }
}

void QnRecordingBusinessActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    nx::vms::event::ActionParameters params;
    params.fps = ui->fpsSpinBox->value();
    params.recordBeforeMs = ui->beforeSpinBox->value() * kMsecPerSecond;
    params.recordAfter = ui->afterSpinBox->value();

    params.streamQuality = (Qn::StreamQuality)ui->qualityComboBox->itemData(
        ui->qualityComboBox->currentIndex()).toInt();

    params.durationMs = ui->fixedDurationCheckBox->isChecked()
        ? ui->fixedDuration->value() * 1000
        : 0;

    model()->setActionParams(params);
}
