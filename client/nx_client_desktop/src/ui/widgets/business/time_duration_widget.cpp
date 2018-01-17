#include "time_duration_widget.h"
#include "ui_time_duration_widget.h"

#include <ui/workaround/widgets_signals_workaround.h>

namespace nx {
namespace client {
namespace desktop {

constexpr int kSeconds = 1;
constexpr int kSecondsInMinute = kSeconds * 60;
constexpr int kSecondsPerHour = kSecondsInMinute * 60;
constexpr int kSecondsInDay = kSecondsPerHour * 24;

TimeDurationWidget::TimeDurationWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::TimeDurationWidget)
{
    ui->setupUi(this);
    ui->periodComboBox->addItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Seconds), kSeconds);

    connect(ui->periodComboBox, QnComboboxCurrentIndexChanged, this,
        [this]
        {
            updateMinimumValue();
            emit valueChanged();
        });

    connect(ui->valueSpinBox, QnSpinboxIntValueChanged, this,
        [this](int value)
        {
            if (value == 0)
            {
                int index = ui->periodComboBox->currentIndex();
                NX_ASSERT(index >= 0, "Invalid minimum value for the minimal period.");
                if (index == 0)
                {
                    ui->valueSpinBox->setValue(1);
                }
                else
                {
                    {
                        QSignalBlocker blocker(ui->periodComboBox);
                        ui->periodComboBox->setCurrentIndex(index - 1);
                        updateMinimumValue();
                    }

                    int intervalSec = ui->periodComboBox->itemData(index).toInt();
                    int value = intervalSec / ui->periodComboBox->itemData(index - 1).toInt();
                    ui->valueSpinBox->setValue(value - 1);
                }
            }
            emit valueChanged();
        });
}

TimeDurationWidget::~TimeDurationWidget()
{
}

void TimeDurationWidget::setMaximum(int value)
{
    ui->valueSpinBox->setMaximum(value);
}

void TimeDurationWidget::addDurationSuffix(QnTimeStrings::Suffix suffix)
{
    QString suffixName = QnTimeStrings::longSuffix(suffix);

    int period = 1;
    switch (suffix)
    {
    case QnTimeStrings::Suffix::Minutes:
        period = kSecondsInMinute;
        break;
    case QnTimeStrings::Suffix::Hours:
        period = kSecondsPerHour;
        break;
    case QnTimeStrings::Suffix::Days:
        period = kSecondsInDay;
        break;
    default:
        NX_ASSERT(false, "Suffix is not supported");
        return;
    }

    ui->periodComboBox->addItem(suffixName, period);
}

void TimeDurationWidget::setValue(int secs)
{
    if (secs > 0)
    {
        int idx = 0;
        while (idx < ui->periodComboBox->count() - 1
               && secs >= ui->periodComboBox->itemData(idx + 1).toInt()
               && secs  % ui->periodComboBox->itemData(idx + 1).toInt() == 0)
        {
            idx++;
        }

        ui->periodComboBox->setCurrentIndex(idx);
        ui->valueSpinBox->setValue(secs / ui->periodComboBox->itemData(idx).toInt());
    }
}

int TimeDurationWidget::value() const
{
    return ui->valueSpinBox->value() * ui->periodComboBox->itemData(ui->periodComboBox->currentIndex()).toInt();
}

QWidget* TimeDurationWidget::lastTabItem() const
{
    return ui->periodComboBox;
}

void TimeDurationWidget::updateMinimumValue()
{
    ui->valueSpinBox->setMinimum(ui->periodComboBox->currentIndex() == 0 ? 1 : 0);
}

} // namespace desktop
} // namespace client
} // namespace nx
