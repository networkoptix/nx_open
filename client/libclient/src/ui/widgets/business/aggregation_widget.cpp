#include "aggregation_widget.h"
#include "ui_aggregation_widget.h"

#include <text/time_strings.h>

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

QnAggregationWidget::QnAggregationWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AggregationWidget)
{
    ui->setupUi(this);

    QSize minSize(this->minimumSize());
    minSize.setWidth(defaultOptimalWidth);
    setMinimumSize(minSize);

    ui->periodComboBox->addItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Seconds), second);
    ui->periodComboBox->addItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Minutes), minute);
    ui->periodComboBox->addItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Hours), hour);
    ui->periodComboBox->addItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Days), day);

    // initial state: checkbox is cleared
    ui->periodWidget->setVisible(false);

    connect(ui->enabledCheckBox,    &QCheckBox::toggled,    ui->periodWidget,   &QWidget::setVisible);
    connect(ui->enabledCheckBox,    &QCheckBox::toggled,    ui->instantLabel,   &QLabel::setHidden);
    connect(ui->enabledCheckBox,    &QCheckBox::toggled,    this,               &QnAggregationWidget::valueChanged);

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

QnAggregationWidget::~QnAggregationWidget()
{
}

void QnAggregationWidget::setValue(int secs) {
    ui->enabledCheckBox->setChecked(secs > 0);
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

int QnAggregationWidget::value() const {
    if (!ui->enabledCheckBox->isChecked())
        return 0;
    return  ui->valueSpinBox->value() * ui->periodComboBox->itemData(ui->periodComboBox->currentIndex()).toInt();
}

QWidget *QnAggregationWidget::lastTabItem()
{
    return ui->periodComboBox;
}

void QnAggregationWidget::setShort(bool value) {
    ui->longLabel->setVisible(!value);
}

int QnAggregationWidget::optimalWidth() {
    return defaultOptimalWidth;
}

void QnAggregationWidget::updateMinimumValue() {
    ui->valueSpinBox->setMinimum(ui->periodComboBox->currentIndex() == 0 ? 1 : 0);
}
