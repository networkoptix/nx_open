#include "recording_threshold_widget.h"
#include "ui_recording_threshold_widget.h"

#include <text/time_strings.h>

#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

namespace nx {
namespace client {
namespace desktop {

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
    ui->recordBeforeSpinBox->setValue(state.recording.thresholds.beforeSec.valueOr(
        ec2::kDefaultRecordBeforeMotionSec));
    setReadOnly(ui->recordBeforeSpinBox, state.readOnly);

    ui->recordAfterSpinBox->setValue(state.recording.thresholds.afterSec.valueOr(
        ec2::kDefaultRecordAfterMotionSec));
    setReadOnly(ui->recordAfterSpinBox, state.readOnly);
}

} // namespace desktop
} // namespace client
} // namespace nx
