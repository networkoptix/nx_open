#include "camera_schedule_widget.h"
#include "ui_camera_schedule_widget.h"

#include <core/resourcemanagment/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/video_server_resource.h>
#include <licensing/license.h>

#include <ui/style/globals.h>
#include <ui/common/color_transformations.h>
#include <ui/common/read_only.h>

QnCameraScheduleWidget::QnCameraScheduleWidget(QWidget *parent):
    QWidget(parent), 
    ui(new Ui::CameraScheduleWidget),
    m_disableUpdateGridParams(false),
    m_changesDisabled(false),
    m_readOnly(false),
    m_motionAvailable(true)
{
    ui->setupUi(this);

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

    connect(ui->recordAlwaysButton,      SIGNAL(toggled(bool)),              this,   SLOT(updateGridParams()));
    connect(ui->recordMotionButton,      SIGNAL(toggled(bool)),              this,   SLOT(updateGridParams()));
    connect(ui->recordMotionPlusLQButton,SIGNAL(toggled(bool)),              this,   SLOT(updateGridParams()));
    connect(ui->noRecordButton,          SIGNAL(toggled(bool)),              this,   SLOT(updateGridParams()));
    connect(ui->qualityComboBox,         SIGNAL(currentIndexChanged(int)),   this,   SLOT(updateGridParams()));
    connect(ui->fpsSpinBox,              SIGNAL(valueChanged(double)),       this,   SLOT(updateGridParams()));

    connect(ui->recordBeforeSpinBox,    SIGNAL(valueChanged(int)),          this,   SIGNAL(scheduleTasksChanged()));
    connect(ui->recordAfterSpinBox,     SIGNAL(valueChanged(int)),          this,   SIGNAL(scheduleTasksChanged()));
    
    connect(ui->licensesButton,         SIGNAL(clicked()),                  this,   SLOT(at_licensesButton_clicked()));
    connect(ui->displayQualityCheckBox, SIGNAL(stateChanged(int)),          this,   SLOT(at_displayQualiteCheckBox_stateChanged(int)));
    connect(ui->displayFpsCheckBox,     SIGNAL(stateChanged(int)),          this,   SLOT(at_displayFpsCheckBox_stateChanged(int)));
    connect(ui->enableRecordingCheckBox,SIGNAL(clicked()),                  this,   SLOT(at_enableRecordingCheckBox_clicked()));
    connect(ui->enableRecordingCheckBox,SIGNAL(stateChanged(int)),          this,   SLOT(updateGridEnabledState()));
    connect(ui->enableRecordingCheckBox,SIGNAL(stateChanged(int)),          this,   SIGNAL(scheduleEnabledChanged()));
    connect(ui->enableRecordingCheckBox,SIGNAL(stateChanged(int)),          this,   SLOT(updateLicensesLabelText()), Qt::QueuedConnection);
    connect(qnLicensePool,              SIGNAL(licensesChanged()),          this,   SLOT(updateLicensesLabelText()), Qt::QueuedConnection);

    connect(ui->gridWidget,             SIGNAL(cellActivated(QPoint)),      this,   SLOT(at_gridWidget_cellActivated(QPoint)));
    
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

    ui->recordingModeLabel->setText(tr("Normal"));
    if(enabledCount > 0) {
        QnVideoServerResourcePtr server = qnResPool->getResourceById(m_cameras[0]->getParentId()).dynamicCast<QnVideoServerResource>();
        if(server && server->isPanicMode()) {
            QPalette palette = this->palette();
            palette.setColor(QPalette::WindowText, QColor(255, 0, 0));
            ui->recordingModeLabel->setPalette(palette);
            ui->recordingModeLabel->setText(tr("Panic"));
        }
    }

    updateMotionButtons();
    updateLicensesLabelText();
}

QList<QnScheduleTask::Data> QnCameraScheduleWidget::scheduleTasks() const
{
    QList<QnScheduleTask::Data> tasks;
    for (int row = 0; row < ui->gridWidget->rowCount(); ++row) {
        QnScheduleTask::Data task;

        for (int col = 0; col < ui->gridWidget->columnCount();) {
            const QPoint cell(col, row);

            QnScheduleTask::RecordingType recordType = QnScheduleTask::RecordingType_Run;
            {
                QColor color(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::ColorParam).toUInt());
                if (color == ui->recordAlwaysButton->color())
                    recordType = QnScheduleTask::RecordingType_Run;
                else if (color == ui->recordMotionButton->color())
                    recordType = QnScheduleTask::RecordingType_MotionOnly;
                else if (color == ui->recordMotionPlusLQButton->color())
                    recordType = QnScheduleTask::RecordingType_MotionPlusLQ;
                else if (color == ui->noRecordButton->color())
                    recordType = QnScheduleTask::RecordingType_Never;
                else
                    qWarning("ColorParam wasn't acknowledged. fallback to 'Always'");
            }
            QnStreamQuality streamQuality = QnQualityHighest;
            {
                QString shortQuality(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::SecondParam).toString()); // TODO: Oh crap. This string-switching is totally evil.
                if (shortQuality == QLatin1String("Lo"))
                    streamQuality = QnQualityLow;
                else if (shortQuality == QLatin1String("Md"))
                    streamQuality = QnQualityNormal;
                else if (shortQuality == QLatin1String("Hi"))
                    streamQuality = QnQualityHigh;
                else if (shortQuality == QLatin1String("Bst"))
                    streamQuality = QnQualityHighest;
                else
                    qWarning("SecondParam wasn't acknowledged. fallback to 'Highest'");
            }
            int fps = fps = ui->gridWidget->cellValue(cell, QnScheduleGridWidget::FirstParam).toInt();
            if (fps == 0 && recordType != QnScheduleTask::RecordingType_Never)
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
            tasks.append(QnScheduleTask::Data(nDay, 0, 86400, QnScheduleTask::RecordingType_Never, 10, 10));
    }

    foreach (const QnScheduleTask::Data &task, tasks) {
        const int row = task.m_dayOfWeek - 1;

        QColor color, colorInside;
        switch (task.m_recordType) {
        case QnScheduleTask::RecordingType_Run: colorInside = color = ui->recordAlwaysButton->color(); break;
        case QnScheduleTask::RecordingType_MotionOnly: colorInside = color = ui->recordMotionButton->color(); break;
        case QnScheduleTask::RecordingType_MotionPlusLQ: 
            color = ui->recordMotionPlusLQButton->color(); 
            colorInside = ui->recordAlwaysButton->color();
            break;
        case QnScheduleTask::RecordingType_Never: colorInside = color = ui->noRecordButton->color(); break;
        default:
            qWarning("QnCameraScheduleWidget::setScheduleTasks(): Unhandled RecordingType value %d", task.m_recordType);
            break;
        }

        QString shortQuality = QLatin1String("-");
        if (task.m_recordType != QnScheduleTask::RecordingType_Never)
        {
            switch (task.m_streamQuality) 
            {
                case QnQualityLow: shortQuality = QLatin1String("Lo"); break;
                case QnQualityNormal: shortQuality = QLatin1String("Md"); break;
                case QnQualityHigh: shortQuality = QLatin1String("Hi"); break;
                case QnQualityHighest: shortQuality = QLatin1String("Bst"); break;
                default:
                    qWarning("QnCameraScheduleWidget::setScheduleTasks(): Unhandled StreamQuality value %d.", task.m_streamQuality);
                    break;
            }
        }

        //int fps = task.m_fps;
        QString fps = QLatin1String("-");
        if (task.m_recordType != QnScheduleTask::RecordingType_Never)
            fps = QString::number(task.m_fps);

        for (int col = task.m_startTime / 3600; col < task.m_endTime / 3600; ++col) {
            const QPoint cell(col, row);

            ui->gridWidget->setCellValue(cell, QnScheduleGridWidget::ColorParam, color.rgba());
            ui->gridWidget->setCellValue(cell, QnScheduleGridWidget::ColorInsideParam, colorInside.rgba());
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
        return QLatin1String("Md");
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
    if (text == QLatin1String("Md"))
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
    if (m_disableUpdateGridParams)
        return;

    QColor color;
    if (ui->recordAlwaysButton->isChecked())
        color = ui->recordAlwaysButton->color();
    else if (ui->recordMotionButton->isChecked())
        color = ui->recordMotionButton->color();
    else if (ui->noRecordButton->isChecked())
        color = ui->noRecordButton->color();
    else if (ui->recordMotionPlusLQButton->isChecked())
        color = ui->recordMotionPlusLQButton->color();

    bool enabled = !ui->noRecordButton->isChecked();
    bool motionEnabled = ui->recordMotionButton->isChecked() || ui->recordMotionPlusLQButton->isChecked();

    ui->fpsSpinBox->setEnabled(enabled);
    ui->qualityComboBox->setEnabled(enabled);
    ui->recordBeforeSpinBox->setEnabled(motionEnabled);
    ui->recordAfterSpinBox->setEnabled(motionEnabled);

    if(!(m_readOnly && fromUserInput)) {
        ui->gridWidget->setDefaultParam(QnScheduleGridWidget::ColorParam, color.rgba());
        if (ui->recordMotionPlusLQButton->isChecked())
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::ColorInsideParam, ui->recordAlwaysButton->color().rgba());
        else
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::ColorInsideParam, color.rgba());
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

int QnCameraScheduleWidget::getMaxFps() const
{
    return ui->fpsSpinBox->maximum();
}

void QnCameraScheduleWidget::setMaxFps(int value)
{
    if(value < ui->fpsSpinBox->minimum())
        value = ui->fpsSpinBox->minimum(); /* Silently ignoring invalid input is OK here. */
    ui->fpsSpinBox->setMaximum(value);
    ui->gridWidget->setMaxFps(value);
}

int QnCameraScheduleWidget::getGridMaxFps()
{
    return ui->gridWidget->getMaxFps();
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

    int toBeUsed = qnResPool->activeCameras() + m_cameras.size() - alreadyActive;
    int used = qnResPool->activeCameras() + activeCameraCount() - alreadyActive;
    int total = qnLicensePool->getLicenses().totalCameras();

    QPalette palette = this->palette();
    if(toBeUsed > total)
        palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->licensesLabel->setPalette(palette);

    if(ui->enableRecordingCheckBox->checkState() == Qt::Checked) {
        ui->licensesLabel->setText(tr("%n license(s) are used out of %1.", NULL, used).arg(total));
    } else {
        ui->licensesLabel->setText(
            tr("%1. %2.").
                arg(tr("Requires %n license(s)", NULL, m_cameras.size() - alreadyActive)).
                arg(tr("%n license(s) are used out of %1", NULL, used).arg(total))
        );
    }
}

void QnCameraScheduleWidget::updateMotionButtons() {
    bool hasDualStreaming = !m_cameras.isEmpty();
    bool hasMotion = !m_cameras.isEmpty();
    foreach(const QnVirtualCameraResourcePtr &camera, m_cameras) {
        hasDualStreaming &= camera->hasDualStreaming();
        hasMotion &= camera->supportedMotionType() != MT_NoMotion;
    }

    ui->recordMotionButton->setEnabled(m_motionAvailable && hasMotion);
    ui->labelMotionOnly->setEnabled(ui->recordMotionButton->isEnabled());
    ui->recordMotionPlusLQButton->setEnabled(m_motionAvailable && hasDualStreaming && hasMotion);
    ui->labelMotionPlusLQ->setEnabled(ui->recordMotionPlusLQButton->isEnabled());

    if(ui->recordMotionButton->isChecked() && !ui->recordMotionButton->isEnabled())
        ui->recordAlwaysButton->setChecked(true);
    if(ui->recordMotionPlusLQButton->isChecked() && !ui->recordMotionPlusLQButton->isEnabled())
        ui->recordAlwaysButton->setChecked(true);

    if(!m_motionAvailable) {
        for (int row = 0; row < ui->gridWidget->rowCount(); ++row) {
            for (int col = 0; col < ui->gridWidget->columnCount(); ++col) {
                const QPoint cell(col, row);

                // TODO: swordsmanship skills must be used here.
                QnScheduleTask::RecordingType recordType = QnScheduleTask::RecordingType_Run;
                {
                    QColor color(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::ColorParam).toUInt());
                    if (color == ui->recordAlwaysButton->color())
                        recordType = QnScheduleTask::RecordingType_Run;
                    else if (color == ui->recordMotionButton->color())
                        recordType = QnScheduleTask::RecordingType_MotionOnly;
                    else if (color == ui->recordMotionPlusLQButton->color())
                        recordType = QnScheduleTask::RecordingType_MotionPlusLQ;
                    else if (color == ui->noRecordButton->color())
                        recordType = QnScheduleTask::RecordingType_Never;
                    else
                        qWarning("ColorParam wasn't acknowledged. fallback to 'Always'");
                }

                if(recordType == QnScheduleTask::RecordingType_MotionOnly || recordType == QnScheduleTask::RecordingType_MotionPlusLQ) {
                    ui->gridWidget->setCellValue(cell, QnScheduleGridWidget::ColorParam, ui->recordAlwaysButton->color().rgba());
                    ui->gridWidget->setCellValue(cell, QnScheduleGridWidget::ColorInsideParam, ui->recordAlwaysButton->color().rgba());
                }
            }
        }
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCameraScheduleWidget::at_gridWidget_cellActivated(const QPoint &cell)
{
    m_disableUpdateGridParams = true;
    QColor color(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::ColorParam).toUInt());
    double fps(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::FirstParam).toDouble());
    QString shortQuality(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::SecondParam).toString());

    if (color == ui->recordAlwaysButton->color())
        ui->recordAlwaysButton->setChecked(true);
    else if (color == ui->recordMotionButton->color())
        ui->recordMotionButton->setChecked(true);
    else if (color == ui->recordMotionPlusLQButton->color())
        ui->recordMotionPlusLQButton->setChecked(true);
    else if (color == ui->noRecordButton->color())
        ui->noRecordButton->setChecked(true);
    if (color != ui->noRecordButton->color())
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
