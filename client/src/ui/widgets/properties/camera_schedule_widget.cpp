#include "camera_schedule_widget.h"
#include "ui_camera_schedule_widget.h"

#include <QtGui/QMessageBox>

//TODO: #gdm ask #elric about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <licensing/license.h>

#include <ui/common/color_transformations.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/export_camera_settings_dialog.h>
#include <ui/style/globals.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/event_processors.h>

QnCameraScheduleWidget::QnCameraScheduleWidget(QWidget *parent):
    QWidget(parent), 
    ui(new Ui::CameraScheduleWidget),
    m_disableUpdateGridParams(false),
    m_motionAvailable(true),
    m_changesDisabled(false),
    m_readOnly(false),
    m_maxFps(0),
    m_maxDualStreamingFps(0),
    m_inUpdate(0)
{
    ui->setupUi(this);

    setHelpTopic(ui->exportScheduleButton, Qn::CameraSettings_Recording_Export_Help);

    // init buttons
    ui->recordAlwaysButton->setColor(qnGlobals->recordAlwaysColor());
    ui->recordAlwaysButton->setCheckedColor(shiftColor(qnGlobals->recordAlwaysColor(), SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA));

    ui->recordMotionButton->setColor(qnGlobals->recordMotionColor());
    ui->recordMotionButton->setCheckedColor(shiftColor(qnGlobals->recordMotionColor(), SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA));

    ui->recordMotionPlusLQButton->setColor(shiftColor(qnGlobals->recordMotionColor(),0,1,0));
    ui->recordMotionPlusLQButton->setCheckedColor(shiftColor(qnGlobals->recordMotionColor(), SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA));
    ui->recordMotionPlusLQButton->setInsideColor(qnGlobals->recordAlwaysColor()); // inside color

    ui->noRecordButton->setColor(qnGlobals->noRecordColor());
    ui->noRecordButton->setCheckedColor(shiftColor(qnGlobals->noRecordColor(), SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA));

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
    connect(ui->enableRecordingCheckBox,SIGNAL(stateChanged(int)),          this,   SLOT(updateGridEnabledState()));
    connect(ui->enableRecordingCheckBox,SIGNAL(stateChanged(int)),          this,   SIGNAL(scheduleEnabledChanged()));
    connect(ui->enableRecordingCheckBox,SIGNAL(stateChanged(int)),          this,   SLOT(updateLicensesLabelText()), Qt::QueuedConnection);
    connect(qnLicensePool,              SIGNAL(licensesChanged()),          this,   SLOT(updateLicensesLabelText()), Qt::QueuedConnection);

    connect(ui->gridWidget,             SIGNAL(cellActivated(QPoint)),      this,   SLOT(at_gridWidget_cellActivated(QPoint)));

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
    setReadOnly(ui->recordMotionButton, readOnly); // TODO: this is not valid. Camera may not support HW motion, we need to check for this.
    setReadOnly(ui->recordMotionPlusLQButton, readOnly);
    setReadOnly(ui->noRecordButton, readOnly);
    setReadOnly(ui->qualityComboBox, readOnly);
    setReadOnly(ui->fpsSpinBox, readOnly);
    setReadOnly(ui->enableRecordingCheckBox, readOnly);
    setReadOnly(ui->gridWidget, readOnly);
    setReadOnly(ui->exportScheduleButton, readOnly);
    setReadOnly(ui->exportWarningLabel, readOnly);
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
    foreach (QnVirtualCameraResourcePtr camera, m_cameras)
        (camera->isScheduleDisabled() ? disabledCount : enabledCount)++;

    if(enabledCount > 0 && disabledCount > 0) {
        ui->enableRecordingCheckBox->setCheckState(Qt::PartiallyChecked);
    } else {
        ui->enableRecordingCheckBox->setCheckState(enabledCount > 0 ? Qt::Checked : Qt::Unchecked);
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
            QnStreamQuality streamQuality = QnQualityHighest;
            if (recordType != Qn::RecordingType_Never)
            {
                QString shortQuality(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::SecondParam).toString()); // TODO: Oh crap. This string-switching is totally evil.
                if (shortQuality == QLatin1String("Lo"))
                    streamQuality = QnQualityLow;
                else if (shortQuality == QLatin1String("Me"))
                    streamQuality = QnQualityNormal;
                else if (shortQuality == QLatin1String("Hi"))
                    streamQuality = QnQualityHigh;
                else if (shortQuality == QLatin1String("Bst"))
                    streamQuality = QnQualityHighest;
                else
                    qWarning("SecondParam wasn't acknowledged. fallback to 'Highest'");
            }
            int fps = ui->gridWidget->cellValue(cell, QnScheduleGridWidget::FirstParam).toInt();
            if (fps == 0 && recordType != Qn::RecordingType_Never)
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

    if (!tasks.isEmpty()) {
        const QnScheduleTask::Data &task = tasks.first();

        ui->recordBeforeSpinBox->setValue(task.m_beforeThreshold);
        ui->recordAfterSpinBox->setValue(task.m_afterThreshold);
    } else {
        for (int nDay = 1; nDay <= 7; ++nDay)
            tasks.append(QnScheduleTask::Data(nDay, 0, 86400, Qn::RecordingType_Never, 10, 10));
    }

    foreach (const QnScheduleTask::Data &task, tasks) {
        const int row = task.m_dayOfWeek - 1;
        QString shortQuality = QLatin1String("-");
        if (task.m_recordType != Qn::RecordingType_Never)
        {
            switch (task.m_streamQuality) 
            {
                case QnQualityLow: shortQuality = QLatin1String("Lo"); break;
                case QnQualityNormal: shortQuality = QLatin1String("Me"); break;
                case QnQualityHigh: shortQuality = QLatin1String("Hi"); break;
                case QnQualityHighest: shortQuality = QLatin1String("Bst"); break;
                default:
                    qWarning("QnCameraScheduleWidget::setScheduleTasks(): Unhandled StreamQuality value %d.", task.m_streamQuality);
                    break;
            }
        }

        //int fps = task.m_fps;
        QString fps = QLatin1String("-");
        if (task.m_recordType != Qn::RecordingType_Never)
            fps = QString::number(task.m_fps);

        for (int col = task.m_startTime / 3600; col < task.m_endTime / 3600; ++col) {
            const QPoint cell(col, row);

            ui->gridWidget->setCellRecordingType(cell, task.m_recordType);
            ui->gridWidget->setCellValue(cell, QnScheduleGridWidget::SecondParam, shortQuality);
            ui->gridWidget->setCellValue(cell, QnScheduleGridWidget::FirstParam, fps);
        }
    }

    connectToGridWidget();

    emit scheduleTasksChanged();
}

static inline QString getShortText(const QString &text)
{
    if (text == QLatin1String("Low"))
        return QLatin1String("Lo");
    if (text == QLatin1String("Medium"))
        return QLatin1String("Me");
    if (text == QLatin1String("High"))
        return QLatin1String("Hi");
    if (text == QLatin1String("Best"))
        return QLatin1String("Bst");
    return QLatin1String("-");
}

static inline QString getLongText(const QString &text)
{
    if (text == QLatin1String("Lo"))
        return QLatin1String("Low");
    if (text == QLatin1String("Me"))
        return QLatin1String("Medium");
    if (text == QLatin1String("Hi"))
        return QLatin1String("High");
    if (text == QLatin1String("Bst"))
        return QLatin1String("Best");
    return QLatin1String("-");
}

int QnCameraScheduleWidget::qualityTextToIndex(const QString &text)
{
    for (int i = 0; i < ui->qualityComboBox->count(); ++i)
    {
        if (ui->qualityComboBox->itemText(i) == text)
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

    Qn::RecordingType recordType = Qn::RecordingType_Never;
    if (ui->recordAlwaysButton->isChecked())
        recordType = Qn::RecordingType_Run;
    else if (ui->recordMotionButton->isChecked())
        recordType = Qn::RecordingType_MotionOnly;
    else if (ui->noRecordButton->isChecked())
        recordType = Qn::RecordingType_Never;
    else if (ui->recordMotionPlusLQButton->isChecked())
        recordType = Qn::RecordingType_MotionPlusLQ;
    else
        qWarning() << "QnCameraScheduleWidget::No record type is selected!";

    bool enabled = !ui->noRecordButton->isChecked();
    ui->fpsSpinBox->setEnabled(enabled);
    ui->qualityComboBox->setEnabled(enabled);
    updateRecordSpinboxes();


    if(!(m_readOnly && fromUserInput)) {
        ui->gridWidget->setDefaultParam(QnScheduleGridWidget::RecordTypeParam, recordType);
        if (ui->noRecordButton->isChecked())
        {
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::FirstParam, QLatin1String("-"));
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::SecondParam, QLatin1String("-"));
        }
        else
        {
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::FirstParam, QString::number(ui->fpsSpinBox->value()));
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::SecondParam, getShortText(ui->qualityComboBox->currentText()));
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
            tr("Current fps in schedule grid is %1. Fps was dropped down to maximum camera fps %2").arg(currentMaxFps).arg(value));
    }
    if (currentMaxDualStreamingFps > dualStreamValue)
    {
        QMessageBox::warning(this, tr("FPS value is too high"),
            tr("For software motion 2 fps is reserved for secondary stream. Current fps in schedule grid is %1. Fps was dropped down to %2")
                             .arg(currentMaxDualStreamingFps).arg(dualStreamValue));
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

int QnCameraScheduleWidget::activeCameraCount() const 
{
    switch(ui->enableRecordingCheckBox->checkState()) {
    case Qt::Checked:
        return m_cameras.size();
    case Qt::Unchecked:
        return 0;
    case Qt::PartiallyChecked: {
        int result = 0;
        foreach (const QnVirtualCameraResourcePtr &camera, m_cameras)
            if (!camera->isScheduleDisabled())
                result++;
        return result;
    }
    default:
        return 0;
    }
}

void QnCameraScheduleWidget::setMotionAvailable(bool available) {
    if(m_motionAvailable == available)
        return;

    m_motionAvailable = available;

    updateMotionButtons();
}

bool QnCameraScheduleWidget::isMotionAvailable() {
    return m_motionAvailable;
}


void QnCameraScheduleWidget::updateGridEnabledState() 
{
    bool enabled = ui->enableRecordingCheckBox->checkState() == Qt::Checked;

    ui->scheduleGridGroupBox->setEnabled(enabled);
    ui->settingsGroupBox->setEnabled(enabled);
    ui->motionGroupBox->setEnabled(enabled);
    ui->gridWidget->setEnabled(enabled && !m_changesDisabled);
}

void QnCameraScheduleWidget::updateLicensesLabelText() 
{
    int alreadyActive = 0;
    foreach (const QnVirtualCameraResourcePtr &camera, m_cameras)
        if (!camera->isScheduleDisabled())
            alreadyActive++;

    // how many licenses will be used if recording will be enabled on all cameras
    int toBeUsed = qnResPool->activeCameras() + m_cameras.size() - alreadyActive;

    // how many licensed is used really
    int used = qnResPool->activeCameras() + activeCameraCount() - alreadyActive;

    // how many licenses do we have
    int total = qnLicensePool->getLicenses().totalCameras();

    QPalette palette = this->palette();
    if(used > total)
        palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->licensesLabel->setPalette(palette);

    QString usageText = tr("%n license(s) are used out of %1.", NULL, used).arg(total);

    if (ui->enableRecordingCheckBox->checkState() == Qt::Checked) {
        ui->licensesLabel->setText(usageText);
    } else if (toBeUsed > total){
        ui->licensesLabel->setText(
            QString(QLatin1String("%1 %2")).
                arg(tr("Activate %n more license(s).", NULL, toBeUsed - total)).
                arg(usageText)
        );
    } else {
        ui->licensesLabel->setText(
            QString(QLatin1String("%1 %2")).
                arg(tr("%n license(s) will be used.", NULL, m_cameras.size() - alreadyActive)).
                arg(usageText)
        );
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
        hasDualStreaming &= camera->hasDualStreaming();
        hasMotion &= camera->supportedMotionType() != MT_NoMotion;
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
                if(recordType == Qn::RecordingType_MotionOnly || recordType == Qn::RecordingType_MotionPlusLQ)
                    ui->gridWidget->setCellRecordingType(cell, Qn::RecordingType_Run);
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
    double fps(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::FirstParam).toDouble());
    QString shortQuality(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::SecondParam).toString());

    switch (recordType) {
        case Qn::RecordingType_Run:
            ui->recordAlwaysButton->setChecked(true);
            break;
        case Qn::RecordingType_MotionOnly:
            ui->recordMotionButton->setChecked(true);
            break;
        case Qn::RecordingType_MotionPlusLQ:
            ui->recordMotionPlusLQButton->setChecked(true);
            break;
        default:
            ui->noRecordButton->setChecked(true);
            break;
    }

    if (recordType != Qn::RecordingType_Never)
    {
        ui->fpsSpinBox->setValue(fps);
        ui->qualityComboBox->setCurrentIndex(qualityTextToIndex(getLongText(shortQuality)));
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

void QnCameraScheduleWidget::at_displayQualiteCheckBox_stateChanged(int state)
{
    ui->gridWidget->setShowSecondParam(state);
}

void QnCameraScheduleWidget::at_displayFpsCheckBox_stateChanged(int state)
{
    ui->gridWidget->setShowFirstParam(state);
}

void QnCameraScheduleWidget::at_licensesButton_clicked() 
{
    emit moreLicensesRequested();
}

bool QnCameraScheduleWidget::isSecondaryStreamReserver() const
{
    return ui->recordMotionPlusLQButton->isChecked();
}

void QnCameraScheduleWidget::at_releaseSignalizer_activated(QObject *target) {
    QWidget *widget = qobject_cast<QWidget *>(target);
    if(!widget)
        return;

    if(widget->isEnabled() || (widget->parentWidget() && !widget->parentWidget()->isEnabled()))
        return;

    // TODO: duplicate code.
    bool hasDualStreaming = !m_cameras.isEmpty();
    bool hasMotion = !m_cameras.isEmpty();
    foreach(const QnVirtualCameraResourcePtr &camera, m_cameras) {
        hasDualStreaming &= camera->hasDualStreaming();
        hasMotion &= camera->supportedMotionType() != MT_NoMotion;
    }

    if(m_cameras.size() > 1) {
        QMessageBox::warning(this, tr("Warning"), tr("Motion Recording is disabled or not supported by some of the selected cameras. Please go to the cameras' motion setup page to ensure it is supported and enabled."));
    } else {
        if(!(hasMotion && hasDualStreaming)) {
            QMessageBox::warning(this, tr("Warning"), tr("Dual-Streaming and Motion Detection are not available for this camera."));
        } else {
            QMessageBox::warning(this, tr("Warning"), tr("Motion Recording is disabled. Please go to the motion setup page to setup the camera's motion area and sensitivity."));
        }
    }
}

void QnCameraScheduleWidget::at_exportScheduleButton_clicked() {
    bool recordingEnabled = ui->enableRecordingCheckBox->checkState() == Qt::Checked;
    QnExportCameraSettingsDialog dialog(NULL, context());
    dialog.setRecordingEnabled(recordingEnabled);

    bool motionUsed = recordingEnabled && hasMotionOnGrid();
    bool dualStreamingUsed = motionUsed && hasDualStreamingMotionOnGrid();

    dialog.setMotionParams(motionUsed, dualStreamingUsed);

    if (dialog.exec() == QDialog::Rejected)
        return;

    QnVirtualCameraResourceList cameras = dialog.getSelectedCameras();
    foreach(QnVirtualCameraResourcePtr camera, cameras) {
        camera->setScheduleDisabled(!recordingEnabled);
        if (recordingEnabled){
            int maxFps = camera->getMaxFps();

            //TODO: #gdm ask #elric about constant MIN_SECOND_STREAM_FPS moving out of the live_stream_provider module
            // or just use camera->reservedSecondStreamFps();

            int decreaseAlways = 0;
            if (camera->streamFpsSharingMethod() == Qn::shareFps && camera->getMotionType() == MT_SoftwareGrid)
                decreaseAlways = MIN_SECOND_STREAM_FPS;

            int decreaseIfMotionPlusLQ = 0;
            if (camera->streamFpsSharingMethod() == Qn::shareFps)
                decreaseIfMotionPlusLQ = MIN_SECOND_STREAM_FPS;

            QnScheduleTaskList tasks;
            foreach(const QnScheduleTask::Data &data, scheduleTasks()){
                QnScheduleTask task(data);
                if (task.getRecordingType() == Qn::RecordingType_MotionPlusLQ)
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
            if (recordType == Qn::RecordingType_MotionOnly || recordType == Qn::RecordingType_MotionPlusLQ)
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
            if(recordType == Qn::RecordingType_MotionPlusLQ)
                return true;
        }
    }
    return false;
}
