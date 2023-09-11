// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_duration_widget.h"
#include "ui_time_duration_widget.h"

#include <nx/utils/log/assert.h>

#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {

constexpr int kSeconds = 1;
constexpr int kSecondsInMinute = kSeconds * 60;
constexpr int kSecondsPerHour = kSecondsInMinute * 60;
constexpr int kSecondsInDay = kSecondsPerHour * 24;

TimeDurationWidget::TimeDurationWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::TimeDurationWidget)
{
    ui->setupUi(this);
    ui->periodComboBox->addItem(
        QnTimeStrings::longSuffixCapitalized(QnTimeStrings::Suffix::Seconds), kSeconds);
    ui->periodComboBox->setVisible(
        false); //< Have sense to be visible only when more than 1 item is added.
    ui->prefixLabel->setText(
        QnTimeStrings::fullSuffixCapitalized(QnTimeStrings::Suffix::Seconds, /*count*/ 0));
    ui->valueSpinBox->setMinimum(m_min);
    ui->valueSpinBox->setMaximum(m_max);

    connect(ui->periodComboBox, QnComboboxCurrentIndexChanged, this,
        [this]
        {
            updateMinimumValue();
            updateMaximumValue();
            emit valueChanged();
        });

    connect(ui->valueSpinBox, QnSpinboxIntValueChanged, this,
        [this](int value)
        {
            if (value == 0 && ui->periodComboBox->currentIndex() > 0)
            {
                int index = ui->periodComboBox->currentIndex();
                {
                    QSignalBlocker blocker(ui->periodComboBox);
                    ui->periodComboBox->setCurrentIndex(index - 1);
                    updateMinimumValue();
                }

                int intervalSec = ui->periodComboBox->itemData(index).toInt();
                int value = intervalSec / ui->periodComboBox->itemData(index - 1).toInt();
                ui->valueSpinBox->setValue(value - 1);
            }
            emit valueChanged();
        });
}

TimeDurationWidget::~TimeDurationWidget()
{
}

void TimeDurationWidget::setMaximum(int secs)
{
    if (!NX_ASSERT(secs >= m_min))
        return;

    m_max = secs;

    const auto periodsCount = ui->periodComboBox->count();
    for (auto i = periodsCount - 1; i > 0; --i)
    {
        if (ui->periodComboBox->itemData(i).toInt() < m_max)
            break;

        ui->periodComboBox->removeItem(i);
    }

    updateMaximumValue();
}

void TimeDurationWidget::setMinimum(int secs)
{
    if (!NX_ASSERT(secs <= m_max))
        return;

    m_min = secs;

    updateMinimumValue();
}

void TimeDurationWidget::addDurationSuffix(QnTimeStrings::Suffix suffix)
{
    QString suffixName = QnTimeStrings::longSuffixCapitalized(suffix);

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
        NX_ASSERT(false, "Suffix '%1' is not supported", suffixName);
        return;
    }

    if (period > m_max)
        return;

    ui->periodComboBox->addItem(suffixName, period);
    ui->prefixLabel->setVisible(false);
    ui->periodComboBox->setVisible(true);
}

void TimeDurationWidget::setValue(int secs)
{
    secs = std::clamp(secs, m_min, m_max);
    if (value() == secs)
        return;

    int idx = 0;
    while (idx < ui->periodComboBox->count() - 1
        && secs >= ui->periodComboBox->itemData(idx + 1).toInt()
        && secs % ui->periodComboBox->itemData(idx + 1).toInt() == 0)
    {
        idx++;
    }

    ui->periodComboBox->setCurrentIndex(idx);
    ui->valueSpinBox->setValue(secs / ui->periodComboBox->itemData(idx).toInt());
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
    const auto currentIndex = ui->periodComboBox->currentIndex();

    if (currentIndex == 0) //< seconds
        ui->valueSpinBox->setMinimum(m_min);
    else if (m_min < ui->periodComboBox->itemData(currentIndex).toInt())
        ui->valueSpinBox->setMinimum(m_min / ui->periodComboBox->itemData(currentIndex).toInt());
    else
        ui->valueSpinBox->setMinimum(0);
}

void TimeDurationWidget::updateMaximumValue()
{
    const auto currentIndex = ui->periodComboBox->currentIndex();

    if (currentIndex == 0) //< seconds
        ui->valueSpinBox->setMaximum(m_max);
    else
        ui->valueSpinBox->setMaximum(m_max / ui->periodComboBox->itemData(currentIndex).toInt());
}

} // namespace nx::vms::client::desktop
