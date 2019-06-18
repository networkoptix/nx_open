#include "aggregation_widget.h"
#include "ui_aggregation_widget.h"

#include <text/time_strings.h>

#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {

static constexpr int kDefaultOptimalWidth = 300;

AggregationWidget::AggregationWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::AggregationWidget)
{
    ui->setupUi(this);

    QSize minSize(this->minimumSize());
    minSize.setWidth(kDefaultOptimalWidth);
    setMinimumSize(minSize);

    ui->periodWidget->addDurationSuffix(QnTimeStrings::Suffix::Minutes);
    ui->periodWidget->addDurationSuffix(QnTimeStrings::Suffix::Hours);
    ui->periodWidget->addDurationSuffix(QnTimeStrings::Suffix::Days);

    const auto updateVisualState =
        [this](bool aggregationIsEnabled)
        {
            ui->periodWidget->setVisible(aggregationIsEnabled);
            ui->everyLabel->setVisible(aggregationIsEnabled);
            ui->instantLabel->setVisible(!aggregationIsEnabled);
            emit valueChanged();
        };
    connect(ui->enabledCheckBox, &QCheckBox::toggled, this, updateVisualState);

    // Initial state: checkbox is cleared.
    updateVisualState(false);

    connect(ui->periodWidget, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
}

AggregationWidget::~AggregationWidget()
{
}

void AggregationWidget::setValue(int secs)
{
    ui->enabledCheckBox->setChecked(secs > 0);
    ui->periodWidget->setValue(secs);
}

int AggregationWidget::value() const
{
    if (!ui->enabledCheckBox->isChecked())
        return 0;
    return ui->periodWidget->value();
}

QWidget* AggregationWidget::lastTabItem() const
{
    return ui->periodWidget->lastTabItem();
}

void AggregationWidget::setShort(bool value)
{
    ui->longLabel->setVisible(!value);
}

int AggregationWidget::optimalWidth()
{
    return kDefaultOptimalWidth;
}

} // namespace nx::vms::client::desktop
