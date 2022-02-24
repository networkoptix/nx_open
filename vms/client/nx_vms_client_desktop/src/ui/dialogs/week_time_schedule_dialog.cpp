// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "week_time_schedule_dialog.h"

#include "ui_week_time_schedule_dialog.h"

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <utils/media/bitStream.h>

#include <nx/vms/client/desktop/resource_properties/schedule/schedule_cell_painter.h>

using ScheduleGridWidget = nx::vms::client::desktop::ScheduleGridWidget;

QnWeekTimeScheduleDialog::QnWeekTimeScheduleDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::WeekTimeScheduleDialog),
    m_scheduleCellPainter(new nx::vms::client::desktop::ScheduleCellPainter())
{
    ui->setupUi(this);

    ui->gridWidget->setCellPainter(m_scheduleCellPainter.get());
    ui->valueOnButton->setCellPainter(m_scheduleCellPainter.get());
    ui->valueOffButton->setCellPainter(m_scheduleCellPainter.get());

    ui->valueOnButton->setButtonBrush(true);
    ui->valueOffButton->setButtonBrush({});

    setHelpTopic(this, Qn::EventsActions_Schedule_Help);

    connect(ui->valueOnButton,  &QToolButton::clicked, this,
        [this]() { ui->gridWidget->setBrush(ui->valueOnButton->buttonBrush()); });

    connect(ui->valueOffButton, &QToolButton::clicked, this,
        [this]() { ui->gridWidget->setBrush(ui->valueOffButton->buttonBrush()); });

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QnWeekTimeScheduleDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QnWeekTimeScheduleDialog::reject);
}

QnWeekTimeScheduleDialog::~QnWeekTimeScheduleDialog()
{
}

QString QnWeekTimeScheduleDialog::scheduleTasks() const
{
    std::array<uint8_t, 24> buffer; // qPower2Floor(24*7/8, 4)
    BitStreamWriter writer;
    writer.setBuffer(buffer.data(), sizeof(buffer));

    for (const auto dayOfWeek: ScheduleGridWidget::daysOfWeek())
    {
        for (int hour = 0; hour < ScheduleGridWidget::kHoursInDay; ++hour)
            writer.putBit(ui->gridWidget->cellData(dayOfWeek, hour).toBool());
    }

    writer.flushBits();
    QByteArray result = QByteArray::fromRawData((char*)buffer.data(), 24 * 7 / 8);
    return QString::fromUtf8(result.toHex());
}

void QnWeekTimeScheduleDialog::setScheduleTasks(const QString& value)
{
    if (value.isEmpty())
    {
        ui->gridWidget->fillGridData(true);
        return;
    }

    QByteArray schedule = QByteArray::fromHex(value.toUtf8());
    BitStreamReader reader((uint8_t*)schedule.data(), schedule.size());

    for (const auto dayOfWeek: ScheduleGridWidget::daysOfWeek())
    {
        for (int hour = 0; hour < ScheduleGridWidget::kHoursInDay; ++hour)
            ui->gridWidget->setCellData(dayOfWeek, hour, reader.getBit());
    }
}
