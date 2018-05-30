#include "camera_schedule_widget.h"
#include "ui_camera_schedule_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"
#include "../export_schedule_resource_selection_dialog_delegate.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/misc/schedule_task.h>
#include <ui/common/palette.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <utils/camera/camera_bitrate_calculator.h>
#include <utils/common/event_processors.h>
#include <utils/math/color_transformations.h>
#include <utils/math/math.h>
#include <utils/license_usage_helper.h>

#include <nx/client/desktop/common/utils/checkbox_utils.h>
#include <nx/client/desktop/common/utils/aligner.h>
#include <nx/client/desktop/common/utils/provided_text_display.h>
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

namespace nx {
namespace client {
namespace desktop {

using namespace ui;

CameraScheduleWidget::CameraScheduleWidget(
    AbstractTextProvider* licenseUsageTextProvider,
    CameraSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::CameraScheduleWidget)
{
    setupUi();
    NX_ASSERT(store && licenseUsageTextProvider);

    auto licenseUsageTextDisplay = new ProvidedTextDisplay(licenseUsageTextProvider, this);
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
            // TODO: updateAlert(ScheduleChange);
        });

    connect(ui->licensesButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(action::PreferencesLicensesTabAction); });

    /*
    QnCamLicenseUsageHelper helper(commonModule());
    ui->licensesUsageWidget->init(&helper);

    auto notifyAboutScheduleEnabledChanged =
        [this](int state)
        {
            updateAlert(EnabledChange);
            emit scheduleEnabledChanged(state);
        };


    connect(ui->licensesButton, &QPushButton::clicked, this,
        &CameraScheduleWidget::at_licensesButton_clicked);

    connect(ui->enableRecordingCheckBox, &QCheckBox::stateChanged, this,
        notifyAboutScheduleEnabledChanged);

    connect(ui->exportScheduleButton, &QPushButton::clicked, this,
        &CameraScheduleWidget::at_exportScheduleButton_clicked);

    connect(ui->archiveLengthWidget, &QnArchiveLengthWidget::alertChanged, this,
        &nx::client::desktop::CameraScheduleWidget::alert);

    ui->exportWarningLabel->setVisible(false);

    auto updateLicensesIfNeeded =
        [this]
        {
            if (!isVisible())
                return;
            updateLicensesLabelText();
        };
    */
}

void CameraScheduleWidget::setupUi()
{
    ui->setupUi(this);

    auto scrollBar = new QnSnappedScrollBar(this);
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
    auto settingsHint = nx::client::desktop::HintButton::hintThat(ui->settingsGroupBox);
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
    CheckboxUtils::setupTristateCheckbox(ui->enableRecordingCheckBox,
        state.recording.enabled.hasValue(),
        recordingEnabled);
    setReadOnly(ui->enableRecordingCheckBox, state.readOnly);

    ui->licensesButton->setVisible(state.globalPermissions.testFlag(Qn::GlobalAdminPermission));

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
}

ScheduleTasks CameraScheduleWidget::calculateScheduleTasks() const
{
    ScheduleTasks tasks;
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

/*
void CameraScheduleWidget::afterContextInitialized()
{

    connect(
        context(),
        &QnWorkbenchContext::userChanged,
        this,
        &CameraScheduleWidget::updateLicensesButtonVisible);
    updateLicensesButtonVisible();

}

void CameraScheduleWidget::setCameras(const QnVirtualCameraResourceList &cameras)
{
    for (const auto& camera: m_cameras)
    {
        connect(camera, &QnSecurityCamResource::resourceChanged,
            this, &CameraScheduleWidget::at_cameraResourceChanged, Qt::QueuedConnection);
        connect(camera, &QnResource::propertyChanged, this,
            [this](const QnResourcePtr& resource, const QString& propertyName)
            {
                if (propertyName == nx::media::kCameraMediaCapabilityParamName
                    || propertyName == Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME)
                {
                    syncBitrateWithFps();
                }
            });
    }
}


void CameraScheduleWidget::loadDataToUi()
{
    updateMaxFPS();
    updateMotionAvailable();
    updateRecordingParamsAvailable();

    if (m_cameras.isEmpty())
    {
        setScheduleTasks(QnScheduleTaskList());
    }
    else
    {
        bool isScheduleEqual = true;
        bool isFpsValid = true;
        for (const auto& camera : m_cameras)
        {
            QList<QnScheduleTask::Data> cameraScheduleTasksData;
            for (const auto& scheduleTask : camera->getScheduleTasks())
            {
                switch (scheduleTask.getRecordingType())
                {
                    case Qn::RecordingType::never:
                        continue;
                    case Qn::RecordingType::motionAndLow:
                        isFpsValid &= scheduleTask.getFps() <= m_maxDualStreamingFps;
                        break;
                    case Qn::RecordingType::always:
                    case Qn::RecordingType::motionOnly:
                        isFpsValid &= scheduleTask.getFps() <= m_maxFps;
                        break;
                    default:
                        break;
                }
            }
        }

        ui->gridWidget->setActive(isScheduleEqual && isFpsValid);
    }
    updateMotionButtons();
    updateLicensesLabelText();
    updateGridParams();
    setScheduleAlert(QString());
}

void CameraScheduleWidget::applyChanges()
{
    if (!canChangeRecording)
    {
        QSignalBlocker blocker(ui->enableRecordingCheckBox);
        updateScheduleEnabled();
        setScheduleAlert(QString());
        updateLicensesLabelText();
    }
}

void CameraScheduleWidget::setExportScheduleButtonEnabled(bool enabled)
{
    ui->exportScheduleButton->setEnabled(enabled);
    ui->exportWarningLabel->setVisible(!enabled);
}


bool CameraScheduleWidget::canEnableRecording() const
{
    QnCamLicenseUsageHelper licenseHelper(commonModule());
    return licenseHelper.canEnableRecording(m_cameras);
}

int CameraScheduleWidget::getGridMaxFps(bool motionPlusLqOnly)
{
    return ui->gridWidget->getMaxFps(motionPlusLqOnly);
}

void CameraScheduleWidget::updateLicensesLabelText()
{
    QnCamLicenseUsageHelper helper(commonModule());

    switch (ui->enableRecordingCheckBox->checkState())
    {
        case Qt::Checked:
            helper.propose(m_cameras, true);
            break;
        case Qt::Unchecked:
            helper.propose(m_cameras, false);
            break;
        default:
            break;
    }
    ui->licensesUsageWidget->loadData(&helper);
}

void CameraScheduleWidget::updateLicensesButtonVisible()
{
    ui->licensesButton->setVisible(accessController()->hasGlobalPermission(Qn::GlobalAdminPermission));
}

void CameraScheduleWidget::at_cameraResourceChanged()
{
    updateMaxFPS();
    updateMotionButtons();
}

void CameraScheduleWidget::updateMotionButtons()
{
    if (!m_motionAvailable)
    {
        for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
        {
            for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
            {
                const QPoint cell(col, row);
                auto params = ui->gridWidget->cellValue(cell);
                Qn::RecordingType recordType = params.recordingType;
                if (recordType == Qn::RecordingType::motionOnly || recordType == Qn::RecordingType::motionAndLow)
                {
                    params.recordingType = Qn::RecordingType::always;
                    ui->gridWidget->setCellValue(cell, params);
                }
            }
        }
    }
}

void CameraScheduleWidget::at_licensesButton_clicked()
{
    menu()->trigger(action::PreferencesLicensesTabAction);
}

void CameraScheduleWidget::at_exportScheduleButton_clicked()
{
    if (m_cameras.size() > 1)
    {
        NX_EXPECT(false, Q_FUNC_INFO);
        return;
    }

    bool recordingEnabled = ui->enableRecordingCheckBox->checkState() == Qt::Checked;
    bool motionUsed = recordingEnabled && hasMotionOnGrid();
    bool dualStreamingUsed = motionUsed && hasDualStreamingMotionOnGrid();
    bool hasVideo = boost::algorithm::all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasVideo(0); });

    QScopedPointer<QnResourceSelectionDialog> dialog(
        new QnResourceSelectionDialog(QnResourceSelectionDialog::Filter::cameras, this));
    auto dialogDelegate = new ExportScheduleResourceSelectionDialogDelegate(this, recordingEnabled, motionUsed, dualStreamingUsed, hasVideo);
    dialog->setDelegate(dialogDelegate);

    QSet<QnUuid> ids;
    for (auto camera: m_cameras)
        ids << camera->getId();
    dialog->setSelectedResources(ids);
    setHelpTopic(dialog.data(), Qn::CameraSettings_Recording_Export_Help);
    if (!dialog->exec())
        return;

    const bool copyArchiveLength = dialogDelegate->doCopyArchiveLength();

    auto applyChanges =
        [this, sourceCamera = m_cameras.front(), copyArchiveLength, recordingEnabled]
            (const QnVirtualCameraResourcePtr &camera)
        {
            if (copyArchiveLength)
                ui->archiveLengthWidget->submitToResources({camera});

            camera->setScheduleDisabled(!recordingEnabled);
            int maxFps = camera->getMaxFps();

            // TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
            // or just use camera->reservedSecondStreamFps();

            int decreaseAlways = 0;
            if (camera->streamFpsSharingMethod() == Qn::BasicFpsSharing && camera->getMotionType() == Qn::MotionType::MT_SoftwareGrid)
                decreaseAlways = QnLiveStreamParams::kMinSecondStreamFps;

            int decreaseIfMotionPlusLQ = 0;
            if (camera->streamFpsSharingMethod() == Qn::BasicFpsSharing)
                decreaseIfMotionPlusLQ = QnLiveStreamParams::kMinSecondStreamFps;

            QnScheduleTaskList tasks;
            for (auto task: scheduleTasks())
            {
                if (task.getRecordingType() == Qn::RecordingType::motionAndLow)
                    task.setFps(qMin(task.getFps(), maxFps - decreaseIfMotionPlusLQ));
                else
                    task.setFps(qMin(task.getFps(), maxFps - decreaseAlways));

                if (const auto bitrate = task.getBitrateKbps()) // Try to calculate new custom bitrate
                {
                    // Target camera supports custom bitrate
                    const auto normalBitrate = getBitrateForQuality(sourceCamera,
                        task.getStreamQuality(), task.getFps(), ui->bitrateSpinBox->decimals());
                    const auto bitrateAspect = (bitrate - normalBitrate) / normalBitrate;

                    const auto targetNormalBitrate = getBitrateForQuality(camera,
                        task.getStreamQuality(), task.getFps(), ui->bitrateSpinBox->decimals());
                    const auto targetBitrate = targetNormalBitrate * bitrateAspect;
                    task.setBitrateKbps(targetBitrate);
                }
                tasks.append(task);
            }
            updateRecordThresholds(tasks);

            camera->setScheduleTasks(tasks);
        };

    auto selectedCameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
        dialog->selectedResources());
    qnResourcesChangesManager->saveCameras(selectedCameras, applyChanges);
    updateLicensesLabelText();
}

bool CameraScheduleWidget::hasMotionOnGrid() const
{
    for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
    {
        for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
        {
            const QPoint cell(col, row);
            Qn::RecordingType recordType = ui->gridWidget->cellValue(cell).recordingType;
            if (recordType == Qn::RecordingType::motionOnly || recordType == Qn::RecordingType::motionAndLow)
                return true;
        }
    }
    return false;
}

bool CameraScheduleWidget::hasDualStreamingMotionOnGrid() const
{
    for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
    {
        for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
        {
            const QPoint cell(col, row);
            if (ui->gridWidget->cellValue(cell).recordingType == Qn::RecordingType::motionAndLow)
                return true;
        }
    }
    return false;
}

void CameraScheduleWidget::setArchiveLengthAlert(const QString& archiveLengthAlert)
{
    if (!m_scheduleAlert.isEmpty())
        emit alert(m_scheduleAlert);
    else
        emit alert(ui->archiveLengthWidget->alert());
}

void CameraScheduleWidget::updateAlert(AlertReason when)
{
    // License check and alert:
    const auto checkCanEnableRecording =
        [this]() -> bool
        {
            if (canEnableRecording())
                return true;

            switch (ui->enableRecordingCheckBox->checkState())
            {
                case Qt::Unchecked:
                case Qt::PartiallyChecked:
                    setScheduleAlert(Alert::notEnoughLicenses);
                    break;

                case Qt::Checked:
                    setScheduleAlert(Alert::licenseLimitExceeded);
                    break;

                default:
                    break;
            }

            return false;
        };

    switch (when)
    {
        // Current "brush" was changed (mode, fps, quality):
        case CurrentParamsChange:
        {
            if (checkCanEnableRecording() && !isRecordingScheduled())
                setScheduleAlert(Alert::brushChanged);
            break;
        }

        // Some cell(s) in the schedule were changed:
        case ScheduleChange:
        {
            if (isRecordingScheduled())
            {
                if (!checkCanEnableRecording())
                    break;

                if (!isScheduleEnabled())
                {
                    setScheduleAlert(Alert::RecordingIsNotEnabled);
                    break;
                }
            }

            setScheduleAlert(QString());
            break;
        }

        // Recording was enabled or disabled:
        case EnabledChange:
        {
            if (isScheduleEnabled())
            {
                if (!checkCanEnableRecording())
                    break;

                if (!isRecordingScheduled())
                {
                    setScheduleAlert(Alert::EmptySchedule);
                    break;
                }
            }

            setScheduleAlert(QString());
            break;
        }
    }
}

bool CameraScheduleWidget::isRecordingScheduled() const
{
    return any_of(scheduleTasks(),
        [](const QnScheduleTask& task) -> bool
        {
            return !task.isEmpty() && task.getRecordingType() != Qn::RecordingType::never;
        });
}
*/

} // namespace desktop
} // namespace client
} // namespace nx
