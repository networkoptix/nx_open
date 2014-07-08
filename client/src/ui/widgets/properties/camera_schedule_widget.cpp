#include "camera_schedule_widget.h"
#include "ui_camera_schedule_widget.h"

#include <QtCore/QCoreApplication>
#include <QtWidgets/QMessageBox>

//TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <licensing/license.h>

#include <ui/common/read_only.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/warning_style.h>
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
                                                        bool dualStreamingUsed
                                                        ):
            base_type(parent),
            m_licensesLabel(NULL),
            m_motionLabel(NULL),
            m_dtsLabel(NULL),
            m_recordingEnabled(recordingEnabled),
            m_motionUsed(motionUsed),
            m_dualStreamingUsed(dualStreamingUsed)
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

            m_motionLabel = new QLabel(parent);
            m_motionLabel->setText(tr("Schedule motion type is not supported by some cameras"));
            setWarningStyle(m_motionLabel);
            parent->layout()->addWidget(m_motionLabel);
            m_motionLabel->setVisible(false);

            m_dtsLabel = new QLabel(parent);
            m_dtsLabel->setText(tr("Recording cannot be enabled for some cameras"));
            setWarningStyle(m_dtsLabel);
            parent->layout()->addWidget(m_dtsLabel);
            m_dtsLabel->setVisible(false);
        }

        virtual bool validate(const QnResourceList &selected) override {
            QnVirtualCameraResourceList cameras = selected.filtered<QnVirtualCameraResource>();

            QnLicenseUsageHelper helper(cameras, m_recordingEnabled);

            QPalette palette = m_parentPalette;
            bool licensesOk = helper.isValid();
            if(!licensesOk)
                setWarningStyle(&palette);
            m_licensesLabel->setPalette(palette);

            QString usageText =
                    tr("%n license(s) will be used out of %1.", "", helper.usedDigital()).arg(helper.totalDigital());

            if (helper.totalAnalog() > 0) {
                usageText += L'\n';
                usageText += tr("%n analog license(s) will be used out of %1.", "", helper.usedAnalog()).arg(helper.totalAnalog());
            }
            m_licensesLabel->setText(usageText);

            bool motionOk = true;
            if (m_motionUsed){
                foreach (const QnVirtualCameraResourcePtr &camera, cameras)
                {
                    bool hasMotion = camera->getMotionType() != Qn::MT_NoMotion;
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

            bool dtsOk = true;
            foreach (const QnVirtualCameraResourcePtr &camera, cameras){
                if (camera->isDtsBased()) {
                    dtsOk = false;
                    break;
                }
            }
            m_dtsLabel->setVisible(!dtsOk);
            return licensesOk && motionOk && dtsOk;
        }

    private:
        QLabel* m_licensesLabel;
        QLabel* m_motionLabel;
        QLabel* m_dtsLabel;
        QCheckBox *m_archiveCheckbox;
        QPalette m_parentPalette;

        bool m_recordingEnabled;
        bool m_motionUsed;
        bool m_dualStreamingUsed;
    };

    const int dangerousMinArchiveValue = 5;

}

QnCameraScheduleWidget::QnCameraScheduleWidget(QWidget *parent):
    QWidget(parent),
    ui(new Ui::CameraScheduleWidget),
    m_disableUpdateGridParams(false),
    m_motionAvailable(true),
    m_recordingParamsAvailable(true),
    m_changesDisabled(false),
    m_readOnly(false),
    m_maxFps(0),
    m_maxDualStreamingFps(0),
    m_inUpdate(0)
{
    ui->setupUi(this);

    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityLow), Qn::QualityLow);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityNormal), Qn::QualityNormal);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityHigh), Qn::QualityHigh);
    ui->qualityComboBox->addItem(toDisplayString(Qn::QualityHighest), Qn::QualityHighest);

    setHelpTopic(ui->exportScheduleButton, Qn::CameraSettings_Recording_Export_Help);

    // init buttons
    ui->recordAlwaysButton->setColor(qnGlobals->recordAlwaysColor());
    ui->recordAlwaysButton->setCheckedColor(qnGlobals->recordAlwaysColor().lighter());

    ui->recordMotionButton->setColor(qnGlobals->recordMotionColor());
    ui->recordMotionButton->setCheckedColor(qnGlobals->recordMotionColor().lighter());

    ui->recordMotionPlusLQButton->setColor(qnGlobals->recordMotionColor());
    ui->recordMotionPlusLQButton->setCheckedColor(qnGlobals->recordMotionColor().lighter());
    ui->recordMotionPlusLQButton->setInsideColor(qnGlobals->recordAlwaysColor());

    ui->noRecordButton->setColor(qnGlobals->noRecordColor());
    ui->noRecordButton->setCheckedColor(qnGlobals->noRecordColor().lighter());

    connect(ui->recordAlwaysButton,      SIGNAL(toggled(bool)),             this,   SLOT(updateGridParams()));
    connect(ui->recordMotionButton,      SIGNAL(toggled(bool)),             this,   SLOT(updateGridParams()));
    connect(ui->recordMotionPlusLQButton,SIGNAL(toggled(bool)),             this,   SLOT(updateGridParams()));
    connect(ui->recordMotionPlusLQButton,SIGNAL(toggled(bool)),             this,   SLOT(updateMaxFpsValue(bool)));
    connect(ui->noRecordButton,          SIGNAL(toggled(bool)),             this,   SLOT(updateGridParams()));
    connect(ui->qualityComboBox,         SIGNAL(currentIndexChanged(int)),  this,   SLOT(updateGridParams()));
    connect(ui->fpsSpinBox,              SIGNAL(valueChanged(double)),      this,   SLOT(updateGridParams()));
    connect(this,                        SIGNAL(scheduleTasksChanged()),    this, SLOT(updateRecordSpinboxes()));

    connect(ui->recordBeforeSpinBox,    SIGNAL(valueChanged(int)),          this,   SIGNAL(recordingSettingsChanged()));
    connect(ui->recordAfterSpinBox,     SIGNAL(valueChanged(int)),          this,   SIGNAL(recordingSettingsChanged()));

    connect(ui->licensesButton,         SIGNAL(clicked()),                  this,   SLOT(at_licensesButton_clicked()));
    connect(ui->displayQualityCheckBox, SIGNAL(stateChanged(int)),          this,   SLOT(at_displayQualiteCheckBox_stateChanged(int)));
    connect(ui->displayFpsCheckBox,     SIGNAL(stateChanged(int)),          this,   SLOT(at_displayFpsCheckBox_stateChanged(int)));
    connect(ui->enableRecordingCheckBox,SIGNAL(clicked()),                  this,   SLOT(at_enableRecordingCheckBox_clicked()));
    connect(ui->enableRecordingCheckBox, SIGNAL(stateChanged(int)),          this,   SLOT(updateGridEnabledState()));
    connect(ui->enableRecordingCheckBox,SIGNAL(stateChanged(int)),          this,   SIGNAL(scheduleEnabledChanged(int)));
    connect(ui->enableRecordingCheckBox,SIGNAL(stateChanged(int)),          this,   SLOT(updateLicensesLabelText()), Qt::QueuedConnection);
    connect(qnLicensePool,              SIGNAL(licensesChanged()),          this,   SLOT(updateLicensesLabelText()), Qt::QueuedConnection);

    connect(ui->gridWidget,             SIGNAL(cellActivated(QPoint)),      this,   SLOT(at_gridWidget_cellActivated(QPoint)));

    connect(ui->checkBoxMinArchive,      SIGNAL(clicked()),                 this,   SLOT(at_checkBoxArchive_clicked()));
    connect(ui->checkBoxMinArchive,      SIGNAL(stateChanged(int)),         this,   SLOT(updateDaysEnabledState()));
    connect(ui->checkBoxMinArchive,      SIGNAL(stateChanged(int)),         this,   SIGNAL(archiveRangeChanged()));

    connect(ui->checkBoxMaxArchive,      SIGNAL(clicked()),                 this,   SLOT(at_checkBoxArchive_clicked()));
    connect(ui->checkBoxMaxArchive,      SIGNAL(stateChanged(int)),         this,   SLOT(updateDaysEnabledState()));
    connect(ui->checkBoxMaxArchive,      SIGNAL(stateChanged(int)),         this,   SIGNAL(archiveRangeChanged()));

    connect(ui->spinBoxMinDays,          SIGNAL(valueChanged(int)),         this,   SLOT(at_archiveRangeChanged()));
    connect(ui->spinBoxMinDays,          SIGNAL(valueChanged(int)),         this,   SIGNAL(archiveRangeChanged()));

    connect(ui->spinBoxMaxDays,          SIGNAL(valueChanged(int)),         this,   SLOT(at_archiveRangeChanged()));
    connect(ui->spinBoxMaxDays,          SIGNAL(valueChanged(int)),         this,   SIGNAL(archiveRangeChanged()));

    connect(ui->exportScheduleButton,   SIGNAL(clicked()),                  this,   SLOT(at_exportScheduleButton_clicked()));
    ui->exportWarningLabel->setVisible(false);

    QnSingleEventSignalizer *releaseSignalizer = new QnSingleEventSignalizer(this);
    releaseSignalizer->setEventType(QEvent::MouseButtonRelease);
    connect(releaseSignalizer, SIGNAL(activated(QObject *, QEvent *)), this, SLOT(at_releaseSignalizer_activated(QObject *)));
    ui->recordMotionButton->installEventFilter(releaseSignalizer);
    ui->recordMotionPlusLQButton->installEventFilter(releaseSignalizer);

    QnSingleEventSignalizer* gridMouseReleaseSignalizer = new QnSingleEventSignalizer(this);
    gridMouseReleaseSignalizer->setEventType(QEvent::MouseButtonRelease);
    connect(gridMouseReleaseSignalizer, SIGNAL(activated(QObject *, QEvent *)), this, SIGNAL(controlsChangesApplied()));
    ui->gridWidget->installEventFilter(gridMouseReleaseSignalizer);

    setWarningStyle(ui->minArchiveDaysWarningLabel);
    ui->minArchiveDaysWarningLabel->setVisible(false);

    connectToGridWidget();

    updateGridEnabledState();
    updateLicensesLabelText();
    updateMotionButtons();
}

QnCameraScheduleWidget::~QnCameraScheduleWidget() {
    return;
}

void QnCameraScheduleWidget::connectToGridWidget()
{
    connect(ui->gridWidget, SIGNAL(cellValueChanged(const QPoint &)), this, SIGNAL(scheduleTasksChanged()));

}

void QnCameraScheduleWidget::disconnectFromGridWidget()
{
    disconnect(ui->gridWidget, SIGNAL(cellValueChanged(const QPoint &)), this, SIGNAL(scheduleTasksChanged()));
}

void QnCameraScheduleWidget::beginUpdate() {
    m_inUpdate++;
    if (m_inUpdate > 1)
        return;
    disconnectFromGridWidget();

}

void QnCameraScheduleWidget::endUpdate() {
    m_inUpdate--;
    if (m_inUpdate > 0)
        return;
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
    setReadOnly(ui->spinBoxMaxDays, readOnly && ui->checkBoxMaxArchive->checkState() == Qt::Checked);
    setReadOnly(ui->spinBoxMinDays, readOnly && ui->checkBoxMinArchive->checkState() == Qt::Checked);
    m_readOnly = readOnly;
}

const QnVirtualCameraResourceList &QnCameraScheduleWidget::cameras() const {
    return m_cameras;
}

void QnCameraScheduleWidget::setCameras(const QnVirtualCameraResourceList &cameras) {
    if(m_cameras == cameras)
        return;

    m_cameras = cameras;

    int enabledCount = 0, disabledCount = 0;
    
    int minDays = 0, maxDays = 0;
    bool mixedMinDays = false, mixedMaxDays = false;
    bool firstCamera = true;

    foreach (QnVirtualCameraResourcePtr camera, m_cameras) 
    {
        (camera->isScheduleDisabled() ? disabledCount : enabledCount)++;

        if (firstCamera) {
            minDays = camera->minDays();
            maxDays = camera->maxDays();
            firstCamera = false;
            continue;
        }
        mixedMinDays |= camera->minDays() != minDays;
        mixedMaxDays |= camera->maxDays() != maxDays;
    }

    if(enabledCount > 0 && disabledCount > 0)
        ui->enableRecordingCheckBox->setCheckState(Qt::PartiallyChecked);
    else 
        ui->enableRecordingCheckBox->setCheckState(enabledCount > 0 ? Qt::Checked : Qt::Unchecked);

    if(mixedMinDays) {
        ui->checkBoxMinArchive->setCheckState(Qt::PartiallyChecked);
    }
    else {
        ui->checkBoxMinArchive->setCheckState(minDays > 0 ? Qt::Checked : Qt::Unchecked);
        ui->spinBoxMinDays->setValue(qAbs(minDays));
    }

    if(mixedMaxDays) {
        ui->checkBoxMaxArchive->setCheckState(Qt::PartiallyChecked);
    }
    else {
        ui->checkBoxMaxArchive->setCheckState(maxDays > 0 ? Qt::Checked : Qt::Unchecked);
        ui->spinBoxMaxDays->setValue(qAbs(maxDays));
    }

    updatePanicLabelText();
    updateMotionButtons();
    updateLicensesLabelText();
}

void QnCameraScheduleWidget::setContext(QnWorkbenchContext *context) {
    m_context = context;

    if(context) {
        connect(context->instance<QnWorkbenchPanicWatcher>(), SIGNAL(panicModeChanged()), this, SLOT(updatePanicLabelText()));
        connect(context, SIGNAL(userChanged(const QnUserResourcePtr &)), this, SLOT(updateLicensesButtonVisible()));
        updatePanicLabelText();
        updateLicensesButtonVisible();
    }
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
    if (m_inUpdate > 0)
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

    m_maxFps = value;
    m_maxDualStreamingFps = dualStreamValue;

    int currentMaxFps = getGridMaxFps();
    int currentMaxDualStreamingFps = getGridMaxFps(true);
    if (currentMaxFps > value)
    {
        QMessageBox::warning(this, tr("FPS value is too high"),
            tr("Current fps in schedule grid is %1. Fps was dropped down to maximum camera fps %2.").arg(currentMaxFps).arg(value));
        emit scheduleTasksChanged();
    }
    if (currentMaxDualStreamingFps > dualStreamValue)
    {
        QMessageBox::warning(this, tr("FPS value is too high"),
            tr("For software motion 2 fps is reserved for secondary stream. Current fps in schedule grid is %1. Fps was dropped down to %2.")
                             .arg(currentMaxDualStreamingFps).arg(dualStreamValue));
        emit scheduleTasksChanged();
    }

    updateMaxFpsValue(ui->recordMotionPlusLQButton->isChecked());
    ui->gridWidget->setMaxFps(value, dualStreamValue);
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
}

bool QnCameraScheduleWidget::isMotionAvailable() const
{
    return m_motionAvailable;
}

bool QnCameraScheduleWidget::isRecordingParamsAvailable() const
{
    return m_recordingParamsAvailable;
}


void QnCameraScheduleWidget::updateDaysEnabledState()
{
    bool isEnabled = ui->enableRecordingCheckBox->checkState() == Qt::Checked;
    ui->spinBoxMaxDays->setEnabled(isEnabled && ui->checkBoxMaxArchive->checkState() == Qt::Unchecked);
    ui->spinBoxMinDays->setEnabled(isEnabled && ui->checkBoxMinArchive->checkState() == Qt::Unchecked);
}

void QnCameraScheduleWidget::updateGridEnabledState()
{
    bool enabled = ui->enableRecordingCheckBox->checkState() == Qt::Checked;

    ui->scheduleGridGroupBox->setEnabled(enabled);
    ui->settingsGroupBox->setEnabled(enabled);
    ui->motionGroupBox->setEnabled(enabled && m_recordingParamsAvailable);
    ui->gridWidget->setEnabled(enabled && !m_changesDisabled);

    ui->checkBoxMinArchive->setEnabled(enabled);
    ui->checkBoxMaxArchive->setEnabled(enabled);
    updateDaysEnabledState();
}

void QnCameraScheduleWidget::updateLicensesLabelText()
{
    QnLicenseUsageHelper helper;

    int usedDigitalChange = helper.usedDigital();
    int usedAnalogChange = helper.usedAnalog();

    switch(ui->enableRecordingCheckBox->checkState()) {
    case Qt::Checked:
        helper.propose(m_cameras, true);
        break;
    case Qt::Unchecked:
        helper.propose(m_cameras, false);
        break;
    default:
        break;
    }

    usedDigitalChange = helper.usedDigital() - usedDigitalChange;
    usedAnalogChange = helper.usedAnalog() - usedAnalogChange;

    { // digital licenses
        QString usageText = tr("%n license(s) are used out of %1.", "", helper.usedDigital()).arg(helper.totalDigital());
        ui->digitalLicensesLabel->setText(usageText);
        QPalette palette = this->palette();
        if (!helper.isValid() && helper.required() > 0)
            setWarningStyle(&palette);
        ui->digitalLicensesLabel->setPalette(palette);
    }

    { // analog licenses
        QString usageText = tr("%n analog license(s) are used out of %1.", "", helper.usedAnalog()).arg(helper.totalAnalog());
        ui->analogLicensesLabel->setText(usageText);
        QPalette palette = this->palette();
        if (!helper.isValid() && helper.required() > 0)
            setWarningStyle(&palette);
        ui->analogLicensesLabel->setPalette(palette);
        ui->analogLicensesLabel->setVisible(helper.totalAnalog() > 0);
    }

    if (ui->enableRecordingCheckBox->checkState() != Qt::Checked) {
        ui->requiredLicensesLabel->setVisible(false);
        return;
    }

    { // required licenses
        QPalette palette = this->palette();
        if (!helper.isValid())
            setWarningStyle(&palette);
        ui->requiredLicensesLabel->setPalette(palette);
        ui->requiredLicensesLabel->setVisible(true);
    }

    if (helper.required() > 0) {
        ui->requiredLicensesLabel->setText(tr("Activate %n more license(s).", "", helper.required()));
    } else if (usedDigitalChange > 0 && usedAnalogChange > 0) {
        ui->requiredLicensesLabel->setText(tr("%1 more licenses and %2 more analog licenses will be used.")
                                           .arg(usedDigitalChange)
                                           .arg(usedAnalogChange)
                                           );
    } else if (usedDigitalChange > 0) {
        ui->requiredLicensesLabel->setText(tr("%n more license(s) will be used.", "", usedDigitalChange));
    } else if (usedAnalogChange > 0) {
        ui->requiredLicensesLabel->setText(tr("%n more analog license(s) will be used.", "", usedAnalogChange));
    }
    else {
        ui->requiredLicensesLabel->setText(QString());
    }


}

void QnCameraScheduleWidget::updateLicensesButtonVisible() {
    ui->licensesButton->setVisible(context()->accessController()->globalPermissions() & Qn::GlobalProtectedPermission);
}

void QnCameraScheduleWidget::updateRecordSpinboxes(){
    bool motionEnabled = m_motionAvailable && (
                ui->recordMotionButton->isChecked() ||
                ui->recordMotionPlusLQButton->isChecked() ||
                hasMotionOnGrid()
                );
    ui->recordBeforeSpinBox->setEnabled(motionEnabled);
    ui->recordAfterSpinBox->setEnabled(motionEnabled);
}

void QnCameraScheduleWidget::updateMotionButtons() {
    bool hasDualStreaming = !m_cameras.isEmpty();
    bool hasMotion = !m_cameras.isEmpty();
    foreach(const QnVirtualCameraResourcePtr &camera, m_cameras) {
        hasDualStreaming &= camera->hasDualStreaming2();
        hasMotion &= camera->supportedMotionType() != Qn::MT_NoMotion;
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

void QnCameraScheduleWidget::updateMaxFpsValue(bool motionPlusLqToggled) {
    if (motionPlusLqToggled)
        ui->fpsSpinBox->setMaximum(m_maxDualStreamingFps);
    else
        ui->fpsSpinBox->setMaximum(m_maxFps);
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

void QnCameraScheduleWidget::at_enableRecordingCheckBox_clicked()
{
    Qt::CheckState state = ui->enableRecordingCheckBox->checkState();

    ui->enableRecordingCheckBox->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->enableRecordingCheckBox->setCheckState(Qt::Checked);
}

void QnCameraScheduleWidget::at_checkBoxArchive_clicked()
{
    QCheckBox* checkBox = (QCheckBox*) sender();
    Qt::CheckState state = checkBox->checkState();

    checkBox->setTristate(false);
    if (state == Qt::PartiallyChecked)
        checkBox->setCheckState(Qt::Checked);
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
    emit moreLicensesRequested();
}

void QnCameraScheduleWidget::at_releaseSignalizer_activated(QObject *target) {
    QWidget *widget = qobject_cast<QWidget *>(target);
    if(!widget)
        return;

    if(widget->isEnabled() || (widget->parentWidget() && !widget->parentWidget()->isEnabled()))
        return;

    // TODO: #GDM #Common duplicate code.
    bool hasDualStreaming = !m_cameras.isEmpty();
    bool hasMotion = !m_cameras.isEmpty();
    foreach(const QnVirtualCameraResourcePtr &camera, m_cameras) {
        hasDualStreaming &= camera->hasDualStreaming2();
        hasMotion &= camera->supportedMotionType() != Qn::MT_NoMotion;
    }

    if(m_cameras.size() > 1) {
        QMessageBox::warning(this, tr("Warning"), tr("Motion Recording is disabled or not supported by some of the selected cameras. Please go to the cameras' motion setup page to ensure it is supported and enabled."));
    } else {
        if (hasMotion && !hasDualStreaming) {
            QMessageBox::warning(this, tr("Warning"), tr("Dual-Streaming is not supported by this camera."));
        } else if(!hasMotion && !hasDualStreaming) {
            QMessageBox::warning(this, tr("Warning"), tr("Dual-Streaming and Motion Detection are not available for this camera."));
        } else /* Has dual streaming but not motion */ {
            QMessageBox::warning(this, tr("Warning"), tr("Motion Recording is disabled. Please go to the motion setup page to setup the camera's motion area and sensitivity."));
        }
    }
}

void QnCameraScheduleWidget::at_exportScheduleButton_clicked() {
    bool recordingEnabled = ui->enableRecordingCheckBox->checkState() == Qt::Checked;
    bool motionUsed = recordingEnabled && hasMotionOnGrid();
    bool dualStreamingUsed = motionUsed && hasDualStreamingMotionOnGrid();

    QScopedPointer<QnResourceSelectionDialog> dialog(new QnResourceSelectionDialog(this));
    auto dialogDelegate = new QnExportScheduleResourceSelectionDialogDelegate(this, recordingEnabled, motionUsed, dualStreamingUsed);
    dialog->setDelegate(dialogDelegate);
    dialog->setSelectedResources(m_cameras);
    setHelpTopic(dialog.data(), Qn::CameraSettings_Recording_Export_Help);
    if (dialog->exec() != QDialog::Accepted)
        return;

    const bool copyArchiveLength = dialogDelegate->doCopyArchiveLength();
    QnVirtualCameraResourceList cameras = dialog->selectedResources().filtered<QnVirtualCameraResource>();
    foreach(QnVirtualCameraResourcePtr camera, cameras) 
    {
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
    }
    updateLicensesLabelText();
    emit scheduleExported(cameras);
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

int QnCameraScheduleWidget::maxRecordedDays() const
{
    if (ui->checkBoxMaxArchive->checkState() == Qt::PartiallyChecked)
        return RecordedDaysDontChange;
    else
        return ui->spinBoxMaxDays->value() * (ui->checkBoxMaxArchive->isChecked() ? 1 : -1);
}

int QnCameraScheduleWidget::minRecordedDays() const
{
    if (ui->checkBoxMinArchive->checkState() == Qt::PartiallyChecked)
        return RecordedDaysDontChange;
    else
        return ui->spinBoxMinDays->value() * (ui->checkBoxMinArchive->isChecked() ? 1 : -1);
}

void QnCameraScheduleWidget::at_archiveRangeChanged()
{
    if (ui->checkBoxMinArchive->checkState() == Qt::Checked && ui->checkBoxMaxArchive->checkState() == Qt::Checked)
    {
        if (ui->spinBoxMaxDays->value() < ui->spinBoxMinDays->value())
            ui->spinBoxMaxDays->setValue(ui->spinBoxMinDays->value());
    }
    ui->minArchiveDaysWarningLabel->setVisible(ui->checkBoxMinArchive->isChecked() && ui->spinBoxMinDays->value() > dangerousMinArchiveValue);
}
