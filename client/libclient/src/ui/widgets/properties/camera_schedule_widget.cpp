#include "camera_schedule_widget.h"
#include "ui_camera_schedule_widget.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QScopedValueRollback>

//TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <nx_ec/data/api_camera_attributes_data.h>

#include <camera/fps_calculator.h>

#include <licensing/license.h>

#include <ui/actions/action_manager.h>
#include <ui/common/palette.h>
#include <ui/common/read_only.h>
#include <ui/common/checkbox_utils.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/custom_style.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/event_processors.h>
#include <utils/math/color_transformations.h>
#include <utils/license_usage_helper.h>

using boost::algorithm::all_of;
using boost::algorithm::any_of;

namespace {

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
        auto cameras = qnResPool->getResources<QnVirtualCameraResource>(selected);

        QnCamLicenseUsageHelper helper(cameras, m_recordingEnabled);

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

static const int kDangerousMinArchiveDays = 5;
static const int kRecordingTypeLabelFontSize = 12;
static const int kRecordingTypeLabelFontWeight = QFont::DemiBold;
static const int kDefaultBeforeThresholdSec = 5;
static const int kDefaultAfterThresholdSec = 5;
static const int kRecordedDaysDontChange = std::numeric_limits<int>::max();

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

    NX_ASSERT(parent);
    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(parent ? parent : this);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseMaximumSpace(true);

    ui->enableRecordingCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);

    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityLow), Qn::QualityLow);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityNormal), Qn::QualityNormal);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityHigh), Qn::QualityHigh);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityHighest), Qn::QualityHighest);
    ui->qualityComboBox->setCurrentIndex(ui->qualityComboBox->findData(Qn::QualityHigh));

    setHelpTopic(ui->archiveGroupBox, Qn::CameraSettings_Recording_ArchiveLength_Help);
    setHelpTopic(ui->exportScheduleButton, Qn::CameraSettings_Recording_Export_Help);

    // init buttons
    connect(ui->gridWidget, &QnScheduleGridWidget::colorsChanged, this,
        &QnCameraScheduleWidget::updateColors);
    updateColors();

    QnCamLicenseUsageHelper helper;
    ui->licensesUsageWidget->init(&helper);

    QnCheckbox::autoCleanTristate(ui->enableRecordingCheckBox);
    QnCheckbox::autoCleanTristate(ui->checkBoxMinArchive);
    QnCheckbox::autoCleanTristate(ui->checkBoxMaxArchive);

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

            switch (state)
            {
                case Qt::Checked:
                    checkScheduleParamsSet();
                    break;
                case Qt::Unchecked:
                    setScheduleAlert(QString());
                    break;
                default:
                    break;
            }

            emit scheduleEnabledChanged(state);
        };

    auto notifyAboutArchiveRangeChanged =
        [this]
        {
            if (!m_updating)
                emit archiveRangeChanged();
        };

    auto handleCellValueChanged =
        [this](const QPoint& cell)
        {
            if (m_updating)
                return;

            if (ui->gridWidget->cellValue(cell).recordingType != Qn::RT_Never)
                checkRecordingEnabled();

            emit scheduleTasksChanged();
        };

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

    connect(ui->gridWidget, &QnScheduleGridWidget::cellValueChanged,
        this, handleCellValueChanged);

    connect(ui->checkBoxMinArchive, &QCheckBox::stateChanged, this,
        &QnCameraScheduleWidget::updateArchiveRangeEnabledState);
    connect(ui->checkBoxMinArchive, &QCheckBox::stateChanged, this,
        notifyAboutArchiveRangeChanged);

    connect(ui->checkBoxMaxArchive, &QCheckBox::stateChanged, this,
        &QnCameraScheduleWidget::updateArchiveRangeEnabledState);
    connect(ui->checkBoxMaxArchive, &QCheckBox::stateChanged, this,
        notifyAboutArchiveRangeChanged);

    connect(ui->spinBoxMinDays, QnSpinboxIntValueChanged, this,
        &QnCameraScheduleWidget::validateArchiveLength);
    connect(ui->spinBoxMinDays, QnSpinboxIntValueChanged, this,
        notifyAboutArchiveRangeChanged);

    connect(ui->spinBoxMaxDays, QnSpinboxIntValueChanged, this,
        &QnCameraScheduleWidget::validateArchiveLength);
    connect(ui->spinBoxMaxDays, QnSpinboxIntValueChanged, this,
        notifyAboutArchiveRangeChanged);

    connect(ui->exportScheduleButton, &QPushButton::clicked, this,
        &QnCameraScheduleWidget::at_exportScheduleButton_clicked);

    ui->exportWarningLabel->setVisible(false);

    auto releaseSignalizer = new QnSingleEventSignalizer(this);
    releaseSignalizer->setEventType(QEvent::MouseButtonRelease);
    connect(releaseSignalizer, &QnSingleEventSignalizer::activated, this,
        &QnCameraScheduleWidget::at_releaseSignalizer_activated);
    ui->recordMotionButton->installEventFilter(releaseSignalizer);
    ui->recordMotionPlusLQButton->installEventFilter(releaseSignalizer);

    auto gridMouseReleaseSignalizer = new QnSingleEventSignalizer(this);
    gridMouseReleaseSignalizer->setEventType(QEvent::MouseButtonRelease);
    connect(gridMouseReleaseSignalizer, &QnSingleEventSignalizer::activated, this,
        &QnCameraScheduleWidget::controlsChangesApplied);
    ui->gridWidget->installEventFilter(gridMouseReleaseSignalizer);

    updateGridEnabledState();
    updateMotionButtons();

    auto updateLicensesIfNeeded = [this]
        {
            if (!isVisible())
                return;
            updateLicensesLabelText();
        };

    QnCamLicenseUsageWatcher* camerasUsageWatcher = new QnCamLicenseUsageWatcher(this);
    connect(camerasUsageWatcher, &QnLicenseUsageWatcher::licenseUsageChanged, this, updateLicensesIfNeeded);

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

    setReadOnly(ui->checkBoxMaxArchive, readOnly);
    setReadOnly(ui->checkBoxMinArchive, readOnly);
    setReadOnly(ui->spinBoxMaxDays, readOnly);
    setReadOnly(ui->spinBoxMinDays, readOnly);
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

    updateScheduleEnabled();
    updateMinDays();
    updateMaxDays();
    updatePanicLabelText();
    updateMotionButtons();
    updateLicensesLabelText();
    updateGridParams();
    retranslateUi();
}

void QnCameraScheduleWidget::submitToResources()
{
    int maxDays = maxRecordedDays();
    int minDays = minRecordedDays();

    bool scheduleChanged = ui->gridWidget->isActive();

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
        if (maxDays != kRecordedDaysDontChange)
            camera->setMaxDays(maxDays);

        if (minDays != kRecordedDaysDontChange)
            camera->setMinDays(minDays);

        if (camera->isDtsBased())
            continue;

        QnScheduleTaskList scheduleTasks = basicScheduleTasks;
        if (!scheduleChanged)
        {
            scheduleTasks = camera->getScheduleTasks();
            updateRecordThresholds(scheduleTasks);
        }
        camera->setScheduleTasks(scheduleTasks);

        if (canChangeRecording)
        {
            camera->setLicenseUsed(enableRecording);
            camera->setScheduleDisabled(!enableRecording);
        }
    }

    if (!canChangeRecording)
    {
        QSignalBlocker blocker(ui->enableRecordingCheckBox);
        updateScheduleEnabled();
    }
}

void QnCameraScheduleWidget::updateScheduleEnabled()
{
    int enabledCount = 0, disabledCount = 0;
    for (const auto &camera : m_cameras)
        (camera->isScheduleDisabled() ? disabledCount : enabledCount)++;

    QnCheckbox::setupTristateCheckbox(ui->enableRecordingCheckBox,
        enabledCount == 0 || disabledCount == 0, enabledCount > 0);
}

void QnCameraScheduleWidget::updateMinDays()
{
    auto calcMinDays = [](int d) { return d == 0 ? ec2::kDefaultMinArchiveDays : qAbs(d); };

    const int minDays = m_cameras.isEmpty()
        ? 0
        : (*std::min_element(m_cameras.cbegin(), m_cameras.cend(),
            [calcMinDays](const QnVirtualCameraResourcePtr &l, const QnVirtualCameraResourcePtr &r)
            {
                return calcMinDays(l->minDays()) < calcMinDays(r->minDays());
            }))->minDays();

    bool sameMinDays = boost::algorithm::all_of(m_cameras,
        [minDays](const QnVirtualCameraResourcePtr &camera)
        {
            return camera->minDays() == minDays;
        });

    QnCheckbox::setupTristateCheckbox(ui->checkBoxMinArchive, sameMinDays, minDays <= 0);
    ui->spinBoxMinDays->setValue(calcMinDays(minDays));
}

void QnCameraScheduleWidget::updateMaxDays()
{
    auto calcMaxDays = [](int d) { return d == 0 ? ec2::kDefaultMaxArchiveDays : qAbs(d); };

    const int maxDays = m_cameras.isEmpty()
        ? 0
        : (*std::max_element(m_cameras.cbegin(), m_cameras.cend(),
            [calcMaxDays](const QnVirtualCameraResourcePtr &l, const QnVirtualCameraResourcePtr &r)
            {
                return calcMaxDays(l->maxDays()) < calcMaxDays(r->maxDays());
            }))->maxDays();

    bool sameMaxDays = boost::algorithm::all_of(m_cameras,
        [maxDays](const QnVirtualCameraResourcePtr &camera)
        {
            return camera->maxDays() == maxDays;
        });

    QnCheckbox::setupTristateCheckbox(ui->checkBoxMaxArchive, sameMaxDays, maxDays <= 0);
    ui->spinBoxMaxDays->setValue(calcMaxDays(maxDays));
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
    QnCamLicenseUsageHelper licenseHelper(m_cameras, true);
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

void QnCameraScheduleWidget::updateGridParams(bool pickedFromGrid)
{
    if (m_updating)
        return;

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

    for (auto label : {ui->labelAlways, ui->labelMotionOnly, ui->labelMotionPlusLQ, ui->labelNoRecord})
    {
        auto button = qobject_cast<QAbstractButton*>(label->buddy());
        QPalette::ColorRole foreground = button && button->isChecked() ? QPalette::Highlight : QPalette::WindowText;
        label->setForegroundRole(foreground);
    }

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

    //TODO: #GDM is it really needed here?
    updateMaxFPS();

    checkScheduleSet();
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

void QnCameraScheduleWidget::updateArchiveRangeEnabledState()
{
    ui->spinBoxMaxDays->setEnabled(ui->checkBoxMaxArchive->checkState() == Qt::Unchecked);
    ui->spinBoxMinDays->setEnabled(ui->checkBoxMinArchive->checkState() == Qt::Unchecked);
    validateArchiveLength();
}

void QnCameraScheduleWidget::updateGridEnabledState()
{
    ui->motionGroupBox->setEnabled(m_recordingParamsAvailable);
    updateArchiveRangeEnabledState();
}

void QnCameraScheduleWidget::updateLicensesLabelText()
{
    QnCamLicenseUsageHelper helper;

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

    checkCanEnableRecording();
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
        checkRecordingEnabled();
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
    menu()->trigger(QnActions::PreferencesLicensesTabAction);
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
        QnMessageBox::warning(
            this,
            tr("Warning"),
            tr("Motion Recording is disabled or not supported on some of the selected cameras. Please go to the motion setup page to ensure it is supported and enabled.")
        );
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
            QnMessageBox::warning(
                this,
                tr("Warning"),
                tr("Dual-Streaming is not supported on this camera."));
        }
        else if (!hasMotion && !hasDualStreaming)
        {
            QnMessageBox::warning(
                this,
                tr("Warning"),
                tr("Dual-Streaming and Motion Detection are not available on this camera."));
        }
        else /* Has dual streaming but not motion */
        {
            QnMessageBox::warning(
                this,
                tr("Warning"),
                tr("Motion Recording is disabled. Please go to the motion setup page to setup the cameras's motion area and sensitivity."));
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
        {
            int maxDays = maxRecordedDays();
            if (maxDays != kRecordedDaysDontChange)
                camera->setMaxDays(maxDays);
            int minDays = minRecordedDays();
            if (minDays != kRecordedDaysDontChange)
                camera->setMinDays(minDays);
        }

        camera->setScheduleDisabled(!recordingEnabled);
        if (recordingEnabled)
        {
            int maxFps = camera->getMaxFps();

            //TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
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
        }
    };

    auto selectedCameras = qnResPool->getResources<QnVirtualCameraResource>(
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

int QnCameraScheduleWidget::maxRecordedDays() const
{
    switch (ui->checkBoxMaxArchive->checkState())
    {
        case Qt::Unchecked:
            return ui->spinBoxMaxDays->value();
        case Qt::Checked:   //automatically manage but save for future use
            return ui->spinBoxMaxDays->value() * -1;
        default:
            return kRecordedDaysDontChange;
    }
}

int QnCameraScheduleWidget::minRecordedDays() const
{
    switch (ui->checkBoxMinArchive->checkState())
    {
        case Qt::Unchecked:
            return ui->spinBoxMinDays->value();
        case Qt::Checked:   //automatically manage but save for future use
            return ui->spinBoxMinDays->value() * -1;
        default:
            return kRecordedDaysDontChange;
    }
}

void QnCameraScheduleWidget::validateArchiveLength()
{
    if (ui->checkBoxMinArchive->checkState() == Qt::Unchecked && ui->checkBoxMaxArchive->checkState() == Qt::Unchecked)
    {
        if (ui->spinBoxMaxDays->value() < ui->spinBoxMinDays->value())
            ui->spinBoxMaxDays->setValue(ui->spinBoxMinDays->value());
    }

    QString alertText;
    bool alertVisible = ui->spinBoxMinDays->isEnabled() && ui->spinBoxMinDays->value() > kDangerousMinArchiveDays;

    if (alertVisible)
    {
        alertText = QnDeviceDependentStrings::getDefaultNameFromSet(
            tr("High minimum value can lead to archive length decrease on other devices."),
            tr("High minimum value can lead to archive length decrease on other cameras."));
    }

    setArchiveLengthAlert(alertText);
}

void QnCameraScheduleWidget::setScheduleAlert(const QString& scheduleAlert)
{
    /* We want to force update - emit a signal - even if the text didn't change: */
    m_scheduleAlert = scheduleAlert;
    emit alert(m_scheduleAlert.isEmpty() ? m_archiveLengthAlert : m_scheduleAlert);
}

void QnCameraScheduleWidget::setArchiveLengthAlert(const QString& archiveLengthAlert)
{
    /* We want to force update - emit a signal - even if the text didn't change: */
    m_archiveLengthAlert = archiveLengthAlert;
    emit alert(m_archiveLengthAlert.isEmpty() ? m_scheduleAlert : m_archiveLengthAlert);
}

bool QnCameraScheduleWidget::checkCanEnableRecording()
{
    setScheduleAlert(QString());
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
}

void QnCameraScheduleWidget::checkRecordingEnabled()
{
    if (!checkCanEnableRecording())
        return;

    if (isScheduleEnabled())
        setScheduleAlert(QString());
    else
        setScheduleAlert(tr("Turn on selector at the top of the window to enable recording."));
}

void QnCameraScheduleWidget::checkScheduleSet()
{
    if (!checkCanEnableRecording())
        return;

    if (!isRecordingScheduled())
        setScheduleAlert(tr("Select areas on the schedule to apply chosen parameters to."));
}

void QnCameraScheduleWidget::checkScheduleParamsSet()
{
    if (!checkCanEnableRecording())
        return;

    if (!isRecordingScheduled())
        setScheduleAlert(tr("Set recording parameters and select areas on the schedule grid to apply them to."));
    else
        checkRecordingEnabled();
}

bool QnCameraScheduleWidget::isRecordingScheduled() const
{
    return any_of(scheduleTasks(),
        [](const QnScheduleTask& task) -> bool
        {
            return !task.isEmpty() && task.getRecordingType() != Qn::RT_Never;
        });
}
