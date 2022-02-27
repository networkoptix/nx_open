// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rewind_for_widget.h"
#include "ui_rewind_for_widget.h"

#include <nx/vms/text/time_strings.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {

RewindForWidget::RewindForWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::RewindForWidget())
{
    ui->setupUi(this);
    ui->secondsSpinBox->setSuffix(QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));

    const auto updateVisualState =
        [this](bool aggregationIsEnabled)
        {
            ui->secondsSpinBox->setVisible(aggregationIsEnabled);
            ui->rewindForLabel->setVisible(aggregationIsEnabled);
            ui->liveLabel->setVisible(!aggregationIsEnabled);
        };
    connect(ui->enabledCheckBox, &QCheckBox::toggled, this, updateVisualState);

    // Initial state: checkbox is cleared.
    updateVisualState(false);

    connect(ui->enabledCheckBox, &QCheckBox::toggled, this, &RewindForWidget::valueEdited);
    connect(ui->secondsSpinBox, QnSpinboxIntValueChanged, this,
        &RewindForWidget::valueEdited);
}

RewindForWidget::~RewindForWidget()
{
}

std::chrono::seconds RewindForWidget::value() const
{
    return ui->enabledCheckBox->isChecked()
        ? std::chrono::seconds(ui->secondsSpinBox->value())
        : std::chrono::seconds(0);
}

void RewindForWidget::setValue(std::chrono::seconds value)
{
    QSignalBlocker blocker(this);
    const bool enabled = (value.count() > 0);
    ui->enabledCheckBox->setChecked(enabled);
    if (enabled)
        ui->secondsSpinBox->setValue(value.count());
}

} // namespace nx::vms::client::desktop
