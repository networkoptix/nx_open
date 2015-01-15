#include "aggregation_widget.h"
#include "ui_aggregation_widget.h"

namespace {
    const int defaultOptimalWidth = 300;
}

QnAggregationWidget::QnAggregationWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AggregationWidget)
{
    ui->setupUi(this);

    QSize minSize(this->minimumSize());
    minSize.setWidth(defaultOptimalWidth);
    setMinimumSize(minSize);

    ui->periodComboBox->addItem(tr("sec"), 1);
    ui->periodComboBox->addItem(tr("min"), 60);
    ui->periodComboBox->addItem(tr("hrs"), 60*60);
    ui->periodComboBox->addItem(tr("days"), 60*60*24);

    // initial state: checkbox is cleared
    ui->periodWidget->setVisible(false);

    connect(ui->enabledCheckBox, SIGNAL(toggled(bool)), ui->periodWidget, SLOT(setVisible(bool)));
    connect(ui->enabledCheckBox, SIGNAL(toggled(bool)), ui->instantLabel, SLOT(setHidden(bool)));

    connect(ui->enabledCheckBox,    SIGNAL(toggled(bool)),              this, SIGNAL(valueChanged()));
    connect(ui->valueSpinBox,       SIGNAL(valueChanged(int)),          this, SIGNAL(valueChanged()));
    connect(ui->periodComboBox,     SIGNAL(currentIndexChanged(int)),   this, SIGNAL(valueChanged()));
}

QnAggregationWidget::~QnAggregationWidget()
{
}

void QnAggregationWidget::setValue(int secs) {
    ui->enabledCheckBox->setChecked(secs > 0);
    if (secs > 0) {
        int idx = 0;
        while (idx < ui->periodComboBox->count() - 1
               && secs >= ui->periodComboBox->itemData(idx+1).toInt()
               && secs  % ui->periodComboBox->itemData(idx+1).toInt() == 0)
            idx++;

        ui->periodComboBox->setCurrentIndex(idx);
        ui->valueSpinBox->setValue(secs / ui->periodComboBox->itemData(idx).toInt());
    }
}

int QnAggregationWidget::value() const {
    if (!ui->enabledCheckBox->isChecked())
        return 0;
    return  ui->valueSpinBox->value() * ui->periodComboBox->itemData(ui->periodComboBox->currentIndex()).toInt();
}

void QnAggregationWidget::setShort(bool value) {
    ui->longLabel->setVisible(!value);
}

int QnAggregationWidget::optimalWidth() {
    return defaultOptimalWidth;
}
