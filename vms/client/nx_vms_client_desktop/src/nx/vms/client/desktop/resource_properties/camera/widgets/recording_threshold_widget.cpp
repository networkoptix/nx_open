// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recording_threshold_widget.h"
#include "ui_recording_threshold_widget.h"

#include <nx/vms/text/time_strings.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

RecordingThresholdWidget::RecordingThresholdWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::RecordingThresholdWidget())
{
    ui->setupUi(this);

    ui->recordBeforeSpinBox->setSuffix(
        L' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));
    ui->recordAfterSpinBox->setSuffix(
        L' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));
}

RecordingThresholdWidget::~RecordingThresholdWidget() = default;

void RecordingThresholdWidget::setStore(CameraSettingsDialogStore* store)
{
    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &RecordingThresholdWidget::loadState);

    connect(ui->recordBeforeSpinBox, QnSpinboxIntValueChanged, store,
        [store](int value)
        {
            store->setRecordingBeforeThresholdSec(value);
        });

    connect(ui->recordAfterSpinBox, QnSpinboxIntValueChanged, store,
        [store](int value)
        {
            store->setRecordingAfterThresholdSec(value);
        });
}

void RecordingThresholdWidget::loadState(const CameraSettingsDialogState& state)
{
    if (!state.supportsSchedule())
        return;

    const bool supportsRecordingByEvents = state.supportsRecordingByEvents();
    ui->recordBeforeSpinBox->setEnabled(supportsRecordingByEvents);
    ui->recordAfterSpinBox->setEnabled(supportsRecordingByEvents);
    if (supportsRecordingByEvents)
    {
        ui->recordBeforeSpinBox->setValue(state.recording.thresholds.beforeSec.valueOr(
            nx::vms::api::kDefaultRecordBeforeMotionSec));
        setReadOnly(ui->recordBeforeSpinBox, state.readOnly);

        ui->recordAfterSpinBox->setValue(state.recording.thresholds.afterSec.valueOr(
            nx::vms::api::kDefaultRecordAfterMotionSec));
        setReadOnly(ui->recordAfterSpinBox, state.readOnly);
    }
}

} // namespace nx::vms::client::desktop
