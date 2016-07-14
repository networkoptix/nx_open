#include "camera_schedule_widget.h"
#include "ui_camera_schedule_widget.h"

#include <QtCore/QCoreApplication>

//TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <licensing/license.h>

#include <ui/actions/action_manager.h>
#include <ui/common/read_only.h>
#include <ui/common/checkbox_utils.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/custom_style.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/event_processors.h>
#include <utils/math/color_transformations.h>
#include <utils/license_usage_helper.h>

namespace {

    class QnExportScheduleResourceSelectionDialogDelegate: public QnResourceSelectionDialogDelegate {
        Q_DECLARE_TR_FUNCTIONS(QnExportScheduleResourceSelectionDialogDelegate);
        typedef QnResourceSelectionDialogDelegate base_type;
    public:
        QnExportScheduleResourceSelectionDialogDelegate(QWidget* parent,
                                                        bool recordingEnabled,
                                                        bool motionUsed,
                                                        bool dualStreamingUsed,
                                                        bool hasVideo
                                                        ):
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

        ~QnExportScheduleResourceSelectionDialogDelegate() {

        }

        bool doCopyArchiveLength() const {
            return m_archiveCheckbox->isChecked();
        }

        virtual void init(QWidget* parent) override {
            m_parentPalette = parent->palette();

            m_archiveCheckbox = new QCheckBox(parent);
            m_archiveCheckbox->setText(tr("Copy archive length settings"));
            parent->layout()->addWidget(m_archiveCheckbox);

            m_licensesLabel = new QLabel(parent);
            parent->layout()->addWidget(m_licensesLabel);

            auto addWarningLabel = [parent](const QString &text) -> QLabel* {
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

        virtual bool validate(const QnResourceList &selected) override {
            using boost::algorithm::all_of;
            using boost::algorithm::any_of;

            QnVirtualCameraResourceList cameras = selected.filtered<QnVirtualCameraResource>();

            QnCamLicenseUsageHelper helper(cameras, m_recordingEnabled);

            QPalette palette = m_parentPalette;
            bool licensesOk = helper.isValid();
            QString licenseUsage = helper.getProposedUsageMsg();
            if(!licensesOk) {
                setWarningStyle(&palette);
                licenseUsage += L'\n' + helper.getRequiredMsg();
            }
            m_licensesLabel->setText(licenseUsage);
            m_licensesLabel->setPalette(palette);

            bool motionOk = true;
            if (m_motionUsed) {
                foreach (const QnVirtualCameraResourcePtr &camera, cameras)
                {
                    bool hasMotion = (camera->getMotionType() != Qn::MT_NoMotion);
                    if (!hasMotion) {
                        motionOk = false;
                        break;
                    }
                    if (m_dualStreamingUsed && !camera->hasDualStreaming2()){
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

    const int defaultMinArchiveDays = 1;
    const int dangerousMinArchiveDays = 5;
    const int defaultMaxArchiveDays = 30;
}

QnCameraScheduleWidget::QnCameraScheduleWidget(QWidget *parent):
    QWidget(parent),
    QnWorkbenchContextAware(parent, true),
    QnUpdatable(),
    ui(new Ui::CameraScheduleWidget),
    m_disableUpdateGridParams(false),
    m_motionAvailable(true),
    m_recordingParamsAvailable(true),
    m_changesDisabled(false),
    m_readOnly(false),
    m_maxFps(0),
    m_maxDualStreamingFps(0)
{
    ui->setupUi(this);

    ui->enableRecordingCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);

    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityLow), Qn::QualityLow);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityNormal), Qn::QualityNormal);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityHigh), Qn::QualityHigh);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityHighest), Qn::QualityHighest);
    ui->qualityComboBox->setCurrentIndex(ui->qualityComboBox->findData(Qn::QualityHigh));

    setHelpTopic(ui->archiveGroupBox, Qn::CameraSettings_Recording_ArchiveLength_Help);
    setHelpTopic(ui->exportScheduleButton, Qn::CameraSettings_Recording_Export_Help);

    // init buttons
    connect(ui->gridWidget, &QnScheduleGridWidget::colorsChanged, this, &QnCameraScheduleWidget::updateColors);
    updateColors();

    QnCamLicenseUsageHelper helper;
    ui->licensesUsageWidget->init(&helper);

    QnCheckbox::autoCleanTristate(ui->enableRecordingCheckBox);
    QnCheckbox::autoCleanTristate(ui->checkBoxMinArchive);
    QnCheckbox::autoCleanTristate(ui->checkBoxMaxArchive);

    auto notifyAboutScheduleEnabledChanged = [this](int state)
    {
        if (!isUpdating())
            emit scheduleEnabledChanged(state);
    };

    auto notifyAboutArchiveRangeChanged = [this]
    {
        if (!isUpdating())
            emit archiveRangeChanged();
    };

    connect(ui->recordAlwaysButton,         &QnCheckedButton::toggled,                      this, &QnCameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionButton,         &QnCheckedButton::toggled,                      this, &QnCameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionPlusLQButton,   &QnCheckedButton::toggled,                      this, &QnCameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionPlusLQButton,   &QnCheckedButton::toggled,                      this, &QnCameraScheduleWidget::updateMaxFpsValue);
    connect(ui->noRecordButton,             &QnCheckedButton::toggled,                      this, &QnCameraScheduleWidget::updateGridParams);
    connect(ui->qualityComboBox,            QnComboboxCurrentIndexChanged,                  this, &QnCameraScheduleWidget::updateGridParams);
    connect(ui->fpsSpinBox,                 QnSpinboxIntValueChanged,                       this, &QnCameraScheduleWidget::updateGridParams);
    connect(this,                           &QnCameraScheduleWidget::scheduleTasksChanged,  this, &QnCameraScheduleWidget::updateRecordSpinboxes);

    connect(ui->recordBeforeSpinBox,        QnSpinboxIntValueChanged,                       this, &QnCameraScheduleWidget::recordingSettingsChanged);
    connect(ui->recordAfterSpinBox,         QnSpinboxIntValueChanged,                       this, &QnCameraScheduleWidget::recordingSettingsChanged);

    connect(ui->licensesButton,             &QPushButton::clicked,                          this, &QnCameraScheduleWidget::at_licensesButton_clicked);
    connect(ui->displayQualityCheckBox,     &QCheckBox::stateChanged,                       this, &QnCameraScheduleWidget::at_displayQualiteCheckBox_stateChanged);
    connect(ui->displayFpsCheckBox,         &QCheckBox::stateChanged,                       this, &QnCameraScheduleWidget::at_displayFpsCheckBox_stateChanged);

    connect(ui->enableRecordingCheckBox,    &QCheckBox::stateChanged,                       this, &QnCameraScheduleWidget::updateLicensesLabelText);
    connect(ui->enableRecordingCheckBox,    &QCheckBox::stateChanged,                       this, notifyAboutScheduleEnabledChanged);

    connect(ui->gridWidget,                 &QnScheduleGridWidget::cellActivated,           this, &QnCameraScheduleWidget::at_gridWidget_cellActivated);

    connect(ui->checkBoxMinArchive,         &QCheckBox::stateChanged,                       this, &QnCameraScheduleWidget::updateArchiveRangeEnabledState);
    connect(ui->checkBoxMinArchive,         &QCheckBox::stateChanged,                       this, notifyAboutArchiveRangeChanged);

    connect(ui->checkBoxMaxArchive,         &QCheckBox::stateChanged,                       this, &QnCameraScheduleWidget::updateArchiveRangeEnabledState);
    connect(ui->checkBoxMaxArchive,         &QCheckBox::stateChanged,                       this, notifyAboutArchiveRangeChanged);

    connect(ui->spinBoxMinDays,             QnSpinboxIntValueChanged,                       this, &QnCameraScheduleWidget::validateArchiveLength);
    connect(ui->spinBoxMinDays,             QnSpinboxIntValueChanged,                       this, notifyAboutArchiveRangeChanged);

    connect(ui->spinBoxMaxDays,             QnSpinboxIntValueChanged,                       this, &QnCameraScheduleWidget::validateArchiveLength);
    connect(ui->spinBoxMaxDays,             QnSpinboxIntValueChanged,                       this, notifyAboutArchiveRangeChanged);

    connect(ui->exportScheduleButton,       &QPushButton::clicked,                          this, &QnCameraScheduleWidget::at_exportScheduleButton_clicked);

    ui->exportWarningLabel->setVisible(false);

    QnSingleEventSignalizer *releaseSignalizer = new QnSingleEventSignalizer(this);
    releaseSignalizer->setEventType(QEvent::MouseButtonRelease);
    connect(releaseSignalizer, &QnSingleEventSignalizer::activated, this, &QnCameraScheduleWidget::at_releaseSignalizer_activated);
    ui->recordMotionButton->installEventFilter(releaseSignalizer);
    ui->recordMotionPlusLQButton->installEventFilter(releaseSignalizer);

    QnSingleEventSignalizer* gridMouseReleaseSignalizer = new QnSingleEventSignalizer(this);
    gridMouseReleaseSignalizer->setEventType(QEvent::MouseButtonRelease);
    connect(gridMouseReleaseSignalizer, &QnSingleEventSignalizer::activated, this, &QnCameraScheduleWidget::controlsChangesApplied);
    ui->gridWidget->installEventFilter(gridMouseReleaseSignalizer);

    setWarningStyle(ui->minArchiveDaysWarningLabel);
    ui->minArchiveDaysWarningLabel->setVisible(false);

    connectToGridWidget();

    updateGridEnabledState();
    updateMotionButtons();

    auto updateLicensesIfNeeded = [this] {
        if (!isVisible())
            return;
        updateLicensesLabelText();
    };

    QnCamLicenseUsageWatcher* camerasUsageWatcher = new QnCamLicenseUsageWatcher(this);
    connect(camerasUsageWatcher, &QnLicenseUsageWatcher::licenseUsageChanged, this,  updateLicensesIfNeeded);

    retranslateUi();
}

QnCameraScheduleWidget::~QnCameraScheduleWidget()
{
}

void QnCameraScheduleWidget::retranslateUi()
{
    ui->retranslateUi(this);

    QString warnText = (qnResPool && qnResPool->containsIoModules())
        ? tr("Warning! High minimum value could decrease other devices' recording durations.")
        : tr("Warning! High minimum value could decrease other cameras' recording durations.");

    ui->minArchiveDaysWarningLabel->setText(warnText);

    ui->scheduleGridGroupBox->setTitle(lit("%1\t(%2)").arg(
        ui->scheduleGridGroupBox->title()).arg(
            tr("based on server time")));
}

void QnCameraScheduleWidget::afterContextInitialized() {
    connect(context()->instance<QnWorkbenchPanicWatcher>(), &QnWorkbenchPanicWatcher::panicModeChanged, this, &QnCameraScheduleWidget::updatePanicLabelText);
    connect(context(), &QnWorkbenchContext::userChanged, this, &QnCameraScheduleWidget::updateLicensesButtonVisible);
    updatePanicLabelText();
    updateLicensesButtonVisible();
}

bool QnCameraScheduleWidget::hasHeightForWidth() const
{
    return false;   //TODO: #GDM temporary fix to handle string freeze
                    // all labels with word-wrap should be replaced by QnWordWrappedLabel.
                    // This widget has 5 of them (label_4.._8, exportWarningLabel)
}

void QnCameraScheduleWidget::connectToGridWidget()
{
    connect(ui->gridWidget, &QnScheduleGridWidget::cellValueChanged, this, &QnCameraScheduleWidget::scheduleTasksChanged);
}

void QnCameraScheduleWidget::disconnectFromGridWidget()
{
    disconnect(ui->gridWidget, &QnScheduleGridWidget::cellValueChanged, this, &QnCameraScheduleWidget::scheduleTasksChanged);
}

void QnCameraScheduleWidget::beforeUpdate() {
    disconnectFromGridWidget();
}

void QnCameraScheduleWidget::afterUpdate() {
    connectToGridWidget();
    updateGridParams(); // TODO: #GDM #Common does not belong here...
}

void QnCameraScheduleWidget::setChangesDisabled(bool val)
{
    m_changesDisabled = val;

    updateGridEnabledState(); /* Update controls' enabled states. */
}

bool QnCameraScheduleWidget::isChangesDisabled() const
{
    return m_changesDisabled;
}

bool QnCameraScheduleWidget::isReadOnly() const
{
    return m_readOnly;
}

void QnCameraScheduleWidget::setReadOnly(bool readOnly)
{
    if(m_readOnly == readOnly)
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

const QnVirtualCameraResourceList &QnCameraScheduleWidget::cameras() const {
    return m_cameras;
}

void QnCameraScheduleWidget::setCameras(const QnVirtualCameraResourceList &cameras) {
    if(m_cameras == cameras)
        return;

    for (const QnVirtualCameraResourcePtr &camera: m_cameras)
        disconnect(camera.data(), &QnSecurityCamResource::resourceChanged, this, &QnCameraScheduleWidget::updateMotionButtons);

    m_cameras = cameras;

    for (const QnVirtualCameraResourcePtr &camera: m_cameras)
        connect(camera.data(), &QnSecurityCamResource::resourceChanged, this, &QnCameraScheduleWidget::updateMotionButtons, Qt::QueuedConnection);

    updateScheduleEnabled();
    updateMinDays();
    updateMaxDays();
    updatePanicLabelText();
    updateMotionButtons();
    updateLicensesLabelText();
    retranslateUi();
}

void QnCameraScheduleWidget::updateScheduleEnabled() {
    int enabledCount = 0, disabledCount = 0;
    for (const QnVirtualCameraResourcePtr &camera: m_cameras) {
        (camera->isScheduleDisabled() ? disabledCount : enabledCount)++;
    }
    QnCheckbox::setupTristateCheckbox(ui->enableRecordingCheckBox, enabledCount == 0 || disabledCount == 0, enabledCount > 0);
}

void QnCameraScheduleWidget::updateMinDays() {
    auto calcMinDays = [](int d) { return d == 0 ? defaultMinArchiveDays : qAbs(d);  };

    const int minDays = m_cameras.isEmpty()
        ? 0
        : (*std::min_element(m_cameras.cbegin(), m_cameras.cend(), [calcMinDays](const QnVirtualCameraResourcePtr &l, const QnVirtualCameraResourcePtr &r) {
                return calcMinDays(l->minDays()) < calcMinDays(r->minDays());
            }))->minDays();

    bool sameMinDays = boost::algorithm::all_of(m_cameras, [minDays](const QnVirtualCameraResourcePtr &camera) {
        return camera->minDays() == minDays;
    });

    QnCheckbox::setupTristateCheckbox(ui->checkBoxMinArchive, sameMinDays, minDays <= 0);
    ui->spinBoxMinDays->setValue(calcMinDays(minDays));
}

void QnCameraScheduleWidget::updateMaxDays() {
    auto calcMaxDays = [](int d) { return d == 0 ? defaultMaxArchiveDays : qAbs(d);  };

    const int maxDays = m_cameras.isEmpty()
        ? 0
        : (*std::max_element(m_cameras.cbegin(), m_cameras.cend(), [calcMaxDays](const QnVirtualCameraResourcePtr &l, const QnVirtualCameraResourcePtr &r) {
                return calcMaxDays(l->maxDays()) < calcMaxDays(r->maxDays());
            }))->maxDays();

    bool sameMaxDays = boost::algorithm::all_of(m_cameras, [maxDays](const QnVirtualCameraResourcePtr &camera) {
        return camera->maxDays() == maxDays;
    });

    QnCheckbox::setupTristateCheckbox(ui->checkBoxMaxArchive, sameMaxDays, maxDays <= 0);
    ui->spinBoxMaxDays->setValue(calcMaxDays(maxDays));
}

void QnCameraScheduleWidget::setExportScheduleButtonEnabled(bool enabled) {
    ui->exportScheduleButton->setEnabled(enabled);
    ui->exportWarningLabel->setVisible(!enabled);
}

void QnCameraScheduleWidget::updatePanicLabelText() {
    ui->panicModeLabel->setText(tr("Off"));
    ui->panicModeLabel->setPalette(this->palette());

    if(!context())
        return;

    if(context()->instance<QnWorkbenchPanicWatcher>()->isPanicMode()) {
        QPalette palette = this->palette();
        palette.setColor(QPalette::WindowText, QColor(255, 0, 0));
        ui->panicModeLabel->setPalette(palette);
        ui->panicModeLabel->setText(tr("On"));
    }
}

QList<QnScheduleTask::Data> QnCameraScheduleWidget::scheduleTasks() const
{
    QList<QnScheduleTask::Data> tasks;
    for (int row = 0; row < ui->gridWidget->rowCount(); ++row) {
        QnScheduleTask::Data task;

        for (int col = 0; col < ui->gridWidget->columnCount();) {
            const QPoint cell(col, row);

            Qn::RecordingType recordType = ui->gridWidget->cellRecordingType(cell);
            Qn::StreamQuality streamQuality = Qn::QualityHighest;
            if (recordType != Qn::RT_Never)
                streamQuality = (Qn::StreamQuality) ui->gridWidget->cellValue(cell, QnScheduleGridWidget::QualityParam).toInt();
            int fps = ui->gridWidget->cellValue(cell, QnScheduleGridWidget::FpsParam).toInt();
            if (fps == 0 && recordType != Qn::RT_Never)
                fps = 10;

            if (task.m_startTime == task.m_endTime) {
                // an invalid one; initialize
                task.m_dayOfWeek = row + 1;
                task.m_startTime = col * 3600; // in secs from start of day
                task.m_endTime = (col + 1) * 3600; // in secs from start of day
                task.m_recordType = recordType;
                task.m_beforeThreshold = ui->recordBeforeSpinBox->value();
                task.m_afterThreshold = ui->recordAfterSpinBox->value();
                task.m_streamQuality = streamQuality;
                task.m_fps = fps;
                ++col;
            } else if (task.m_recordType == recordType && task.m_streamQuality == streamQuality && task.m_fps == fps) {
                task.m_endTime = (col + 1) * 3600; // in secs from start of day
                ++col;
            } else {
                tasks.append(task);
                task = QnScheduleTask::Data();
            }
        }

        if (task.m_startTime != task.m_endTime)
            tasks.append(task);
    }

    return tasks;
}

void QnCameraScheduleWidget::setScheduleTasks(const QnScheduleTaskList taskFrom)
{
    QList<QnScheduleTask::Data> rez;
    foreach(const QnScheduleTask& task, taskFrom)
        rez << task.getData();
    setScheduleTasks(rez);
}

void QnCameraScheduleWidget::setScheduleTasks(const QList<QnScheduleTask::Data> &tasksFrom)
{
    //if (!tasksFrom.isEmpty())
    //    doNotChangeStateChanged(0);

    disconnectFromGridWidget(); /* We don't want to get 100500 notifications. */

    QList<QnScheduleTask::Data> tasks = tasksFrom;

    ui->gridWidget->resetCellValues();

    // emptySource means that cameras with different schedules are selected
    bool emptySource = tasks.isEmpty();
    if (!emptySource) {
        const QnScheduleTask::Data &task = tasks.first();

        ui->recordBeforeSpinBox->setValue(task.m_beforeThreshold);
        ui->recordAfterSpinBox->setValue(task.m_afterThreshold);
    } else {
        for (int nDay = 1; nDay <= 7; ++nDay)
            tasks.append(QnScheduleTask::Data(nDay, 0, 86400, Qn::RT_Never, 10, 10));
    }

    foreach (const QnScheduleTask::Data &task, tasks) {
        const int row = task.m_dayOfWeek - 1;
        Qn::StreamQuality q = Qn::QualityNotDefined;
        if (task.m_recordType != Qn::RT_Never)
        {
            switch (task.m_streamQuality)
            {
                case Qn::QualityLow:
                case Qn::QualityNormal:
                case Qn::QualityHigh:
                case Qn::QualityHighest:
                    q = task.m_streamQuality;
                    break;
                default:
                    qWarning("QnCameraScheduleWidget::setScheduleTasks(): Unhandled StreamQuality value %d.", task.m_streamQuality);
                    break;
            }
        }

        //int fps = task.m_fps;
        QString fps = QLatin1String("-");
        if (task.m_recordType != Qn::RT_Never)
            fps = QString::number(task.m_fps);

        for (int col = task.m_startTime / 3600; col < task.m_endTime / 3600; ++col) {
            const QPoint cell(col, row);

            ui->gridWidget->setCellRecordingType(cell, task.m_recordType);
            ui->gridWidget->setCellValue(cell, QnScheduleGridWidget::QualityParam, q);
            ui->gridWidget->setCellValue(cell, QnScheduleGridWidget::FpsParam, fps);
            ui->gridWidget->setCellValue(cell, QnScheduleGridWidget::DiffersFlagParam, emptySource);
        }
    }

    connectToGridWidget();

    if (!isUpdating())
        emit scheduleTasksChanged();
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

void QnCameraScheduleWidget::updateGridParams(bool fromUserInput)
{
    if (isUpdating())
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

    bool enabled = !ui->noRecordButton->isChecked();
    ui->fpsSpinBox->setEnabled(enabled && m_recordingParamsAvailable);
    ui->qualityComboBox->setEnabled(enabled && m_recordingParamsAvailable);
    updateRecordSpinboxes();


    if(!(m_readOnly && fromUserInput)) {
        ui->gridWidget->setDefaultParam(QnScheduleGridWidget::RecordTypeParam, recordType);
        ui->gridWidget->setDefaultParam(QnScheduleGridWidget::DiffersFlagParam, false);
        if (ui->noRecordButton->isChecked())
        {
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::FpsParam, QLatin1String("-"));
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::QualityParam, Qn::QualityNotDefined);
        }
        else
        {
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::FpsParam, QString::number(ui->fpsSpinBox->value()));
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::QualityParam, ui->qualityComboBox->itemData(ui->qualityComboBox->currentIndex()));
        }
    }

    emit gridParamsChanged();
}

void QnCameraScheduleWidget::setFps(int value)
{
    ui->fpsSpinBox->setValue(value);
}

void QnCameraScheduleWidget::setMaxFps(int value, int dualStreamValue) {
    /* Silently ignoring invalid input is OK here. */
    if(value < ui->fpsSpinBox->minimum())
        value = ui->fpsSpinBox->minimum();
    if(dualStreamValue < ui->fpsSpinBox->minimum())
        dualStreamValue = ui->fpsSpinBox->minimum();

    if (m_maxFps != value) {
        m_maxFps = value;
        int currentMaxFps = getGridMaxFps();
        if (currentMaxFps > m_maxFps)
            emit scheduleTasksChanged();
    }

    if (m_maxDualStreamingFps != dualStreamValue) {
        m_maxDualStreamingFps = dualStreamValue;
        int currentMaxDualStreamingFps = getGridMaxFps(true);
        if (currentMaxDualStreamingFps > m_maxDualStreamingFps)
            emit scheduleTasksChanged();
    }

    updateMaxFpsValue(ui->recordMotionPlusLQButton->isChecked());
    ui->gridWidget->setMaxFps(m_maxFps, m_maxDualStreamingFps);
}

int QnCameraScheduleWidget::getGridMaxFps(bool motionPlusLqOnly)
{
    return ui->gridWidget->getMaxFps(motionPlusLqOnly);
}

void QnCameraScheduleWidget::setScheduleEnabled(bool enabled)
{
    ui->enableRecordingCheckBox->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
}

bool QnCameraScheduleWidget::isScheduleEnabled() const {
    return ui->enableRecordingCheckBox->checkState() != Qt::Unchecked;
}

void QnCameraScheduleWidget::setMotionAvailable(bool available) {
    if(m_motionAvailable == available)
        return;

    m_motionAvailable = available;

    updateMotionButtons();
}

void QnCameraScheduleWidget::setRecordingParamsAvailability(bool available)
{
    if(m_recordingParamsAvailable == available)
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

bool QnCameraScheduleWidget::isMotionAvailable() const
{
    return m_motionAvailable;
}

bool QnCameraScheduleWidget::isRecordingParamsAvailable() const
{
    return m_recordingParamsAvailable;
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
    ui->gridWidget->setEnabled(!m_changesDisabled);
    updateArchiveRangeEnabledState();
}

void QnCameraScheduleWidget::updateLicensesLabelText()
{
    QnCamLicenseUsageHelper helper;

    switch(ui->enableRecordingCheckBox->checkState())
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

void QnCameraScheduleWidget::updateMotionButtons() {
    bool hasDualStreaming = !m_cameras.isEmpty();
    bool hasMotion = !m_cameras.isEmpty();
    foreach(const QnVirtualCameraResourcePtr &camera, m_cameras) {
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

    if(ui->recordMotionButton->isChecked() && !ui->recordMotionButton->isEnabled())
        ui->recordAlwaysButton->setChecked(true);
    if(ui->recordMotionPlusLQButton->isChecked() && !ui->recordMotionPlusLQButton->isEnabled())
        ui->recordAlwaysButton->setChecked(true);

    if(!m_motionAvailable) {
        for (int row = 0; row < ui->gridWidget->rowCount(); ++row) {
            for (int col = 0; col < ui->gridWidget->columnCount(); ++col) {
                const QPoint cell(col, row);
                Qn::RecordingType recordType = ui->gridWidget->cellRecordingType(cell);
                if(recordType == Qn::RT_MotionOnly || recordType == Qn::RT_MotionAndLowQuality)
                    ui->gridWidget->setCellRecordingType(cell, Qn::RT_Always);
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

void QnCameraScheduleWidget::updateColors()
{
    ui->recordAlwaysButton->setColor(ui->gridWidget->colors().recordAlways);
    ui->recordMotionButton->setColor(ui->gridWidget->colors().recordMotion);
    ui->recordMotionPlusLQButton->setColor(ui->gridWidget->colors().recordMotion);
    ui->recordMotionPlusLQButton->setInsideColor(ui->gridWidget->colors().recordAlways);
    ui->noRecordButton->setColor(ui->gridWidget->colors().recordNever);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCameraScheduleWidget::at_gridWidget_cellActivated(const QPoint &cell)
{
    m_disableUpdateGridParams = true;

    Qn::RecordingType recordType = ui->gridWidget->cellRecordingType(cell);
    double fps(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::FpsParam).toDouble());
    Qn::StreamQuality q = (Qn::StreamQuality) ui->gridWidget->cellValue(cell, QnScheduleGridWidget::QualityParam).toInt();

    switch (recordType) {
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

    if (recordType != Qn::RT_Never)
    {
        ui->fpsSpinBox->setValue(fps);
        ui->qualityComboBox->setCurrentIndex(qualityToComboIndex(q));
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

void QnCameraScheduleWidget::at_releaseSignalizer_activated(QObject *target) {
    QWidget *widget = qobject_cast<QWidget *>(target);
    if(!widget)
        return;

    if(m_cameras.isEmpty() || widget->isEnabled() || (widget->parentWidget() && !widget->parentWidget()->isEnabled()))
        return;

    using boost::algorithm::all_of;

    if(m_cameras.size() > 1) {
        QnMessageBox::warning(
            this,
            tr("Warning"),
            tr("Motion Recording is disabled or not supported on some of the selected cameras. Please go to the motion setup page to ensure it is supported and enabled.")
            );
    } else /* One camera */ {
        NX_ASSERT(m_cameras.size() == 1, Q_FUNC_INFO, "Following options are valid only for singular camera");
        QnVirtualCameraResourcePtr camera = m_cameras.first();

        // TODO: #GDM #Common duplicate code.
        bool hasDualStreaming = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) {return camera->hasDualStreaming2(); });
        bool hasMotion = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) {return camera->hasMotion(); });

        if (hasMotion && !hasDualStreaming) {
            QnMessageBox::warning(
                this,
                tr("Warning"),
                tr("Dual-Streaming is not supported on this camera."));
        } else if(!hasMotion && !hasDualStreaming) {
            QnMessageBox::warning(
                this,
                tr("Warning"),
                tr("Dual-Streaming and Motion Detection are not available on this camera."));
        } else /* Has dual streaming but not motion */ {
            QnMessageBox::warning(
                this,
                tr("Warning"),
                tr("Motion Recording is disabled. Please go to the motion setup page to setup the cameras's motion area and sensitivity."));
        }
    }
}

void QnCameraScheduleWidget::at_exportScheduleButton_clicked() {
    bool recordingEnabled = ui->enableRecordingCheckBox->checkState() == Qt::Checked;
    bool motionUsed = recordingEnabled && hasMotionOnGrid();
    bool dualStreamingUsed = motionUsed && hasDualStreamingMotionOnGrid();
    bool hasVideo = boost::algorithm::all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) {return camera->hasVideo(0); } );

    QScopedPointer<QnResourceSelectionDialog> dialog(new QnResourceSelectionDialog(this));
    auto dialogDelegate = new QnExportScheduleResourceSelectionDialogDelegate(this, recordingEnabled, motionUsed, dualStreamingUsed, hasVideo);
    dialog->setDelegate(dialogDelegate);
    dialog->setSelectedResources(m_cameras);
    setHelpTopic(dialog.data(), Qn::CameraSettings_Recording_Export_Help);
    if (!dialog->exec())
        return;

    const bool copyArchiveLength = dialogDelegate->doCopyArchiveLength();

    auto applyChanges = [this, copyArchiveLength, recordingEnabled](const QnVirtualCameraResourcePtr &camera) {
        if (copyArchiveLength) {
            int maxDays = maxRecordedDays();
            if (maxDays != RecordedDaysDontChange)
                camera->setMaxDays(maxDays);
            int minDays = minRecordedDays();
            if (minDays != RecordedDaysDontChange)
                camera->setMinDays(minDays);
        }

        camera->setScheduleDisabled(!recordingEnabled);
        if (recordingEnabled){
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
            foreach(const QnScheduleTask::Data &data, scheduleTasks()){
                QnScheduleTask task(data);
                if (task.getRecordingType() == Qn::RT_MotionAndLowQuality)
                    task.setFps(qMin(task.getFps(), maxFps - decreaseIfMotionPlusLQ));
                else
                    task.setFps(qMin(task.getFps(), maxFps - decreaseAlways));
                tasks.append(task);
            }

            camera->setScheduleTasks(tasks);
        }
    };

    qnResourcesChangesManager->saveCameras(dialog->selectedResources().filtered<QnVirtualCameraResource>(), applyChanges);
    updateLicensesLabelText();
}

bool QnCameraScheduleWidget::hasMotionOnGrid() const {
    for (int row = 0; row < ui->gridWidget->rowCount(); ++row) {
        for (int col = 0; col < ui->gridWidget->columnCount(); ++col) {
            const QPoint cell(col, row);
            Qn::RecordingType recordType = ui->gridWidget->cellRecordingType(cell);
            if (recordType == Qn::RT_MotionOnly || recordType == Qn::RT_MotionAndLowQuality)
                return true;
        }
    }
    return false;
}

bool QnCameraScheduleWidget::hasDualStreamingMotionOnGrid() const {
    for (int row = 0; row < ui->gridWidget->rowCount(); ++row) {
        for (int col = 0; col < ui->gridWidget->columnCount(); ++col) {
            const QPoint cell(col, row);
            Qn::RecordingType recordType = ui->gridWidget->cellRecordingType(cell);
            if(recordType == Qn::RT_MotionAndLowQuality)
                return true;
        }
    }
    return false;
}

int QnCameraScheduleWidget::maxRecordedDays() const {
    switch (ui->checkBoxMaxArchive->checkState()) {
    case Qt::Unchecked:
        return ui->spinBoxMaxDays->value();
    case Qt::Checked:   //automatically manage but save for future use
        return ui->spinBoxMaxDays->value() * -1;
    default:
        return RecordedDaysDontChange;
    }
}

int QnCameraScheduleWidget::minRecordedDays() const {
    switch (ui->checkBoxMinArchive->checkState()) {
    case Qt::Unchecked:
        return ui->spinBoxMinDays->value();
    case Qt::Checked:   //automatically manage but save for future use
        return ui->spinBoxMinDays->value() * -1;
    default:
        return RecordedDaysDontChange;
    }
}

void QnCameraScheduleWidget::validateArchiveLength() {
    if (ui->checkBoxMinArchive->checkState() == Qt::Unchecked && ui->checkBoxMaxArchive->checkState() == Qt::Unchecked)
    {
        if (ui->spinBoxMaxDays->value() < ui->spinBoxMinDays->value())
            ui->spinBoxMaxDays->setValue(ui->spinBoxMinDays->value());
    }
    ui->minArchiveDaysWarningLabel->setVisible(ui->spinBoxMinDays->isEnabled() && ui->spinBoxMinDays->value() > dangerousMinArchiveDays);
}
