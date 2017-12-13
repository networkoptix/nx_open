#include "time_duration_widget.h"
#include "ui_time_duration_widget.h"

#include <ui/workaround/widgets_signals_workaround.h>

namespace {
    const int defaultOptimalWidth = 300;

    enum {
        second = 1,
        minute = second * 60,
        hour = minute * 60,
        day = hour * 24
    };
}

QnTimeDurationWidget::QnTimeDurationWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TimeDurationWidget)
{
    ui->setupUi(this);

    QSize minSize(this->minimumSize());
    minSize.setWidth(defaultOptimalWidth);
    setMinimumSize(minSize);

    ui->periodComboBox->addItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Seconds), second);

    connect(ui->periodComboBox,     QnComboboxCurrentIndexChanged, this, [this]() {
        updateMinimumValue();
        emit valueChanged();
    });

    connect(ui->valueSpinBox,       QnSpinboxIntValueChanged, this, [this](int value) {
        if (value == 0) {
            int index = ui->periodComboBox->currentIndex();
            NX_ASSERT(index > 0, Q_FUNC_INFO, "Invalid minimum value for the minimal period.");
            if (index == 0) {
                ui->valueSpinBox->setValue(1);
            } else {
                ui->periodComboBox->blockSignals(true);
                ui->periodComboBox->setCurrentIndex(index - 1);
                ui->periodComboBox->blockSignals(false);
                updateMinimumValue();

                int intervalSec = ui->periodComboBox->itemData(index).toInt();
                int value = intervalSec / ui->periodComboBox->itemData(index - 1).toInt();
                ui->valueSpinBox->setValue(value - 1);
            }
        }
        emit valueChanged();
    });
}

QnTimeDurationWidget::~QnTimeDurationWidget()
{
}

void QnTimeDurationWidget::setMaximum(int value)
{
    this->ui->valueSpinBox->setMaximum(value);
}

void QnTimeDurationWidget::setEnabled(bool value)
{
    this->ui->valueSpinBox->setEnabled(value);
    this->ui->periodComboBox->setEnabled(value);
}

void QnTimeDurationWidget::addDurationSuffix(QnTimeStrings::Suffix suffix)
{
    QString suffixName = QnTimeStrings::longSuffix(suffix);
    int period = 1;
    switch (suffix)
    {
    case QnTimeStrings::Suffix::Minutes:
        period = minute;
        break;
    case QnTimeStrings::Suffix::Hours:
        period = hour;
        break;
    case QnTimeStrings::Suffix::Days:
        period = day;
        break;
    default:
        return;
    }

    ui->periodComboBox->addItem(suffixName, period);
}

void QnTimeDurationWidget::setValue(int secs) {
    if (secs > 0) {
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

int QnTimeDurationWidget::value() const {
    return  ui->valueSpinBox->value() * ui->periodComboBox->itemData(ui->periodComboBox->currentIndex()).toInt();
}

QWidget *QnTimeDurationWidget::lastTabItem()
{
    return ui->periodComboBox;
}

int QnTimeDurationWidget::optimalWidth() {
    return defaultOptimalWidth;
}

void QnTimeDurationWidget::updateMinimumValue() {
    ui->valueSpinBox->setMinimum(ui->periodComboBox->currentIndex() == 0 ? 1 : 0);
}
