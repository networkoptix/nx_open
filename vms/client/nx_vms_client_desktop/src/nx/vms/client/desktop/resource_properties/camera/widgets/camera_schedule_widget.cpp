// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_schedule_widget.h"
#include "ui_camera_schedule_widget.h"

#include <core/misc/schedule_task.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/background_flasher.h>
#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/common/utils/provided_text_display.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource_properties/schedule/record_schedule_cell_painter.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/license/usage_helper.h>
#include <ui/common/palette.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/camera/camera_bitrate_calculator.h>
#include <utils/common/event_processors.h>
#include <utils/math/color_transformations.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"
#include "../utils/license_usage_provider.h"

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
using namespace std::chrono;

CameraScheduleWidget::CameraScheduleWidget(
    LicenseUsageProvider* licenseUsageProvider,
    CameraSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraScheduleWidget),
    m_scheduleCellPainter(new RecordScheduleCellPainter()),
    m_licenseUsageProvider(licenseUsageProvider)
{
    setupUi();
    NX_ASSERT(store && licenseUsageProvider);

    auto licenseUsageTextDisplay = new ProvidedTextDisplay(m_licenseUsageProvider, this);
    licenseUsageTextDisplay->setDisplayingWidget(ui->licenseUsageLabel);

    ui->licenseUsageLabel->setMinimumHeight(style::Metrics::kButtonHeight);
    ui->licenseUsageLabel->setAutoFillBackground(true);
    ui->licenseUsageLabel->setContentsMargins({
        style::Metrics::kStandardPadding, 0, style::Metrics::kStandardPadding, 0});

    ui->gridWidget->setCellPainter(m_scheduleCellPainter.get());
    ui->scheduleSettingsWidget->setScheduleCellPainter(m_scheduleCellPainter.get());

    ui->scheduleSettingsWidget->setStore(store);
    ui->archiveLengthWidget->setStore(store);
    ui->recordingThresholdWidget->setStore(store);

    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &CameraScheduleWidget::loadState);

    connect(ui->enableRecordingCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setRecordingEnabled);

    connect(ui->enableRecordingCheckBox, &CheckBox::cannotBeToggled, this,
        [this] { BackgroundFlasher::flash(ui->licenseUsageLabel, core::colorTheme()->color("red_l2")); });

    // Called when a cell is Alt-clicked. Fetches cell settings as current.
    connect(ui->gridWidget, &ScheduleGridWidget::cellClicked, store,
        [this, store](Qt::DayOfWeek dayOfWeek, int hour, Qt::KeyboardModifiers modifiers)
        {
            if (modifiers.testFlag(Qt::AltModifier))
            {
                const auto cellData = ui->gridWidget->cellData(dayOfWeek, hour);
                if (!cellData.isNull())
                    store->setScheduleBrush(cellData.value<RecordScheduleCellData>());
            }
        });

    connect(ui->gridWidget, &ScheduleGridWidget::gridDataChanged, store,
        [this, store]()
        {
            store->setSchedule(calculateScheduleTasks());
        });

    connect(ui->licensesButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(action::PreferencesLicensesTabAction); });

    connect(ui->exportScheduleButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(action::CopyRecordingScheduleAction); });

    connect(licenseUsageProvider, &LicenseUsageProvider::stateChanged, store,
        [this, store]()
        {
            ui->enableRecordingCheckBox->setCanBeChecked(!m_licenseUsageProvider->limitExceeded());
            updateLicensesButton(store->state());
        });
}

CameraScheduleWidget::~CameraScheduleWidget()
{
    // Required here for forward-declared scoped pointer destruction.
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

    setHelpTopic(this, HelpTopic::Id::CameraSettingsRecordingPage);
    setHelpTopic(ui->licensesButton, HelpTopic::Id::Licenses);
    setHelpTopic(ui->licenseUsageLabel, HelpTopic::Id::Licenses);
    setHelpTopic(ui->exportScheduleButton, HelpTopic::Id::CameraSettings_Recording_Export);
}

void CameraScheduleWidget::loadState(const CameraSettingsDialogState& state)
{
    const auto& recording = state.recording;
    const auto parametersAvailable = recording.parametersAvailable && state.cameraControlEnabled();

    RecordScheduleCellPainter::DisplayOptions options;
    options.setFlag(RecordScheduleCellPainter::DisplayOption::ShowFps,
        recording.showFps && parametersAvailable);
    options.setFlag(RecordScheduleCellPainter::DisplayOption::ShowQuality,
        recording.showQuality && parametersAvailable);
    m_scheduleCellPainter->setDisplayOptions(options);
    ui->gridWidget->update();
    setReadOnly(ui->gridWidget, state.readOnly);

    auto fixedBrush = recording.brush;
    if (fixedBrush.recordingType == Qn::RecordingType::never)
    {
        fixedBrush.fps = 0;
        fixedBrush.streamQuality = Qn::StreamQuality::undefined;
    }
    ui->gridWidget->setBrush(QVariant::fromValue<RecordScheduleCellData>(fixedBrush));

    const bool recordingEnabled = recording.enabled.valueOr(false);
    check_box_utils::setupTristateCheckbox(ui->enableRecordingCheckBox, state.recording.enabled);
    setReadOnly(ui->enableRecordingCheckBox, state.readOnly);

    setReadOnly(ui->exportScheduleButton, state.readOnly);
    setReadOnly(ui->archiveLengthWidget, state.readOnly);

    updateLicensesButton(state);

    ui->gridWidget->resetGridData();
    const auto& schedule = state.recording.schedule;
    ui->gridWidget->setActive(schedule.hasValue());
    ui->copyScheduleGroupBox->setVisible(state.isSingleCamera() && NX_ASSERT(schedule.hasValue()));

    if (schedule.hasValue())
    {
        for (const auto& task: schedule())
        {
            const auto dayOfWeek = static_cast<Qt::DayOfWeek>(task.dayOfWeek);
            Qn::StreamQuality streamQuality = Qn::StreamQuality::undefined;
            qreal bitrateMbps = 0.0;

            if (task.recordingType != Qn::RecordingType::never)
            {
                switch (task.streamQuality)
                {
                    case Qn::StreamQuality::low:
                    case Qn::StreamQuality::normal:
                    case Qn::StreamQuality::high:
                    case Qn::StreamQuality::highest:
                        streamQuality = task.streamQuality;
                        bitrateMbps =
                            common::CameraBitrateCalculator::roundKbpsToMbps(task.bitrateKbps);
                        break;
                    default:
                        break;
                }
            }

            for (int hour = task.startTime / 3600; hour < task.endTime / 3600; ++hour)
            {
                RecordScheduleCellData cellData;

                cellData.recordingType = task.recordingType;
                cellData.metadataTypes = task.metadataTypes;
                cellData.streamQuality = streamQuality;
                cellData.fps = task.fps;
                cellData.bitrateMbitPerSec = state.recording.customBitrateAvailable
                    ? bitrateMbps
                    : RecordScheduleCellData::kAutoBitrateValue;

                ui->gridWidget->setCellData(dayOfWeek, hour,
                    QVariant::fromValue<RecordScheduleCellData>(cellData));
            }
        }
    }

    const bool metadataEnabled = state.isMotionDetectionActive()
        || state.isObjectDetectionSupported();
    ui->recordingThresholdGroupBox->setEnabled(metadataEnabled);
    QString recordingThresholdGroupBoxTooltip;
    if (!metadataEnabled)
    {
        recordingThresholdGroupBoxTooltip =
            ScheduleSettingsWidget::disabledMotionAndObjectDetectionTooltip(state);
    }
    ui->recordingThresholdGroupBox->setToolTip(recordingThresholdGroupBoxTooltip);

    ui->scheduleSettingsWidget->setEnabled(recordingEnabled);
    setLayoutEnabled(ui->recordingScheduleLayout, recordingEnabled);
    setLayoutEnabled(ui->bottomParametersLayout, recordingEnabled);

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
            }

            return QString();
        }());

    ui->recordingAlertBar->setText(
        [&state]()
        {
            if (!state.recordingAlert)
                return QString();

            switch (*state.recordingAlert)
            {
                case CameraSettingsDialogState::RecordingAlert::highArchiveLength:
                {
                    return QnCameraDeviceStringSet(
                        tr("High minimum value can lead to archive length "
                                "decrease on other devices."),
                        tr("High minimum value can lead to archive length "
                                "decrease on other cameras."))
                            .getString(state.deviceType);
                }

                case CameraSettingsDialogState::RecordingAlert::highPreRecordingValue:
                {
                    return tr("High pre-recording time will increase RAM utilization "
                        "on the server");
                }
            }

            NX_ASSERT(false);
            return QString();
        }());
}

QnScheduleTaskList CameraScheduleWidget::calculateScheduleTasks() const
{
    QnScheduleTaskList tasks;

    for (const auto dayOfWeek: nx::vms::common::daysOfWeek())
    {
        QnScheduleTask task;

        for (int hour = 0; hour < nx::vms::common::kHoursInDay;)
        {
            const auto cellDataRaw = ui->gridWidget->cellData(dayOfWeek, hour);
            const auto cellData = cellDataRaw.isValid()
                ? cellDataRaw.value<RecordScheduleCellData>()
                : RecordScheduleCellData{.recordingType = nx::vms::api::RecordingType::never};

            const auto recordingType = cellData.recordingType;
            const auto metadataTypes = cellData.metadataTypes;

            Qn::StreamQuality streamQuality = Qn::StreamQuality::highest;
            int bitrateKbps = 0;

            if (recordingType != Qn::RecordingType::never)
            {
                streamQuality = cellData.streamQuality;
                bitrateKbps = qRound(common::CameraBitrateCalculator::roundMbpsToKbps(
                    cellData.bitrateMbitPerSec));
            }

            int fps = cellData.fps;
            if (fps == 0 && recordingType != Qn::RecordingType::never)
                fps = 10;

            if (task.startTime == task.endTime)
            {
                // An invalid one, initialize.
                task.dayOfWeek = dayOfWeek;
                task.startTime = hour * 3600;     //< In seconds from the start of the day.
                task.endTime = (hour + 1) * 3600; //< In seconds from the start of the day.
                task.recordingType = recordingType;
                task.metadataTypes = metadataTypes;
                task.streamQuality = streamQuality;
                task.bitrateKbps = bitrateKbps;
                task.fps = fps;
                ++hour;
            }
            else if (task.recordingType == recordingType
                && task.metadataTypes == metadataTypes
                && task.streamQuality == streamQuality
                && task.bitrateKbps == bitrateKbps
                && task.fps == fps)
            {
                task.endTime = (hour + 1) * 3600; //< In seconds from the start of the day.
                ++hour;
            }
            else
            {
                tasks.push_back(task);
                task = QnScheduleTask();
            }
        }

        if (task.startTime != task.endTime)
            tasks.push_back(task);
    }

    return tasks;
}

void CameraScheduleWidget::updateLicensesButton(const CameraSettingsDialogState& state)
{
    ui->licensesButton->setVisible(m_licenseUsageProvider
        && m_licenseUsageProvider->limitExceeded()
        && state.hasPowerUserPermissions
        && !state.saasInitialized);
}

} // namespace nx::vms::client::desktop
