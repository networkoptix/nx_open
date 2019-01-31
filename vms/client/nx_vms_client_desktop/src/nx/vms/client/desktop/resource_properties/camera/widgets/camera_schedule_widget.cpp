#include "camera_schedule_widget.h"
#include "ui_camera_schedule_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"
#include "../utils/license_usage_provider.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/misc/schedule_task.h>
#include <ui/common/palette.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <utils/camera/camera_bitrate_calculator.h>
#include <utils/common/event_processors.h>
#include <utils/math/color_transformations.h>
#include <utils/math/math.h>
#include <utils/license_usage_helper.h>

#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/provided_text_display.h>
#include <nx/utils/log/assert.h>

namespace {

void setLayoutEnabled(QLayout* layout, bool enabled)
{
    const auto count = layout->count();
    for (int i = 0; i != count; ++i)
    {
        const auto layoutItem = layout->itemAt(i);
        if (const auto widget = layoutItem->widget())
            widget->setEnabled(enabled);
        if (const auto childLayout = layoutItem->layout())
            setLayoutEnabled(childLayout, enabled);
    }
}

} // namespace

namespace nx::vms::client::desktop {

using namespace ui;

CameraScheduleWidget::CameraScheduleWidget(
    LicenseUsageProvider* licenseUsageProvider,
    CameraSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraScheduleWidget)
{
    setupUi();
    NX_ASSERT(store && licenseUsageProvider);

    auto licenseUsageTextDisplay = new ProvidedTextDisplay(licenseUsageProvider, this);
    licenseUsageTextDisplay->setDisplayingWidget(ui->licenseUsageLabel);

    ui->licenseUsageLabel->setMinimumHeight(style::Metrics::kButtonHeight);

    ui->scheduleSettingsWidget->setStore(store);
    ui->archiveLengthWidget->setStore(store);
    ui->recordingThresholdWidget->setStore(store);

    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &CameraScheduleWidget::loadState);

    connect(ui->enableRecordingCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setRecordingEnabled);

    // Called when a cell is Alt-clicked. Fetches cell settings as current.
    connect(ui->gridWidget, &ScheduleGridWidget::cellActivated, store,
        [this, store](const QPoint& pos)
        {
            store->setScheduleBrush(ui->gridWidget->cellValue(pos));
        });

    connect(ui->gridWidget, &ScheduleGridWidget::cellValuesChanged, store,
        [this, store]()
        {
            store->setSchedule(calculateScheduleTasks());
        });

    connect(ui->licensesButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(action::PreferencesLicensesTabAction); });

    connect(ui->exportScheduleButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(action::CopyRecordingScheduleAction); });

    connect(licenseUsageProvider, &LicenseUsageProvider::stateChanged, store,
        [this, store, licenseUsageProvider]()
        {
            ui->licenseAlertBar->setText(licenseUsageProvider->limitExceeded()
                && store->state().recording.enabled.valueOr(false)
                    ? tr("License limit exceeded, recording will not be enabled.")
                    : QString());
        });
}

void CameraScheduleWidget::setupUi()
{
    ui->setupUi(this);

    auto scrollBar = new SnappedScrollBar(ui->mainWidget);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseMaximumSpace(true);

    ui->enableRecordingCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->enableRecordingCheckBox->setForegroundRole(QPalette::ButtonText);

    ui->scheduleGridGroupBox->setTitle(lit("%1\t(%2)").arg(
        tr("Recording Schedule")).arg(
            tr("based on server time")));

    installEventHandler(ui->scrollAreaWidgetContents, QEvent::Resize, this,
        [this]()
        {
            // Since external scroll bar is used it shouldn't be added to minimum width.
            ui->scrollArea->setMinimumWidth(
                ui->scrollAreaWidgetContents->minimumSizeHint().width());
        });

    //TODO: #dkargin Restore hints.
    /*
    auto settingsHint = nx::vms::client::desktop::HintButton::hintThat(ui->settingsGroupBox);
    settingsHint->addHintLine(tr("First choose a recording option, then apply it to day and time blocks on the recording schedule. (0 block is 12:00am to 1:00am, 23 block is 11:00pm to 12:00am.)"));
    ui->scheduleGridGroupBox->setTitle(
        lit("%1\t(%2)").arg(
            tr("Recording Schedule")).arg(
            tr("based on server time")));
    */

    setHelpTopic(ui->exportScheduleButton, Qn::CameraSettings_Recording_Export_Help);
}

void CameraScheduleWidget::loadState(const CameraSettingsDialogState& state)
{
    const auto& recording = state.recording;
    ui->gridWidget->setShowFps(recording.showFps && recording.parametersAvailable);
    ui->gridWidget->setShowQuality(recording.showQuality && recording.parametersAvailable);
    setReadOnly(ui->gridWidget, state.readOnly);

    auto actualBrush = recording.brush;
    if (actualBrush.recordingType == Qn::RecordingType::never)
    {
        actualBrush.fps = 0;
        actualBrush.quality = Qn::StreamQuality::undefined;
    }
    ui->gridWidget->setBrush(actualBrush);

    const bool recordingEnabled = recording.enabled.valueOr(false);
    check_box_utils::setupTristateCheckbox(ui->enableRecordingCheckBox, state.recording.enabled);
    setReadOnly(ui->enableRecordingCheckBox, state.readOnly);

    ui->licensesButton->setVisible(state.globalPermissions.testFlag(GlobalPermission::admin));

    setReadOnly(ui->exportScheduleButton, state.readOnly);
    setReadOnly(ui->exportWarningLabel, state.readOnly);
    setReadOnly(ui->archiveLengthWidget, state.readOnly);

    ui->gridWidget->resetCellValues();
    const auto& schedule = state.recording.schedule;
    ui->gridWidget->setActive(schedule.hasValue());

    if (schedule.hasValue())
    {
        for (const auto& task: schedule())
        {
            const int row = task.dayOfWeek - 1;
            Qn::StreamQuality quality = Qn::StreamQuality::undefined;
            qreal bitrateMbps = 0.0;

            if (task.recordingType != Qn::RecordingType::never)
            {
                switch (task.streamQuality)
                {
                    case Qn::StreamQuality::low:
                    case Qn::StreamQuality::normal:
                    case Qn::StreamQuality::high:
                    case Qn::StreamQuality::highest:
                        quality = task.streamQuality;
                        bitrateMbps = core::CameraBitrateCalculator::roundKbpsToMbps(
                            task.bitrateKbps);
                        break;
                    default:
                        break;
                }
            }

            for (int col = task.startTime / 3600; col < task.endTime / 3600; ++col)
            {
                const QPoint cell(col, row);
                ScheduleGridWidget::CellParams params;
                params.recordingType = task.recordingType;
                params.quality = quality;
                params.fps = task.fps;
                params.bitrateMbps = state.recording.customBitrateAvailable ? bitrateMbps : 0.0;
                ui->gridWidget->setCellValue(cell, params);
            }
        }
    }

    ui->scheduleSettingsWidget->setEnabled(recordingEnabled);
    setLayoutEnabled(ui->recordingScheduleLayout, recordingEnabled);
    setLayoutEnabled(ui->bottomParametersLayout, recordingEnabled);

    ui->copyScheduleGroupBox->setVisible(state.isSingleCamera());
    ui->exportScheduleButton->setEnabled(!state.hasChanges);
    ui->exportWarningLabel->setVisible(state.hasChanges);

    loadAlerts(state);
}

void CameraScheduleWidget::loadAlerts(const CameraSettingsDialogState& state)
{
    ui->hintBar->setText(
        [&state]()
        {
            if (!state.recordingHint)
                return QString();

            switch (*state.recordingHint)
            {
                case CameraSettingsDialogState::RecordingHint::brushChanged:
                    return tr("Select areas on the schedule to apply chosen parameters to.");

                case CameraSettingsDialogState::RecordingHint::emptySchedule:
                    return tr("Set recording parameters and select areas "
                        "on the schedule grid to apply them to.");

                case CameraSettingsDialogState::RecordingHint::recordingIsNotEnabled:
                    return tr("Turn on selector at the top of the window to enable recording.");
            }

            return QString();
        }());

    ui->recordingAlertBar->setText(
        [&state]()
        {
            if (!state.recordingAlert)
                return QString();

            NX_ASSERT(*state.recordingAlert ==
                CameraSettingsDialogState::RecordingAlert::highArchiveLength);

            return QnCameraDeviceStringSet(
                tr("High minimum value can lead to archive length decrease on other devices."),
                tr("High minimum value can lead to archive length decrease on other cameras."))
                .getString(state.deviceType);
        }());
}

QnScheduleTaskList CameraScheduleWidget::calculateScheduleTasks() const
{
    QnScheduleTaskList tasks;
    if (!ui->gridWidget->isActive())
        return tasks;

    for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
    {
        QnScheduleTask task;

        for (int col = 0; col < ui->gridWidget->columnCount();)
        {
            const QPoint cell(col, row);
            const ScheduleGridWidget::CellParams params = ui->gridWidget->cellValue(cell);

            const auto recordingType = params.recordingType;
            Qn::StreamQuality streamQuality = Qn::StreamQuality::highest;
            int bitrateKbps = 0;

            if (recordingType != Qn::RecordingType::never)
            {
                streamQuality = params.quality;
                bitrateKbps = qRound(
                    core::CameraBitrateCalculator::roundMbpsToKbps(params.bitrateMbps));
            }

            int fps = params.fps;
            if (fps == 0 && recordingType != Qn::RecordingType::never)
                fps = 10;

            if (task.startTime == task.endTime)
            {
                // An invalid one; initialize.
                task.dayOfWeek = row + 1;
                task.startTime = col * 3600;     //< In secs from start of day.
                task.endTime = (col + 1) * 3600; //< In secs from start of day.
                task.recordingType = recordingType;
                task.streamQuality = streamQuality;
                task.bitrateKbps = bitrateKbps;
                task.fps = fps;
                ++col;
            }
            else if (task.recordingType == recordingType
                && task.streamQuality == streamQuality
                && task.bitrateKbps == bitrateKbps
                && task.fps == fps)
            {
                task.endTime = (col + 1) * 3600; //< In secs from start of day.
                ++col;
            }
            else
            {
                tasks.append(task);
                task = QnScheduleTask();
            }
        }

        if (task.startTime != task.endTime)
            tasks.append(task);
    }

    return tasks;
}

CameraScheduleWidget::~CameraScheduleWidget()
{
}

} // namespace nx::vms::client::desktop
