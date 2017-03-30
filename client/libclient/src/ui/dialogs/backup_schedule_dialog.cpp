#include "backup_schedule_dialog.h"
#include "ui_backup_schedule_dialog.h"

#include <QtCore/QLocale>
#include <QtCore/QTime>

#include <QtWidgets/QComboBox>

#include <core/resource/server_backup_schedule.h>

#include <nx_ec/data/api_media_server_data.h>

#include <ui/common/palette.h>
#include <ui/common/read_only.h>
#include <ui/style/custom_style.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace {
    const int secsPerDay = 3600 * 24;
    const int daysPerWeek = 7;
    const int bitsPerMegabit = 1024*1024;
    const int bitsPerByte = 8;

    /** Default format used all over the app. */
    const QString defaultTimeFormat = lit("hh:mm");

    Qt::DayOfWeek nextDay(Qt::DayOfWeek day) {
        static_assert(Qt::Sunday == 7, "Make sure Sunday is the last day");
        if (day == Qt::Sunday)
            return Qt::Monday;
        return static_cast<Qt::DayOfWeek>(day + 1);
    }

    int bytesToMBits(int bytes) {
        return bytes * bitsPerByte / bitsPerMegabit;
    }

    int mBitsToBytes(int mBits) {
        return mBits * bitsPerMegabit / bitsPerByte;
    }

    size_t dayToIndex(Qt::DayOfWeek day) {
        return static_cast<size_t>(day) - 1;
    }

    Qt::DayOfWeek indexToDay(size_t index) {
        return static_cast<Qt::DayOfWeek>(index + 1);
    }
}

QnBackupScheduleDialog::QnBackupScheduleDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::BackupScheduleDialog())
{
    ui->setupUi(this);

    initDayOfWeekCheckboxes();

    ui->comboBoxTimeTo->addItem(tr("Until finished"), -1);
    for (int i = 0; i < 24; ++i) {
        QTime t(i, 0, 0);
        ui->comboBoxTimeStart->addItem(t.toString(defaultTimeFormat), i * 3600);
        ui->comboBoxTimeTo->addItem(t.toString(defaultTimeFormat), i * 3600);
    }

    auto updateLimitControls = [this]
    {
        bool enabled = ui->limitBandwithCheckBox->isChecked();
        ui->spinBoxBandwidth->setEnabled(enabled);
        ui->bandwidthLabel->setEnabled(enabled);
        ui->warningLabel->setVisible(enabled);
    };
    setWarningStyle(ui->warningLabel);

    connect(ui->limitBandwithCheckBox, &QCheckBox::stateChanged, this, updateLimitControls);
    updateLimitControls();
}

QnBackupScheduleDialog::~QnBackupScheduleDialog()
{}

const QnBackupScheduleColors & QnBackupScheduleDialog::colors() const {
    return m_colors;
}

void QnBackupScheduleDialog::setColors( const QnBackupScheduleColors &colors ) {
    m_colors = colors;
    updateDayOfWeekCheckboxes();
}

QCheckBox* QnBackupScheduleDialog::checkboxByDayOfWeek( Qt::DayOfWeek day ) {
    return m_dowCheckboxes[dayToIndex(day)];
}

void QnBackupScheduleDialog::initDayOfWeekCheckboxes() {
    QLocale locale;

    QFont nameFont = this->font();
    nameFont.setCapitalization(QFont::Capitalize);

    Qt::DayOfWeek day = locale.firstDayOfWeek();
    for (int i = 0; i < daysPerWeek; ++i) {
        QCheckBox* checkbox = new QCheckBox(this);

        /* Adding checkboxes to widget in the locale order */
        ui->weekDaysLayout->addWidget(checkbox);

        m_dowCheckboxes[dayToIndex(day)] = checkbox;
        checkbox->setFont(nameFont);
        day = nextDay(day);
    }

    ui->weekDaysLayout->addStretch();

    updateDayOfWeekCheckboxes();
}

void QnBackupScheduleDialog::updateDayOfWeekCheckboxes()
{
    QLocale locale;

    for (size_t i = 0; i < m_dowCheckboxes.size(); ++i)
    {
        Qt::DayOfWeek day = indexToDay(static_cast<int>(i));
        QCheckBox* checkbox = m_dowCheckboxes[i];

        checkbox->setText(locale.dayName(day));
        if (!locale.weekdays().contains(day))
            setPaletteColor(checkbox, QPalette::Foreground, m_colors.weekEnd);
    }
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
    for (int i = 0; i < m_dowCheckboxes.size(); ++i) {
        Qt::DayOfWeek day = indexToDay(i);
        QCheckBox* checkbox = m_dowCheckboxes[i];
        checkbox->setChecked(allowedDays.testFlag(ec2::backup::fromQtDOW(day)));
    }

    setNearestValue(ui->comboBoxTimeStart, value.backupStartSec);
    if (value.backupDurationSec == -1)
        setNearestValue(ui->comboBoxTimeTo, value.backupDurationSec);
    else
        setNearestValue(ui->comboBoxTimeTo, (value.backupStartSec + value.backupDurationSec) % secsPerDay);

    ui->spinBoxBandwidth->setValue(qAbs(bytesToMBits(value.backupBitrate)));

    ui->limitBandwithCheckBox->setChecked(value.backupBitrate > 0);
}

void QnBackupScheduleDialog::submitToSettings(QnServerBackupSchedule& value)
{
    QList<Qt::DayOfWeek> days;
    for (size_t i = 0; i < m_dowCheckboxes.size(); ++i) {
        Qt::DayOfWeek day = indexToDay(i);
        QCheckBox* checkbox = m_dowCheckboxes[i];
        if (checkbox->isChecked())
            days << day;
    }

    value.backupDaysOfTheWeek = ec2::backup::fromQtDOW(days);

    value.backupStartSec = ui->comboBoxTimeStart->itemData(ui->comboBoxTimeStart->currentIndex()).toInt();
    int backupEnd = ui->comboBoxTimeTo->itemData(ui->comboBoxTimeTo->currentIndex()).toInt();
    if (backupEnd == -1) {
        value.backupDurationSec = backupEnd;
    }
    else {
        if (backupEnd < value.backupStartSec)
            backupEnd += secsPerDay;
        value.backupDurationSec = backupEnd - value.backupStartSec;
    }

    value.backupBitrate = mBitsToBytes(ui->spinBoxBandwidth->value());
    if (!ui->limitBandwithCheckBox->isChecked())
        value.backupBitrate = -value.backupBitrate;
}

void QnBackupScheduleDialog::setReadOnlyInternal()
{
    base_type::setReadOnlyInternal();

    ::setReadOnly(ui->comboBoxTimeStart, isReadOnly());
    ::setReadOnly(ui->comboBoxTimeTo, isReadOnly());
    ::setReadOnly(ui->limitBandwithCheckBox, isReadOnly());
    ::setReadOnly(ui->spinBoxBandwidth, isReadOnly());

    for (auto checkbox : m_dowCheckboxes)
        ::setReadOnly(checkbox, isReadOnly());
}
