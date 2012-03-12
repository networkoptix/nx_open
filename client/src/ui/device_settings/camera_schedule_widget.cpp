#include "camera_schedule_widget.h"
#include "ui_camera_schedule_widget.h"
#include "ui/style/globals.h"
#include "ui/common/color_transform.h"
#include "ui/common/read_only.h"

QnCameraScheduleWidget::QnCameraScheduleWidget(QWidget *parent):
    QWidget(parent), 
    ui(new Ui::CameraScheduleWidget),
    m_disableUpdateGridParams(false),
    m_changesDisabled(false),
    m_readOnly(false)
{
    ui->setupUi(this);

    // init buttons
    ui->btnRecordAlways->setColor(qnGlobals->recordAlwaysColor());
    ui->btnRecordAlways->setCheckedColor(shiftColor(qnGlobals->recordAlwaysColor(), SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA));

    ui->btnRecordMotion->setColor(qnGlobals->recordMotionColor());
    ui->btnRecordMotion->setCheckedColor(shiftColor(qnGlobals->recordMotionColor(), SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA));

    ui->btnNoRecord->setColor(qnGlobals->noRecordColor());
    ui->btnNoRecord->setCheckedColor(shiftColor(qnGlobals->noRecordColor(), SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA));

    connect(ui->btnRecordAlways,        SIGNAL(toggled(bool)),              this,   SLOT(updateGridParams()));
    connect(ui->btnRecordMotion,        SIGNAL(toggled(bool)),              this,   SLOT(updateGridParams()));
    connect(ui->btnNoRecord,            SIGNAL(toggled(bool)),              this,   SLOT(updateGridParams()));
    connect(ui->comboBoxQuality,        SIGNAL(currentIndexChanged(int)),   this,   SLOT(updateGridParams()));
    connect(ui->fpsSpinBox,             SIGNAL(valueChanged(double)),       this,   SLOT(updateGridParams()));

    connect(ui->chkBoxDisplayQuality,   SIGNAL(stateChanged(int)),          this,   SLOT(onDisplayQualityChanged(int)));
    connect(ui->chkBoxDisplayFPS,       SIGNAL(stateChanged(int)),          this,   SLOT(onDisplayFPSChanged(int)));
    connect(ui->chkBoxEnableSchedule,   SIGNAL(clicked()),                  this,   SLOT(onEnableScheduleClicked()));
    connect(ui->chkBoxEnableSchedule,   SIGNAL(stateChanged(int)),          this,   SLOT(updateGridEnabledState()));
    connect(ui->chkBoxEnableSchedule,   SIGNAL(stateChanged(int)),          this,   SIGNAL(scheduleEnabledChanged()));

    connect(ui->gridWidget,             SIGNAL(cellActivated(QPoint)),      this,   SLOT(onCellActivated(QPoint)));
    
    connectToGridWidget();
    
    updateGridEnabledState();
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
    setReadOnly(ui->btnRecordAlways, readOnly);
    setReadOnly(ui->btnRecordMotion, readOnly);
    setReadOnly(ui->btnNoRecord, readOnly);
    setReadOnly(ui->comboBoxQuality, readOnly);
    setReadOnly(ui->fpsSpinBox, readOnly);
    setReadOnly(ui->chkBoxEnableSchedule, readOnly);
    setReadOnly(ui->gridWidget, readOnly);
    m_readOnly = readOnly;
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
                if (color == ui->btnRecordAlways->color())
                    recordType = QnScheduleTask::RecordingType_Run;
                else if (color == ui->btnRecordMotion->color())
                    recordType = QnScheduleTask::RecordingType_MotionOnly;
                else if (color == ui->btnNoRecord->color())
                    recordType = QnScheduleTask::RecordingType_Never;
                else
                    qWarning("ColorParam wasn't acknowledged. fallback to 'Always'");
            }
            QnStreamQuality streamQuality = QnQualityHighest;
            {
                QString shortQuality(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::SecondParam).toString());
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
                task.m_beforeThreshold = ui->spinBoxRecordBefore->value();
                task.m_afterThreshold = ui->spinBoxRecordAfter->value();
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

        ui->spinBoxRecordBefore->setValue(task.m_beforeThreshold);
        ui->spinBoxRecordAfter->setValue(task.m_afterThreshold);
    } else {
        for (int nDay = 1; nDay <= 7; ++nDay)
            tasks.append(QnScheduleTask::Data(nDay, 0, 86400, QnScheduleTask::RecordingType_Never, 10, 10));
    }

    foreach (const QnScheduleTask::Data &task, tasks) {
        const int row = task.m_dayOfWeek - 1;

        QColor color;
        switch (task.m_recordType) {
        case QnScheduleTask::RecordingType_Run: color = ui->btnRecordAlways->color(); break;
        case QnScheduleTask::RecordingType_MotionOnly: color = ui->btnRecordMotion->color(); break;
        case QnScheduleTask::RecordingType_Never: color = ui->btnNoRecord->color(); break;
        default:
            qWarning("QnCameraScheduleWidget::setScheduleTasks(): Unhandled RecordingType value %d", task.m_recordType);
            break;
        }

        QString shortQuality("-");
        if (task.m_recordType != QnScheduleTask::RecordingType_Never)
        {
            switch (task.m_streamQuality) 
            {
                case QnQualityLow: shortQuality = QLatin1String("Lo"); break;
                case QnQualityNormal: shortQuality = QLatin1String("Md"); break;
                case QnQualityHigh: shortQuality = QLatin1String("Hi"); break;
                case QnQualityHighest: shortQuality = QLatin1String("Bst"); break;
                default:
            qWarning("QnCameraScheduleWidget::setScheduleTasks(): Unhandled StreamQuality value %d", task.m_streamQuality);
                    break;
            }
        }

        //int fps = task.m_fps;
        QString fps("-");
        if (task.m_recordType != QnScheduleTask::RecordingType_Never)
            fps = QString::number(task.m_fps);

        for (int col = task.m_startTime / 3600; col < task.m_endTime / 3600; ++col) {
            const QPoint cell(col, row);

            ui->gridWidget->setCellValue(cell, QnScheduleGridWidget::ColorParam, color.rgba());
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
    for (int i = 0; i < ui->comboBoxQuality->count(); ++i)
    {
        if (ui->comboBoxQuality->itemText(i) == text)
            return i;
    }
    return 0;
}

void QnCameraScheduleWidget::updateGridParams(bool fromUserInput)
{
    if (m_disableUpdateGridParams)
        return;

    QColor color;
    if (ui->btnRecordAlways->isChecked())
        color = ui->btnRecordAlways->color();
    else if (ui->btnRecordMotion->isChecked())
        color = ui->btnRecordMotion->color();
    else if (ui->btnNoRecord->isChecked())
        color = ui->btnNoRecord->color();

    bool enabled = !ui->btnNoRecord->isChecked();
    bool motionEnabled = ui->btnRecordMotion->isChecked();

    ui->fpsSpinBox->setEnabled(enabled);
    ui->comboBoxQuality->setEnabled(enabled);
    ui->spinBoxRecordBefore->setEnabled(motionEnabled);
    ui->spinBoxRecordAfter->setEnabled(motionEnabled);

    if(!(m_readOnly && fromUserInput)) {
        ui->gridWidget->setDefaultParam(QnScheduleGridWidget::ColorParam, color.rgba());
        if (ui->btnNoRecord->isChecked())
        {
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::FirstParam, QLatin1String("-"));
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::SecondParam, QLatin1String("-"));
        }
        else
        {
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::FirstParam, QString::number(ui->fpsSpinBox->value()));
            ui->gridWidget->setDefaultParam(QnScheduleGridWidget::SecondParam, getShortText(ui->comboBoxQuality->currentText()));
        }
    }
}

void QnCameraScheduleWidget::onCellActivated(const QPoint &cell)
{
    m_disableUpdateGridParams = true;
    QColor color(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::ColorParam).toUInt());
    double fps(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::FirstParam).toDouble());
    QString shortQuality(ui->gridWidget->cellValue(cell, QnScheduleGridWidget::SecondParam).toString());

    if (color == ui->btnRecordAlways->color())
        ui->btnRecordAlways->setChecked(true);
    else if (color == ui->btnRecordMotion->color())
        ui->btnRecordMotion->setChecked(true);
    else if (color == ui->btnNoRecord->color())
        ui->btnNoRecord->setChecked(true);
    if (color != ui->btnNoRecord->color())
    {
        ui->fpsSpinBox->setValue(fps);
        ui->comboBoxQuality->setCurrentIndex(qualityTextToIndex(getLongText(shortQuality)));
    }
    m_disableUpdateGridParams = false;
    updateGridParams(true);
}

void QnCameraScheduleWidget::onDisplayQualityChanged(int state)
{
    ui->gridWidget->setShowSecondParam(state);
}

void QnCameraScheduleWidget::onDisplayFPSChanged(int state)
{
    ui->gridWidget->setShowFirstParam(state);
}

void QnCameraScheduleWidget::setMaxFps(int value)
{
    if(value < ui->fpsSpinBox->minimum())
        value = ui->fpsSpinBox->minimum(); /* Silently ignoring invalid input is OK here. */
    ui->fpsSpinBox->setMaximum(value);
}

void QnCameraScheduleWidget::onEnableScheduleClicked()
{
    Qt::CheckState state = ui->chkBoxEnableSchedule->checkState();

    ui->chkBoxEnableSchedule->setTristate(false);
    if (state == Qt::PartiallyChecked)
        ui->chkBoxEnableSchedule->setCheckState(Qt::Checked);
}

void QnCameraScheduleWidget::updateGridEnabledState() 
{
    bool enabled = ui->chkBoxEnableSchedule->checkState() == Qt::Checked;

    ui->ScheduleGridGroupBox->setEnabled(enabled);
    ui->groupBoxSettings->setEnabled(enabled);
    ui->groupBoxMotion->setEnabled(enabled);
    ui->gridWidget->setEnabled(enabled && !m_changesDisabled);
}

void QnCameraScheduleWidget::setScheduleEnabled(Qt::CheckState checkState)
{
    ui->chkBoxEnableSchedule->setCheckState(checkState);
}

Qt::CheckState QnCameraScheduleWidget::getScheduleEnabled() const
{
    return ui->chkBoxEnableSchedule->checkState();
}
