#include "weektime_schedule_widget.h"
#include "ui_weektime_schedule_widget.h"

#include <QtGui/QMessageBox>


#include <core/resource_managment/resource_pool.h>

#include <ui/common/color_transformations.h>
#include <ui/common/read_only.h>
#include <ui/style/globals.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/event_processors.h>
#include "utils/media/bitStream.h"

QnWeekTimeScheduleWidget::QnWeekTimeScheduleWidget(QWidget *parent):
    QWidget(parent), 
    ui(new Ui::WeekTimeScheduleWidget),
    m_disableUpdateGridParams(false),
    m_inUpdate(0)
{
    ui->setupUi(this);

    // init buttons
    ui->recordAlwaysButton->setColor(qnGlobals->recordAlwaysColor());
    ui->recordAlwaysButton->setCheckedColor(shiftColor(qnGlobals->recordAlwaysColor(), SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA));

    ui->noRecordButton->setColor(qnGlobals->noRecordColor());
    ui->noRecordButton->setCheckedColor(shiftColor(qnGlobals->noRecordColor(), SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA, SEL_CELL_CLR_DELTA));

    connect(ui->recordAlwaysButton,      SIGNAL(toggled(bool)),             this,   SLOT(updateGridParams()));
    connect(ui->noRecordButton,          SIGNAL(toggled(bool)),             this,   SLOT(updateGridParams()));
    connect(ui->gridWidget,             SIGNAL(cellActivated(QPoint)),      this,   SLOT(at_gridWidget_cellActivated(QPoint)));

    QnSingleEventSignalizer *releaseSignalizer = new QnSingleEventSignalizer(this);
    releaseSignalizer->setEventType(QEvent::MouseButtonRelease);

    QnSingleEventSignalizer* gridMouseReleaseSignalizer = new QnSingleEventSignalizer(this);
    gridMouseReleaseSignalizer->setEventType(QEvent::MouseButtonRelease);
    ui->gridWidget->installEventFilter(gridMouseReleaseSignalizer);
    
    connectToGridWidget();
    
    ui->gridWidget->setShowSecondParam(false);
    ui->gridWidget->setShowFirstParam(false);
}

QnWeekTimeScheduleWidget::~QnWeekTimeScheduleWidget() {
    return;
}

void QnWeekTimeScheduleWidget::connectToGridWidget() 
{
    connect(ui->gridWidget, SIGNAL(cellValueChanged(const QPoint &)), this, SIGNAL(scheduleTasksChanged()));

}

void QnWeekTimeScheduleWidget::disconnectFromGridWidget() 
{
    disconnect(ui->gridWidget, SIGNAL(cellValueChanged(const QPoint &)), this, SIGNAL(scheduleTasksChanged()));
}

QString QnWeekTimeScheduleWidget::scheduleTasks() const
{
    quint8 buffer[24]; // qPower2Floor(24*7/8, 4)
    BitStreamWriter writer;
    writer.setBuffer(buffer, sizeof(buffer));
    try {
        for (int row = 0; row < ui->gridWidget->rowCount(); ++row) 
        {
            for (int col = 0; col < ui->gridWidget->columnCount();) {
                const QPoint cell(col, row);

                Qn::RecordingType recordType = ui->gridWidget->cellRecordingType(cell);
                QnStreamQuality streamQuality = QnQualityHighest;
                writer.putBit(recordType != Qn::RecordingType_Never);
            }
        }
    } catch(...)
    {
        // it is never happened if grid size is correct
    }
    QByteArray result = QByteArray::fromRawData((char*) buffer, 24*7/8);
    return QString::fromUtf8(result.toHex());
}

void QnWeekTimeScheduleWidget::setScheduleTasks(const QString& value)
{
    QByteArray schedule = QByteArray::fromHex(value.toUtf8());
    disconnectFromGridWidget(); /* We don't want to get 100500 notifications. */

    BitStreamReader reader((quint8*) schedule.data(), schedule.size());
    try {
        for (int row = 0; row < ui->gridWidget->rowCount(); ++row) 
        {
            for (int col = 0; col < ui->gridWidget->columnCount();) {
                const QPoint cell(col, row);

                Qn::RecordingType recordType = ui->gridWidget->cellRecordingType(cell);
                QnStreamQuality streamQuality = QnQualityHighest;
                ui->gridWidget->setCellRecordingType(cell, reader.getBit() ? Qn::RecordingType_Run : Qn::RecordingType_Never);
            }
        }
    } catch(...) {
        // it is never happened if grid size is correct
    }
    connectToGridWidget();

    emit scheduleTasksChanged();
}

void QnWeekTimeScheduleWidget::updateGridParams(bool fromUserInput)
{
    if (m_inUpdate > 0)
        return;

    if (m_disableUpdateGridParams)
        return;

    Qn::RecordingType recordType = Qn::RecordingType_Never;
    if (ui->recordAlwaysButton->isChecked())
        recordType = Qn::RecordingType_Run;
    else if (ui->noRecordButton->isChecked())
        recordType = Qn::RecordingType_Never;
    else
        qWarning() << "QnWeekTimeScheduleWidget::No record type is selected!";

    if(!fromUserInput)
        ui->gridWidget->setDefaultParam(QnScheduleGridWidget::RecordTypeParam, recordType);

    emit gridParamsChanged();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWeekTimeScheduleWidget::at_gridWidget_cellActivated(const QPoint &cell)
{
    m_disableUpdateGridParams = true;

    Qn::RecordingType recordType = ui->gridWidget->cellRecordingType(cell);

    switch (recordType) {
        case Qn::RecordingType_Run:
            ui->recordAlwaysButton->setChecked(true);
            break;
        default:
            ui->noRecordButton->setChecked(true);
            break;
    }

    m_disableUpdateGridParams = false;
    updateGridParams(true);
}
