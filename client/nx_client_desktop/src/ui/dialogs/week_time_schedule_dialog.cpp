#include "week_time_schedule_dialog.h"
#include "ui_week_time_schedule_dialog.h"

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>

#include <utils/math/color_transformations.h>
#include "utils/media/bitStream.h"

QnWeekTimeScheduleDialog::QnWeekTimeScheduleDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::WeekTimeScheduleDialog),
    m_disableUpdateGridParams(false),
    m_inUpdate(0)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::EventsActions_Schedule_Help);

    // init buttons
    connect(ui->gridWidget, &QnScheduleGridWidget::colorsChanged, this, &QnWeekTimeScheduleDialog::updateColors);
    updateColors();

    connect(ui->valueOnButton, SIGNAL(toggled(bool)), this, SLOT(updateGridParams()));
    connect(ui->valueOffButton, SIGNAL(toggled(bool)), this, SLOT(updateGridParams()));
    connect(ui->gridWidget, SIGNAL(cellActivated(QPoint)), this, SLOT(at_gridWidget_cellActivated(QPoint)));

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    connectToGridWidget();

    ui->gridWidget->setShowQuality(false);
    ui->gridWidget->setShowFps(false);
}

QnWeekTimeScheduleDialog::~QnWeekTimeScheduleDialog()
{
}

void QnWeekTimeScheduleDialog::connectToGridWidget()
{
    connect(ui->gridWidget, SIGNAL(cellValueChanged(const QPoint &)), this, SIGNAL(scheduleTasksChanged()));
}

void QnWeekTimeScheduleDialog::disconnectFromGridWidget()
{
    disconnect(ui->gridWidget, SIGNAL(cellValueChanged(const QPoint &)), this, SIGNAL(scheduleTasksChanged()));
}

QString QnWeekTimeScheduleDialog::scheduleTasks() const
{
    quint8 buffer[24]; // qPower2Floor(24*7/8, 4)
    BitStreamWriter writer;
    writer.setBuffer(buffer, sizeof(buffer));
    try
    {
        for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
        {
            for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
            {
                const QPoint cell(col, row);

                Qn::RecordingType recordType = ui->gridWidget->cellValue(cell).recordingType;
                writer.putBit(recordType != Qn::RT_Never);
            }
        }
        writer.flushBits();
    }
    catch (...)
    {
        // it is never happened if grid size is correct
    }
    QByteArray result = QByteArray::fromRawData((char*)buffer, 24 * 7 / 8);
    return QString::fromUtf8(result.toHex());
}

void QnWeekTimeScheduleDialog::setScheduleTasks(const QString& value)
{
    disconnectFromGridWidget(); /* We don't want to get 100500 notifications. */

    if (value.isEmpty())
    {
        for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
        {
            for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
            {
                const QPoint cell(col, row);
                auto params = ui->gridWidget->cellValue(cell);
                params.recordingType = Qn::RT_Always;
                ui->gridWidget->setCellValue(cell, params);
            }
        }
    }
    else
    {
        QByteArray schedule = QByteArray::fromHex(value.toUtf8());
        try
        {
            BitStreamReader reader((quint8*)schedule.data(), schedule.size());
            for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
            {
                for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
                {
                    const QPoint cell(col, row);
                    auto params = ui->gridWidget->cellValue(cell);
                    params.recordingType = reader.getBit() ? Qn::RT_Always : Qn::RT_Never;
                    ui->gridWidget->setCellValue(cell, params);
                }
            }
        }
        catch (...)
        {
            // we are here if value is empty
            // it is never happened if grid size is correct
        }
    }

    connectToGridWidget();

    emit scheduleTasksChanged();
}

void QnWeekTimeScheduleDialog::updateGridParams(bool fromUserInput)
{
    if (m_inUpdate > 0)
        return;

    if (m_disableUpdateGridParams)
        return;

    QnScheduleGridWidget::CellParams brush;
    brush.recordingType = Qn::RT_Never;
    if (ui->valueOnButton->isChecked())
        brush.recordingType = Qn::RT_Always;
    else if (ui->valueOffButton->isChecked())
        brush.recordingType = Qn::RT_Never;
    else
        qWarning() << "QnWeekTimeScheduleDialog::No record type is selected!";

    if (!fromUserInput)
        ui->gridWidget->setBrush(brush);

    emit gridParamsChanged();
}

void QnWeekTimeScheduleDialog::updateColors()
{
    ui->valueOnButton->setColor(ui->gridWidget->colors().recordAlways);
    ui->valueOffButton->setColor(ui->gridWidget->colors().recordNever);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnWeekTimeScheduleDialog::at_gridWidget_cellActivated(const QPoint &cell)
{
    m_disableUpdateGridParams = true;

    Qn::RecordingType recordType = ui->gridWidget->cellValue(cell).recordingType;

    switch (recordType)
    {
        case Qn::RT_Always:
            ui->valueOnButton->setChecked(true);
            break;
        default:
            ui->valueOffButton->setChecked(true);
            break;
    }

    m_disableUpdateGridParams = false;
    updateGridParams(true);
}
