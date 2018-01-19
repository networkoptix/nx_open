#include "camera_schedule_widget.h"
#include "ui_camera_schedule_widget.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QScopedValueRollback>

// TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <nx_ec/data/api_camera_attributes_data.h>

#include <camera/fps_calculator.h>

#include <licensing/license.h>

#include <text/time_strings.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/common/checkbox_utils.h>
#include <ui/common/palette.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/custom_style.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/widgets/properties/archive_length_widget.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/event_processors.h>
#include <utils/math/color_transformations.h>
#include <utils/license_usage_helper.h>

using boost::algorithm::all_of;
using boost::algorithm::any_of;

using namespace nx::client::desktop::ui;

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

class QnExportScheduleResourceSelectionDialogDelegate: public QnResourceSelectionDialogDelegate
{
    Q_DECLARE_TR_FUNCTIONS(QnExportScheduleResourceSelectionDialogDelegate);
    typedef QnResourceSelectionDialogDelegate base_type;
public:
    QnExportScheduleResourceSelectionDialogDelegate(QWidget* parent,
        bool recordingEnabled,
        bool motionUsed,
        bool dualStreamingUsed,
        bool hasVideo)
        :
        base_type(parent),
        m_licensesLabel(NULL),
        m_motionLabel(NULL),
        m_dtsLabel(NULL),
        m_recordingEnabled(recordingEnabled),
        m_motionUsed(motionUsed),
        m_dualStreamingUsed(dualStreamingUsed),
        m_hasVideo(hasVideo)
    {
    }

    virtual ~QnExportScheduleResourceSelectionDialogDelegate()
    {

    }

    bool doCopyArchiveLength() const
    {
        return m_archiveCheckbox->isChecked();
    }

    virtual void init(QWidget* parent) override
    {
        m_parentPalette = parent->palette();

        m_archiveCheckbox = new QCheckBox(parent);
        m_archiveCheckbox->setText(tr("Copy archive length settings"));
        parent->layout()->addWidget(m_archiveCheckbox);

        m_licensesLabel = new QLabel(parent);
        parent->layout()->addWidget(m_licensesLabel);

        auto addWarningLabel = [parent](const QString &text) -> QLabel*
        {
            auto label = new QLabel(text, parent);
            setWarningStyle(label);
            parent->layout()->addWidget(label);
            label->setVisible(false);
            return label;
        };

        m_motionLabel = addWarningLabel(tr("Schedule motion type is not supported by some cameras."));
        m_dtsLabel = addWarningLabel(tr("Recording cannot be enabled for some cameras."));
        m_noVideoLabel = addWarningLabel(tr("Schedule settings are not compatible with some devices."));
    }

    virtual bool validate(const QSet<QnUuid>& selected) override
    {
        auto cameras = resourcePool()->getResources<QnVirtualCameraResource>(selected);

        QnCamLicenseUsageHelper helper(cameras, m_recordingEnabled, commonModule());

        QPalette palette = m_parentPalette;
        bool licensesOk = helper.isValid();
        QString licenseUsage = helper.getProposedUsageMsg();
        if (!licensesOk)
        {
            setWarningStyle(&palette);
            licenseUsage += L'\n' + helper.getRequiredMsg();
        }
        m_licensesLabel->setText(licenseUsage);
        m_licensesLabel->setPalette(palette);

        bool motionOk = true;
        if (m_motionUsed)
        {
            foreach(const QnVirtualCameraResourcePtr &camera, cameras)
            {
                bool hasMotion = (camera->getMotionType() != Qn::MT_NoMotion);
                if (!hasMotion)
                {
                    motionOk = false;
                    break;
                }
                if (m_dualStreamingUsed && !camera->hasDualStreaming2())
                {
                    motionOk = false;
                    break;
                }
            }
        }
        m_motionLabel->setVisible(!motionOk);

        bool dtsCamPresent = any_of(cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->isDtsBased(); });
        m_dtsLabel->setVisible(dtsCamPresent);

        /* If source camera has no video, allow to copy only to cameras without video. */
        bool videoOk = m_hasVideo || !any_of(cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasVideo(0); });
        m_noVideoLabel->setVisible(!videoOk);

        return licensesOk && motionOk && !dtsCamPresent && videoOk;
    }

private:
    QLabel* m_licensesLabel;
    QLabel* m_motionLabel;
    QLabel* m_dtsLabel;
    QLabel* m_noVideoLabel;
    QCheckBox *m_archiveCheckbox;
    QPalette m_parentPalette;

    bool m_recordingEnabled;
    bool m_motionUsed;
    bool m_dualStreamingUsed;
    bool m_hasVideo;
};

static const int kRecordingTypeLabelFontSize = 12;
static const int kRecordingTypeLabelFontWeight = QFont::DemiBold;
static const int kDefaultBeforeThresholdSec = 5;
static const int kDefaultAfterThresholdSec = 5;

}

QnCameraScheduleWidget::QnCameraScheduleWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent, true),
    ui(new Ui::CameraScheduleWidget),
    m_disableUpdateGridParams(false),
    m_motionAvailable(true),
    m_recordingParamsAvailable(true),
    m_readOnly(false),
    m_maxFps(0),
    m_maxDualStreamingFps(0),
    m_motionTypeOverride(Qn::MT_Default),
    m_updating(false)
{
    ui->setupUi(this);
    ui->recordBeforeSpinBox->setSuffix(L' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));
    ui->recordAfterSpinBox->setSuffix(L' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));

    NX_ASSERT(parent);
    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(window());
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseMaximumSpace(true);

    ui->enableRecordingCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->enableRecordingCheckBox->setForegroundRole(QPalette::ButtonText);

    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityLow), Qn::QualityLow);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityNormal), Qn::QualityNormal);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityHigh), Qn::QualityHigh);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityHighest), Qn::QualityHighest);
    ui->qualityComboBox->setCurrentIndex(ui->qualityComboBox->findData(Qn::QualityHigh));

    setHelpTopic(ui->exportScheduleButton, Qn::CameraSettings_Recording_Export_Help);

    // init buttons
    connect(ui->gridWidget, &QnScheduleGridWidget::colorsChanged, this,
        &QnCameraScheduleWidget::updateColors);
    updateColors();

    QnCamLicenseUsageHelper helper(commonModule());
    ui->licensesUsageWidget->init(&helper);

    CheckboxUtils::autoClearTristate(ui->enableRecordingCheckBox);

    QFont labelFont;
    labelFont.setPixelSize(kRecordingTypeLabelFontSize);
    labelFont.setWeight(kRecordingTypeLabelFontWeight);

    for (auto label : {ui->labelAlways, ui->labelMotionOnly, ui->labelMotionPlusLQ, ui->labelNoRecord})
    {
        label->setFont(labelFont);
        label->setProperty(style::Properties::kDontPolishFontProperty, true);
    }

    auto notifyAboutScheduleEnabledChanged =
        [this](int state)
        {
            if (m_updating)
                return;

            updateAlert(EnabledChange);
            emit scheduleEnabledChanged(state);
        };

    auto notifyAboutArchiveRangeChanged =
        [this]
        {
            if (!m_updating)
                emit archiveRangeChanged();
        };

    auto handleCellValuesChanged =
        [this]()
        {
            if (m_updating)
                return;

            updateAlert(ScheduleChange);
            emit scheduleTasksChanged();
        };

    connect(ui->gridWidget, &QnScheduleGridWidget::cellValuesChanged,
        this, handleCellValuesChanged);
    connect(ui->recordAlwaysButton, &QToolButton::toggled, this,
        &QnCameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionButton, &QToolButton::toggled, this,
        &QnCameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionPlusLQButton, &QToolButton::toggled, this,
        &QnCameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionPlusLQButton, &QToolButton::toggled, this,
        &QnCameraScheduleWidget::updateMaxFpsValue);
    connect(ui->noRecordButton, &QToolButton::toggled, this,
        &QnCameraScheduleWidget::updateGridParams);
    connect(ui->qualityComboBox, QnComboboxCurrentIndexChanged, this,
        &QnCameraScheduleWidget::updateGridParams);
    connect(ui->fpsSpinBox, QnSpinboxIntValueChanged, this,
        &QnCameraScheduleWidget::updateGridParams);
    connect(this, &QnCameraScheduleWidget::scheduleTasksChanged, this,
        &QnCameraScheduleWidget::updateRecordSpinboxes);

    connect(ui->recordBeforeSpinBox, QnSpinboxIntValueChanged, this,
        &QnCameraScheduleWidget::recordingSettingsChanged);
    connect(ui->recordAfterSpinBox, QnSpinboxIntValueChanged, this,
        &QnCameraScheduleWidget::recordingSettingsChanged);

    connect(ui->licensesButton, &QPushButton::clicked, this,
        &QnCameraScheduleWidget::at_licensesButton_clicked);
    connect(ui->displayQualityCheckBox, &QCheckBox::stateChanged, this,
        &QnCameraScheduleWidget::at_displayQualiteCheckBox_stateChanged);
    connect(ui->displayFpsCheckBox, &QCheckBox::stateChanged, this,
        &QnCameraScheduleWidget::at_displayFpsCheckBox_stateChanged);

    connect(ui->enableRecordingCheckBox, &QCheckBox::stateChanged, this,
        &QnCameraScheduleWidget::updateLicensesLabelText);
    connect(ui->enableRecordingCheckBox, &QCheckBox::stateChanged, this,
        &QnCameraScheduleWidget::updateGridEnabledState);
    connect(ui->enableRecordingCheckBox, &QCheckBox::stateChanged, this,
        notifyAboutScheduleEnabledChanged);

    connect(ui->gridWidget, &QnScheduleGridWidget::cellActivated, this,
        &QnCameraScheduleWidget::at_gridWidget_cellActivated);

    connect(ui->exportScheduleButton, &QPushButton::clicked, this,
        &QnCameraScheduleWidget::at_exportScheduleButton_clicked);

    connect(ui->archiveLengthWidget, &QnArchiveLengthWidget::alertChanged, this,
        &QnCameraScheduleWidget::emitAlert);
    connect(ui->archiveLengthWidget, &QnArchiveLengthWidget::changed, this,
        notifyAboutArchiveRangeChanged);

    ui->exportWarningLabel->setVisible(false);

    installEventHandler({ ui->recordMotionButton, ui->recordMotionPlusLQButton },
        QEvent::MouseButtonRelease, this, &QnCameraScheduleWidget::at_releaseSignalizer_activated);

    installEventHandler(ui->gridWidget, QEvent::MouseButtonRelease,
        this, &QnCameraScheduleWidget::controlsChangesApplied);

    updateMotionButtons();

    auto updateLicensesIfNeeded =
        [this]
        {
            if (!isVisible())
                return;
            updateLicensesLabelText();
        };

    auto camerasUsageWatcher = new QnCamLicenseUsageWatcher(commonModule(), this);
    connect(camerasUsageWatcher, &QnLicenseUsageWatcher::licenseUsageChanged, this,
        updateLicensesIfNeeded);

    retranslateUi();
}

QnCameraScheduleWidget::~QnCameraScheduleWidget()
{
}

void QnCameraScheduleWidget::overrideMotionType(Qn::MotionType motionTypeOverride)
{
    if (m_motionTypeOverride == motionTypeOverride)
        return;

    m_motionTypeOverride = motionTypeOverride;

    updateMotionAvailable();
    updateMaxFPS();
}

void QnCameraScheduleWidget::retranslateUi()
{
    ui->retranslateUi(this);

    ui->scheduleGridGroupBox->setTitle(lit("%1\t(%2)").arg(
        tr("Recording Schedule")).arg(
        tr("based on server time")));
}

void QnCameraScheduleWidget::afterContextInitialized()
{
    connect(context()->instance<QnWorkbenchPanicWatcher>(), &QnWorkbenchPanicWatcher::panicModeChanged, this, &QnCameraScheduleWidget::updatePanicLabelText);
    connect(context(), &QnWorkbenchContext::userChanged, this, &QnCameraScheduleWidget::updateLicensesButtonVisible);
    updatePanicLabelText();
    updateLicensesButtonVisible();
}

bool QnCameraScheduleWidget::isReadOnly() const
{
    return m_readOnly;
}

void QnCameraScheduleWidget::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->recordAlwaysButton, readOnly);
    setReadOnly(ui->recordMotionButton, readOnly); // TODO: #GDM #Common this is not valid. Camera may not support HW motion, we need to check for this.
    setReadOnly(ui->recordMotionPlusLQButton, readOnly);
    setReadOnly(ui->noRecordButton, readOnly);
    setReadOnly(ui->qualityComboBox, readOnly);
    setReadOnly(ui->fpsSpinBox, readOnly);
    setReadOnly(ui->enableRecordingCheckBox, readOnly);
    setReadOnly(ui->gridWidget, readOnly);
    setReadOnly(ui->exportScheduleButton, readOnly);
    setReadOnly(ui->exportWarningLabel, readOnly);
    setReadOnly(ui->archiveLengthWidget, readOnly);

    m_readOnly = readOnly;
}

const QnVirtualCameraResourceList &QnCameraScheduleWidget::cameras() const
{
    return m_cameras;
}

void QnCameraScheduleWidget::setCameras(const QnVirtualCameraResourceList &cameras)
{
    if (m_cameras == cameras)
        return;

    for (const auto& camera : m_cameras)
        disconnect(camera, &QnSecurityCamResource::resourceChanged, this, &QnCameraScheduleWidget::updateMotionButtons);

    m_cameras = cameras;

    for (const auto& camera : m_cameras)
        connect(camera, &QnSecurityCamResource::resourceChanged, this, &QnCameraScheduleWidget::updateMotionButtons, Qt::QueuedConnection);
}

void QnCameraScheduleWidget::updateFromResources()
{
    QScopedValueRollback<bool> updateRollback(m_updating, true);

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
        QList<QnScheduleTask::Data> scheduleTasksData;
        for (const auto& scheduleTask : m_cameras.front()->getScheduleTasks())
            scheduleTasksData << scheduleTask.getData();

        for (const auto &camera : m_cameras)
        {
            QList<QnScheduleTask::Data> cameraScheduleTasksData;
            for (const auto& scheduleTask : camera->getScheduleTasks())
            {
                cameraScheduleTasksData << scheduleTask.getData();

                bool fpsValid = true;
                switch (scheduleTask.getRecordingType())
                {
                    case Qn::RT_Never:
                        continue;
                    case Qn::RT_MotionAndLowQuality:
                        fpsValid = scheduleTask.getFps() <= m_maxDualStreamingFps;
                        break;
                    case Qn::RT_Always:
                    case Qn::RT_MotionOnly:
                        fpsValid = scheduleTask.getFps() <= m_maxFps;
                        break;
                    default:
                        break;
                }

                if (!fpsValid)
                {
                    isScheduleEqual = false;
                    break;
                }
            }

            if (!isScheduleEqual || cameraScheduleTasksData != scheduleTasksData)
            {
                isScheduleEqual = false;
                break;
            }
        }

        if (isScheduleEqual)
            setScheduleTasks(m_cameras.front()->getScheduleTasks());
        else
            setScheduleTasks(QnScheduleTaskList());
        ui->gridWidget->setActive(isScheduleEqual);
    }

    int currentMaxFps = getGridMaxFps();
    if (currentMaxFps > 0)
        setFps(currentMaxFps);
    else if (!m_cameras.isEmpty())
        setFps(m_cameras.first()->getMaxFps() / 2);

    ui->archiveLengthWidget->updateFromResources(m_cameras);

    updateScheduleEnabled();
    updatePanicLabelText();
    updateMotionButtons();
    updateLicensesLabelText();
    updateGridParams();
    setScheduleAlert(QString());

    retranslateUi();
}

void QnCameraScheduleWidget::submitToResources()
{
    bool scheduleChanged = ui->gridWidget->isActive();

    ui->archiveLengthWidget->submitToResources(m_cameras);

    QnScheduleTaskList basicScheduleTasks;
    if (scheduleChanged)
    {
        for (const auto& scheduleTaskData : this->scheduleTasks())
            basicScheduleTasks.append(scheduleTaskData);
    }
    updateRecordThresholds(basicScheduleTasks);

    bool enableRecording = isScheduleEnabled();
    bool canChangeRecording = !enableRecording || canEnableRecording();

    for (const auto& camera: m_cameras)
    {
        if (canChangeRecording)
        {
            camera->setLicenseUsed(enableRecording);
            camera->setScheduleDisabled(!enableRecording);
        }

        if (camera->isDtsBased())
            continue;

        QnScheduleTaskList scheduleTasks = basicScheduleTasks;
        if (!scheduleChanged)
        {
            scheduleTasks = camera->getScheduleTasks();
            updateRecordThresholds(scheduleTasks);
        }
        camera->setScheduleTasks(scheduleTasks);
    }

    if (!canChangeRecording)
    {
        QSignalBlocker blocker(ui->enableRecordingCheckBox);
        updateScheduleEnabled();
        setScheduleAlert(QString());
        updateLicensesLabelText();
    }
}

void QnCameraScheduleWidget::updateScheduleEnabled()
{
    int enabledCount = 0, disabledCount = 0;
    for (const auto &camera : m_cameras)
        (camera->isScheduleDisabled() ? disabledCount : enabledCount)++;

    CheckboxUtils::setupTristateCheckbox(ui->enableRecordingCheckBox,
        enabledCount == 0 || disabledCount == 0, enabledCount > 0);
}

void QnCameraScheduleWidget::updateMaxFPS()
{
    QPair<int, int> fpsLimits = Qn::calculateMaxFps(m_cameras, m_motionTypeOverride);
    int maxFps = fpsLimits.first;
    int maxDualStreamingFps = fpsLimits.second;

    /* Silently ignoring invalid input is OK here. */
    if (maxFps < ui->fpsSpinBox->minimum())
        maxFps = ui->fpsSpinBox->minimum();
    if (maxDualStreamingFps < ui->fpsSpinBox->minimum())
        maxDualStreamingFps = ui->fpsSpinBox->minimum();

    if (m_maxFps != maxFps)
    {
        m_maxFps = maxFps;
        int currentMaxFps = getGridMaxFps();
        if (currentMaxFps > m_maxFps)
            emit scheduleTasksChanged();
    }

    if (m_maxDualStreamingFps != maxDualStreamingFps)
    {
        m_maxDualStreamingFps = maxDualStreamingFps;
        int currentMaxDualStreamingFps = getGridMaxFps(true);
        if (currentMaxDualStreamingFps > m_maxDualStreamingFps)
            emit scheduleTasksChanged();
    }

    updateMaxFpsValue(ui->recordMotionPlusLQButton->isChecked());
    ui->gridWidget->setMaxFps(m_maxFps, m_maxDualStreamingFps);
}

void QnCameraScheduleWidget::updateMotionAvailable()
{
    using boost::algorithm::all_of;

    bool available = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return !camera->isDtsBased(); });
    if (m_motionTypeOverride != Qn::MT_Default)
    {
        available &= m_motionTypeOverride != Qn::MT_NoMotion;
    }
    else
    {
        available &= all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasMotion(); });
    }

    if (m_motionAvailable == available)
        return;

    m_motionAvailable = available;

    updateMotionButtons();
}

void QnCameraScheduleWidget::setExportScheduleButtonEnabled(bool enabled)
{
    ui->exportScheduleButton->setEnabled(enabled);
    ui->exportWarningLabel->setVisible(!enabled);
}

void QnCameraScheduleWidget::updatePanicLabelText()
{
    ui->panicModeLabel->setText(tr("Off"));
    ui->panicModeLabel->setPalette(this->palette());

    if (!context())
        return;

    if (context()->instance<QnWorkbenchPanicWatcher>()->isPanicMode())
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::WindowText, QColor(255, 0, 0));
        ui->panicModeLabel->setPalette(palette);
        ui->panicModeLabel->setText(tr("On"));
    }
}

QnScheduleTaskList QnCameraScheduleWidget::scheduleTasks() const
{
    QnScheduleTaskList tasks;
    if (!ui->gridWidget->isActive())
        return tasks;

    for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
    {
        QnScheduleTask::Data task;

        for (int col = 0; col < ui->gridWidget->columnCount();)
        {
            const QPoint cell(col, row);
            const QnScheduleGridWidget::CellParams params = ui->gridWidget->cellValue(cell);

            Qn::RecordingType recordType = params.recordingType;
            Qn::StreamQuality streamQuality = Qn::QualityHighest;
            if (recordType != Qn::RT_Never)
                streamQuality = params.quality;
            int fps = params.fps;
            if (fps == 0 && recordType != Qn::RT_Never)
                fps = 10;

            if (task.m_startTime == task.m_endTime)
            {
                // an invalid one; initialize
                task.m_dayOfWeek = row + 1;
                task.m_startTime = col * 3600; // in secs from start of day
                task.m_endTime = (col + 1) * 3600; // in secs from start of day
                task.m_recordType = recordType;
                task.m_streamQuality = streamQuality;
                task.m_fps = fps;
                ++col;
            }
            else if (task.m_recordType == recordType
                && task.m_streamQuality == streamQuality
                && task.m_fps == fps)
            {
                task.m_endTime = (col + 1) * 3600; // in secs from start of day
                ++col;
            }
            else
            {
                tasks.append(task);
                task = QnScheduleTask::Data();
            }
        }

        if (task.m_startTime != task.m_endTime)
            tasks.append(task);
    }

    return tasks;
}

void QnCameraScheduleWidget::setScheduleTasks(const QnScheduleTaskList& value)
{
    QSignalBlocker gridBlocker(ui->gridWidget);

    QnScheduleTaskList tasks = value;

    ui->gridWidget->resetCellValues();
    if (tasks.isEmpty())
    {
        for (int nDay = 1; nDay <= 7; ++nDay)
            tasks.append(QnScheduleTask::Data(nDay, 0, 86400, Qn::RT_Never,
                kDefaultBeforeThresholdSec, kDefaultAfterThresholdSec));
    }

    auto task = tasks.first();
    ui->recordBeforeSpinBox->setValue(task.getBeforeThreshold());
    ui->recordAfterSpinBox->setValue(task.getAfterThreshold());

    for (const auto &task : tasks)
    {
        const int row = task.getDayOfWeek() - 1;
        Qn::StreamQuality quality = Qn::QualityNotDefined;
        if (task.getRecordingType() != Qn::RT_Never)
        {
            switch (task.getStreamQuality())
            {
                case Qn::QualityLow:
                case Qn::QualityNormal:
                case Qn::QualityHigh:
                case Qn::QualityHighest:
                    quality = task.getStreamQuality();
                    break;
                default:
                    qWarning("QnCameraScheduleWidget::setScheduleTasks(): Unhandled StreamQuality value %d.", task.getStreamQuality());
                    break;
            }
        }

        for (int col = task.getStartTime() / 3600; col < task.getEndTime() / 3600; ++col)
        {
            const QPoint cell(col, row);
            QnScheduleGridWidget::CellParams params;
            params.recordingType = task.getRecordingType();
            params.quality = quality;
            params.fps = task.getFps();
            ui->gridWidget->setCellValue(cell, params);
        }
    }

    if (!m_updating)
        emit scheduleTasksChanged();
}

bool QnCameraScheduleWidget::canEnableRecording() const
{
    QnCamLicenseUsageHelper licenseHelper(m_cameras, true, commonModule());
    return all_of(m_cameras,
        [&licenseHelper](const QnVirtualCameraResourcePtr& camera)
        {
            return licenseHelper.isValid(camera->licenseType());
        });
}

void QnCameraScheduleWidget::updateRecordThresholds(QnScheduleTaskList& tasks)
{
    int before = ui->recordBeforeSpinBox->value();
    int after = ui->recordAfterSpinBox->value();
    for (auto& task : tasks)
    {
        task.setBeforeThreshold(before);
        task.setAfterThreshold(after);
    }
}

int QnCameraScheduleWidget::qualityToComboIndex(const Qn::StreamQuality& q)
{
    for (int i = 0; i < ui->qualityComboBox->count(); ++i)
    {
        if (ui->qualityComboBox->itemData(i).toInt() == q)
            return i;
    }
    return 0;
}

void QnCameraScheduleWidget::updateScheduleTypeControls()
{
    const bool recordingEnabled = ui->enableRecordingCheckBox->isChecked();
    const auto labels =
        { ui->labelAlways, ui->labelMotionOnly, ui->labelMotionPlusLQ, ui->labelNoRecord };
    for (auto label: labels)
    {
        const auto button = qobject_cast<QAbstractButton*>(label->buddy());
        const QPalette::ColorRole foreground = button && button->isChecked() && recordingEnabled
            ? QPalette::Highlight
            : QPalette::WindowText;
        label->setForegroundRole(foreground);
    }
}

void QnCameraScheduleWidget::updateGridParams(bool pickedFromGrid)
{
    if (m_disableUpdateGridParams)
        return;

    Qn::RecordingType recordType = Qn::RT_Never;
    if (ui->recordAlwaysButton->isChecked())
        recordType = Qn::RT_Always;
    else if (ui->recordMotionButton->isChecked())
        recordType = Qn::RT_MotionOnly;
    else if (ui->noRecordButton->isChecked())
        recordType = Qn::RT_Never;
    else if (ui->recordMotionPlusLQButton->isChecked())
        recordType = Qn::RT_MotionAndLowQuality;
    else
        qWarning() << "QnCameraScheduleWidget::No record type is selected!";

    updateScheduleTypeControls();
    bool enabled = !ui->noRecordButton->isChecked();
    ui->fpsSpinBox->setEnabled(enabled && m_recordingParamsAvailable);
    ui->qualityComboBox->setEnabled(enabled && m_recordingParamsAvailable);
    updateRecordSpinboxes();

    if (!(m_readOnly && pickedFromGrid))
    {
        QnScheduleGridWidget::CellParams brush;
        brush.recordingType = recordType;

        if (ui->noRecordButton->isChecked())
        {
            brush.fps = 0;
            brush.quality = Qn::QualityNotDefined;
        }
        else
        {
            brush.fps = ui->fpsSpinBox->value();
            int data = ui->qualityComboBox->itemData(ui->qualityComboBox->currentIndex()).toInt();
            brush.quality = static_cast<Qn::StreamQuality>(data);
        }

        ui->gridWidget->setBrush(brush);
    }

    // TODO: #GDM is it really needed here?
    updateMaxFPS();

    if (m_updating)
        return;

    updateAlert(CurrentParamsChange);
    emit gridParamsChanged();
}

void QnCameraScheduleWidget::setFps(int value)
{
    ui->fpsSpinBox->setValue(value);
}

int QnCameraScheduleWidget::getGridMaxFps(bool motionPlusLqOnly)
{
    return ui->gridWidget->getMaxFps(motionPlusLqOnly);
}

void QnCameraScheduleWidget::setScheduleEnabled(bool enabled)
{
    ui->enableRecordingCheckBox->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
}

bool QnCameraScheduleWidget::isScheduleEnabled() const
{
    return ui->enableRecordingCheckBox->checkState() != Qt::Unchecked;
}

void QnCameraScheduleWidget::updateGridEnabledState()
{
    const bool recordingEnabled = ui->enableRecordingCheckBox->isChecked();
    ui->motionGroupBox->setEnabled(m_recordingParamsAvailable);
    setLayoutEnabled(ui->recordingScheduleLayout, recordingEnabled);
    setLayoutEnabled(ui->scheduleSettingsLayout, recordingEnabled);
    setLayoutEnabled(ui->bottomParametersLayout, recordingEnabled);
    updateScheduleTypeControls();
}

void QnCameraScheduleWidget::updateLicensesLabelText()
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

void QnCameraScheduleWidget::updateLicensesButtonVisible()
{
    ui->licensesButton->setVisible(accessController()->hasGlobalPermission(Qn::GlobalAdminPermission));
}

void QnCameraScheduleWidget::updateRecordSpinboxes()
{
    ui->recordBeforeSpinBox->setEnabled(m_motionAvailable);
    ui->recordAfterSpinBox->setEnabled(m_motionAvailable);
}

void QnCameraScheduleWidget::updateMotionButtons()
{
    bool hasDualStreaming = !m_cameras.isEmpty();
    bool hasMotion = !m_cameras.isEmpty();
    for (const auto &camera: m_cameras)
    {
        hasDualStreaming &= camera->hasDualStreaming2();
        hasMotion &= camera->hasMotion();
    }

    bool enabled;

    enabled = m_motionAvailable && hasMotion;
    ui->recordMotionButton->setEnabled(enabled);
    ui->labelMotionOnly->setEnabled(enabled);

    enabled = m_motionAvailable && hasDualStreaming && hasMotion;
    ui->recordMotionPlusLQButton->setEnabled(enabled);
    ui->labelMotionPlusLQ->setEnabled(enabled);

    if (ui->recordMotionButton->isChecked() && !ui->recordMotionButton->isEnabled())
        ui->recordAlwaysButton->setChecked(true);
    if (ui->recordMotionPlusLQButton->isChecked() && !ui->recordMotionPlusLQButton->isEnabled())
        ui->recordAlwaysButton->setChecked(true);

    if (!m_motionAvailable)
    {
        for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
        {
            for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
            {
                const QPoint cell(col, row);
                auto params = ui->gridWidget->cellValue(cell);
                Qn::RecordingType recordType = params.recordingType;
                if (recordType == Qn::RT_MotionOnly || recordType == Qn::RT_MotionAndLowQuality)
                {
                    params.recordingType = Qn::RT_Always;
                    ui->gridWidget->setCellValue(cell, params);
                }
            }
        }
    }
}

void QnCameraScheduleWidget::updateMaxFpsValue(bool motionPlusLqToggled)
{
    int maximum = motionPlusLqToggled ? m_maxDualStreamingFps : m_maxFps;

    /* This check is necessary because Qt doesn't do it and resets spinbox state: */
    if (maximum == ui->fpsSpinBox->maximum())
        return;

    ui->fpsSpinBox->setMaximum(maximum);
}

void QnCameraScheduleWidget::updateRecordingParamsAvailable()
{
    bool available = any_of(m_cameras,
        [](const QnVirtualCameraResourcePtr& camera)
        {
            return camera->hasVideo(0)
                && !camera->hasParam(Qn::NO_RECORDING_PARAMS_PARAM_NAME);
        });

    if (m_recordingParamsAvailable == available)
        return;

    m_recordingParamsAvailable = available;

    updateGridEnabledState();

    ui->gridWidget->setShowQuality(ui->displayQualityCheckBox->isChecked() && m_recordingParamsAvailable);
    ui->gridWidget->setShowFps(ui->displayFpsCheckBox->isChecked() && m_recordingParamsAvailable);

    ui->fpsSpinBox->setEnabled(m_recordingParamsAvailable);
    ui->qualityComboBox->setEnabled(m_recordingParamsAvailable);
    ui->displayQualityCheckBox->setEnabled(m_recordingParamsAvailable);
    ui->displayFpsCheckBox->setEnabled(m_recordingParamsAvailable);
}

void QnCameraScheduleWidget::updateColors()
{
    ui->recordAlwaysButton->setCustomPaintFunction(ui->gridWidget->paintFunction(Qn::RT_Always));
    ui->recordMotionButton->setCustomPaintFunction(ui->gridWidget->paintFunction(Qn::RT_MotionOnly));
    ui->recordMotionPlusLQButton->setCustomPaintFunction(ui->gridWidget->paintFunction(Qn::RT_MotionAndLowQuality));
    ui->noRecordButton->setCustomPaintFunction(ui->gridWidget->paintFunction(Qn::RT_Never));
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCameraScheduleWidget::at_gridWidget_cellActivated(const QPoint &cell)
{
    /* Called when a cell is Alt-clicked this handler fetches cell settings as current. */

    m_disableUpdateGridParams = true;
    const auto params = ui->gridWidget->cellValue(cell);
    switch (params.recordingType)
    {
        case Qn::RT_Always:
            ui->recordAlwaysButton->setChecked(true);
            break;
        case Qn::RT_MotionOnly:
            ui->recordMotionButton->setChecked(true);
            break;
        case Qn::RT_MotionAndLowQuality:
            ui->recordMotionPlusLQButton->setChecked(true);
            break;
        default:
            ui->noRecordButton->setChecked(true);
            break;
    }

    if (params.recordingType != Qn::RT_Never)
    {
        ui->fpsSpinBox->setValue(params.fps);
        ui->qualityComboBox->setCurrentIndex(qualityToComboIndex(params.quality));
    }

    m_disableUpdateGridParams = false;
    updateGridParams(true);
}

void QnCameraScheduleWidget::at_displayQualiteCheckBox_stateChanged(int state)
{
    ui->gridWidget->setShowQuality(state && m_recordingParamsAvailable);
}

void QnCameraScheduleWidget::at_displayFpsCheckBox_stateChanged(int state)
{
    ui->gridWidget->setShowFps(state && m_recordingParamsAvailable);
}

void QnCameraScheduleWidget::at_licensesButton_clicked()
{
    menu()->trigger(action::PreferencesLicensesTabAction);
}

void QnCameraScheduleWidget::at_releaseSignalizer_activated(QObject *target)
{
    QWidget *widget = qobject_cast<QWidget *>(target);
    if (!widget)
        return;

    if (m_cameras.isEmpty() || widget->isEnabled() || (widget->parentWidget() && !widget->parentWidget()->isEnabled()))
        return;

    using boost::algorithm::all_of;

    if (m_cameras.size() > 1)
    {
        QnMessageBox::warning(this,
            tr("Motion detection disabled or not supported"),
            tr("To ensure it is supported and to enable it, go to the \"Motion\" tab in Camera Settings."));
    }
    else /* One camera */
    {
        NX_ASSERT(m_cameras.size() == 1, Q_FUNC_INFO, "Following options are valid only for singular camera");
        QnVirtualCameraResourcePtr camera = m_cameras.first();

        // TODO: #GDM #Common duplicate code.
        bool hasDualStreaming = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasDualStreaming2(); });
        bool hasMotion = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasMotion(); });

        if (hasMotion && !hasDualStreaming)
        {
            QnMessageBox::warning(this, tr("Dual-Streaming not supported for this camera"));
        }
        else if (!hasMotion && !hasDualStreaming)
        {
            QnMessageBox::warning(this, tr("Dual-Streaming and motion detection not supported for this camera"));
        }
        else /* Has dual streaming but not motion */
        {
            QnMessageBox::warning(this,
                tr("Motion detection disabled"),
                tr("To enable or adjust it, go to the \"Motion\" tab in Camera Settings."));
        }
    }
}

void QnCameraScheduleWidget::at_exportScheduleButton_clicked()
{
    bool recordingEnabled = ui->enableRecordingCheckBox->checkState() == Qt::Checked;
    bool motionUsed = recordingEnabled && hasMotionOnGrid();
    bool dualStreamingUsed = motionUsed && hasDualStreamingMotionOnGrid();
    bool hasVideo = boost::algorithm::all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasVideo(0); });

    QScopedPointer<QnResourceSelectionDialog> dialog(
        new QnResourceSelectionDialog(QnResourceSelectionDialog::Filter::cameras, this));
    auto dialogDelegate = new QnExportScheduleResourceSelectionDialogDelegate(this, recordingEnabled, motionUsed, dualStreamingUsed, hasVideo);
    dialog->setDelegate(dialogDelegate);

    QSet<QnUuid> ids;
    for (auto camera: m_cameras)
        ids << camera->getId();
    dialog->setSelectedResources(ids);
    setHelpTopic(dialog.data(), Qn::CameraSettings_Recording_Export_Help);
    if (!dialog->exec())
        return;

    const bool copyArchiveLength = dialogDelegate->doCopyArchiveLength();

    auto applyChanges = [this, copyArchiveLength, recordingEnabled](const QnVirtualCameraResourcePtr &camera)
    {
        if (copyArchiveLength)
            ui->archiveLengthWidget->submitToResources({camera});

        camera->setScheduleDisabled(!recordingEnabled);
        int maxFps = camera->getMaxFps();

        // TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
        // or just use camera->reservedSecondStreamFps();

        int decreaseAlways = 0;
        if (camera->streamFpsSharingMethod() == Qn::BasicFpsSharing && camera->getMotionType() == Qn::MT_SoftwareGrid)
            decreaseAlways = MIN_SECOND_STREAM_FPS;

        int decreaseIfMotionPlusLQ = 0;
        if (camera->streamFpsSharingMethod() == Qn::BasicFpsSharing)
            decreaseIfMotionPlusLQ = MIN_SECOND_STREAM_FPS;

        QnScheduleTaskList tasks;
        for (auto task: scheduleTasks())
        {
            if (task.getRecordingType() == Qn::RT_MotionAndLowQuality)
                task.setFps(qMin(task.getFps(), maxFps - decreaseIfMotionPlusLQ));
            else
                task.setFps(qMin(task.getFps(), maxFps - decreaseAlways));
            tasks.append(task);
        }
        updateRecordThresholds(tasks);

        camera->setScheduleTasks(tasks);
    };

    auto selectedCameras = resourcePool()->getResources<QnVirtualCameraResource>(
        dialog->selectedResources());
    qnResourcesChangesManager->saveCameras(selectedCameras, applyChanges);
    updateLicensesLabelText();
}

bool QnCameraScheduleWidget::hasMotionOnGrid() const
{
    for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
    {
        for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
        {
            const QPoint cell(col, row);
            Qn::RecordingType recordType = ui->gridWidget->cellValue(cell).recordingType;
            if (recordType == Qn::RT_MotionOnly || recordType == Qn::RT_MotionAndLowQuality)
                return true;
        }
    }
    return false;
}

bool QnCameraScheduleWidget::hasDualStreamingMotionOnGrid() const
{
    for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
    {
        for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
        {
            const QPoint cell(col, row);
            if (ui->gridWidget->cellValue(cell).recordingType == Qn::RT_MotionAndLowQuality)
                return true;
        }
    }
    return false;
}

void QnCameraScheduleWidget::setScheduleAlert(const QString& scheduleAlert)
{
    /* We want to force update - emit a signal - even if the text didn't change: */
    m_scheduleAlert = scheduleAlert;
    emitAlert();
}

void QnCameraScheduleWidget::emitAlert()
{
    if (!m_scheduleAlert.isEmpty())
        emit alert(m_scheduleAlert);
    else
        emit alert(ui->archiveLengthWidget->alert());
}

void QnCameraScheduleWidget::updateAlert(AlertReason when)
{
    /* License check and alert: */
    const auto checkCanEnableRecording =
        [this]() -> bool
        {
            if (canEnableRecording())
                return true;

            switch (ui->enableRecordingCheckBox->checkState())
            {
                case Qt::Unchecked:
                case Qt::PartiallyChecked:
                    setScheduleAlert(tr("Not enough licenses to enable recording"));
                    break;

                case Qt::Checked:
                    setScheduleAlert(tr("License limit exceeded, recording will not be enabled."));
                    break;

                default:
                    break;
            }

            return false;
        };

    switch (when)
    {
        /* Current "brush" was changed (mode, fps, quality): */
        case CurrentParamsChange:
        {
            if (checkCanEnableRecording() && !isRecordingScheduled())
                setScheduleAlert(tr("Select areas on the schedule to apply chosen parameters to."));
            break;
        }

        /* Some cell(s) in the schedule were changed: */
        case ScheduleChange:
        {
            if (isRecordingScheduled())
            {
                if (!checkCanEnableRecording())
                    break;

                if (!isScheduleEnabled())
                {
                    setScheduleAlert(tr("Turn on selector at the top of the window to enable recording."));
                    break;
                }
            }

            setScheduleAlert(QString());
            break;
        }

        /* Recording was enabled or disabled: */
        case EnabledChange:
        {
            if (isScheduleEnabled())
            {
                if (!checkCanEnableRecording())
                    break;

                if (!isRecordingScheduled())
                {
                    setScheduleAlert(tr("Set recording parameters and select areas on the schedule grid to apply them to."));
                    break;
                }
            }

            setScheduleAlert(QString());
            break;
        }
    }
}

bool QnCameraScheduleWidget::isRecordingScheduled() const
{
    return any_of(scheduleTasks(),
        [](const QnScheduleTask& task) -> bool
        {
            return !task.isEmpty() && task.getRecordingType() != Qn::RT_Never;
        });
}
