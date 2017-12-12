#include "aggregation_widget.h"
#include "ui_aggregation_widget.h"

#include <text/time_strings.h>

#include <ui/workaround/widgets_signals_workaround.h>

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

    // initial state: checkbox is cleared
    ui->periodWidget->setVisible(false);
    ui->periodWidget->addDurationSuffix(QnTimeStrings::Suffix::Minutes);
    ui->periodWidget->addDurationSuffix(QnTimeStrings::Suffix::Hours);
    ui->periodWidget->addDurationSuffix(QnTimeStrings::Suffix::Days);

    connect(ui->enabledCheckBox,    &QCheckBox::toggled,    ui->periodWidget,   &QWidget::setVisible);
    connect(ui->enabledCheckBox,    &QCheckBox::toggled,    ui->instantLabel,   &QLabel::setHidden);
    connect(ui->enabledCheckBox,    &QCheckBox::toggled,    this,               &QnAggregationWidget::valueChanged);

    connect(ui->periodWidget, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
}

QnAggregationWidget::~QnAggregationWidget()
{
}

void QnAggregationWidget::setValue(int secs) {
    ui->enabledCheckBox->setChecked(secs > 0);
    ui->periodWidget->setValue(secs);
}

int QnAggregationWidget::value() const {
    if (!ui->enabledCheckBox->isChecked())
        return 0;
    return ui->periodWidget->value();
}

QWidget *QnAggregationWidget::lastTabItem()
{
    return ui->periodWidget->lastTabItem();
}

void QnAggregationWidget::setShort(bool value) {
    ui->longLabel->setVisible(!value);
}

int QnAggregationWidget::optimalWidth() {
    return defaultOptimalWidth;
}
