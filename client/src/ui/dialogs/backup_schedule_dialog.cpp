#include "backup_schedule_dialog.h"
#include "core/resource/media_server_user_attributes.h"

namespace {
    static const int SECS_PER_DAY = 3600 * 24;
}

QnBackupScheduleDialog::QnBackupScheduleDialog(QWidget *parent, Qt::WindowFlags windowFlags ):
    base_type(parent, windowFlags),
    ui(new Ui::BackupScheduleDialog())
{
    ui->setupUi(this);
    ui->comboBoxTimeTo->addItem(tr("Until finished"), -1);
    for (int i = 0; i < 24; ++i) {
        QTime t(i, 0, 0);
        ui->comboBoxTimeStart->addItem(t.toString(Qt::DefaultLocaleShortDate), i * 3600);
        ui->comboBoxTimeTo->addItem(t.toString(Qt::DefaultLocaleShortDate), i * 3600);
    }
    connect(ui->comboBoxBandwidth, SIGNAL(currentIndexChanged(int)), this, SLOT(at_bandwidthComboBoxChange(int)));
}

void QnBackupScheduleDialog::accept() 
{
    base_type::accept();
}

void QnBackupScheduleDialog::at_bandwidthComboBoxChange(int index)
{
    ui->spinBoxBandwidth->setEnabled(index > 0);
    ui->bandwidthLabel->setEnabled(index > 0);
}

void QnBackupScheduleDialog::updateFromSettings(const QnMediaServerUserAttributes& value)
{
    int mask = 1;
    for (int i = 1; i <=7; ++i) {
        QList<QCheckBox*> checkBoxes = findChildren<QCheckBox*>(lit("checkBox%1").arg(i));
        if(!checkBoxes.isEmpty())
            checkBoxes[0]->setChecked(value.backupDaysOfTheWeek & mask);
        mask <<= 1;
    }
    setNearestValue(ui->comboBoxTimeStart, value.backupStart);
    if (value.backupDuration == -1)
        setNearestValue(ui->comboBoxTimeTo, value.backupDuration);
    else
        setNearestValue(ui->comboBoxTimeTo, (value.backupStart + value.backupDuration) % SECS_PER_DAY);

    ui->spinBoxBandwidth->setValue(qAbs(value.backupBitrate) * 8 / 1000000);
    ui->comboBoxBandwidth->setCurrentIndex(value.backupBitrate > 0 ? 1 : 0);
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

void QnBackupScheduleDialog::submitToSettings(QnMediaServerUserAttributes& value)
{
    value.backupDaysOfTheWeek = 0;
    int mask = 1;
    for (int i = 1; i <=7; ++i) {
        QList<QCheckBox*> checkBoxes = findChildren<QCheckBox*>(lit("checkBox%1").arg(i));
        if(!checkBoxes.isEmpty() && checkBoxes[0]->isChecked())
            value.backupDaysOfTheWeek |= mask;
        mask <<= 1;
    }
    value.backupStart = ui->comboBoxTimeStart->itemData(ui->comboBoxTimeStart->currentIndex()).toInt();
    int backupEnd = ui->comboBoxTimeTo->itemData(ui->comboBoxTimeTo->currentIndex()).toInt();
    if (backupEnd == -1) {
        value.backupDuration = backupEnd;
    }
    else {
        if (backupEnd < value.backupStart)
            backupEnd += SECS_PER_DAY;
        value.backupDuration = backupEnd - value.backupStart;
    }

    value.backupBitrate = ui->spinBoxBandwidth->value() * 1000000 / 8;
    if (ui->comboBoxBandwidth->currentIndex() == 0)
        value.backupBitrate = -value.backupBitrate;
}
