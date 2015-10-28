#include "backup_schedule_dialog.h"
#include "ui_backup_schedule_dialog.h"

#include <QtCore/QLocale>

#include <core/resource/server_backup_schedule.h>

#include <nx_ec/data/api_media_server_data.h>

#include <ui/common/palette.h>

namespace {
    const int secsPerDay = 3600 * 24;
    const int daysPerWeek = 7;

    Qt::DayOfWeek nextDay(Qt::DayOfWeek day) {
        static_assert(Qt::Sunday == 7, "Make sure Sunday is the last day");
        if (day == Qt::Sunday)
            return Qt::Monday;
        return static_cast<Qt::DayOfWeek>(day + 1);
    }
}

QnBackupScheduleDialog::QnBackupScheduleDialog(QWidget *parent, Qt::WindowFlags windowFlags ):
    base_type(parent, windowFlags),
    ui(new Ui::BackupScheduleDialog())
{
    ui->setupUi(this);

    initDayOfWeekCheckboxes();

    ui->comboBoxTimeTo->addItem(tr("Until finished"), -1);
    for (int i = 0; i < 24; ++i) {
        QTime t(i, 0, 0);
        ui->comboBoxTimeStart->addItem(t.toString(Qt::DefaultLocaleShortDate), i * 3600);
        ui->comboBoxTimeTo->addItem(t.toString(Qt::DefaultLocaleShortDate), i * 3600);
    }
    connect(ui->comboBoxBandwidth, SIGNAL(currentIndexChanged(int)), this, SLOT(at_bandwidthComboBoxChange(int)));
}

QnBackupScheduleDialog::~QnBackupScheduleDialog() 
{}


void QnBackupScheduleDialog::initDayOfWeekCheckboxes() {

    auto locale = QLocale();
    QFont nameFont = this->font();
    nameFont.setCapitalization(QFont::Capitalize);

    qDebug() << locale.weekdays();

    Qt::DayOfWeek day = locale.firstDayOfWeek();
    for (int i = 0; i < daysPerWeek; ++i) {
        QCheckBox* checkbox = new QCheckBox(this);
        ui->weekDaysLayout->addWidget(checkbox);

        m_dowCheckboxes[day - 1] = checkbox;
        checkbox->setText( locale.dayName(day) );
        checkbox->setFont(nameFont);
        if (!locale.weekdays().contains(day))
            setPaletteColor(checkbox, QPalette::Foreground, Qt::red); //TODO: #GDM customize the same way as QnScheduleGridColors
        
        day = nextDay(day);
    }

    ui->weekDaysLayout->addStretch();
}

void QnBackupScheduleDialog::at_bandwidthComboBoxChange(int index)
{
    ui->spinBoxBandwidth->setEnabled(index > 0);
    ui->bandwidthLabel->setEnabled(index > 0);
}

void QnBackupScheduleDialog::setNearestValue(QComboBox* combobox, int time)
{
    int bestIndex = -1;
    int bestDiff = INT_MAX;
    for (int i = 0; i < combobox->count(); ++i) {
        int diff = qAbs(combobox->itemData(i).toInt() - time);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestIndex = i;
        }
    }
    if (bestIndex != -1)
        combobox->setCurrentIndex(bestIndex);
}

void QnBackupScheduleDialog::updateFromSettings(const QnServerBackupSchedule& value)
{
    ec2::backup::DaysOfWeek allowedDays = static_cast<ec2::backup::DaysOfWeek>(value.backupDaysOfTheWeek);
    for (int i = 1; i <= 7; ++i) {
        Qt::DayOfWeek day = static_cast<Qt::DayOfWeek>(i);
        m_dowCheckboxes[i - 1]->setChecked(allowedDays.testFlag(ec2::backup::fromQtDOW(day)));
    }


    setNearestValue(ui->comboBoxTimeStart, value.backupStart);
    if (value.backupDuration == -1)
        setNearestValue(ui->comboBoxTimeTo, value.backupDuration);
    else
        setNearestValue(ui->comboBoxTimeTo, (value.backupStart + value.backupDuration) % secsPerDay);

    ui->spinBoxBandwidth->setValue(qAbs(value.backupBitrate) * 8 / 1000000);
    ui->comboBoxBandwidth->setCurrentIndex(value.backupBitrate > 0 ? 1 : 0);
}

void QnBackupScheduleDialog::submitToSettings(QnServerBackupSchedule& value)
{
    QList<Qt::DayOfWeek> days;
    for (int i = 1; i <= 7; ++i) {
        if (m_dowCheckboxes[i - 1]->isChecked())
            days << static_cast<Qt::DayOfWeek>(i);
    }

    value.backupDaysOfTheWeek = ec2::backup::fromQtDOW(days);

    value.backupStart = ui->comboBoxTimeStart->itemData(ui->comboBoxTimeStart->currentIndex()).toInt();
    int backupEnd = ui->comboBoxTimeTo->itemData(ui->comboBoxTimeTo->currentIndex()).toInt();
    if (backupEnd == -1) {
        value.backupDuration = backupEnd;
    }
    else {
        if (backupEnd < value.backupStart)
            backupEnd += secsPerDay;
        value.backupDuration = backupEnd - value.backupStart;
    }

    value.backupBitrate = ui->spinBoxBandwidth->value() * 1000000 / 8;
    if (ui->comboBoxBandwidth->currentIndex() == 0)
        value.backupBitrate = -value.backupBitrate;
}
