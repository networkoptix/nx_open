// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "failover_priority_picker_widget.h"

#include "ui_failover_priority_picker_widget.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>

namespace nx::vms::client::desktop {

FailoverPriorityPickerWidget::FailoverPriorityPickerWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::FailoverPriorityPickerWidget())
{
    ui->setupUi(this);
    for (auto button: findChildren<QPushButton*>())
    {
        // Button width set to minimally sufficient to render non-elided text.
        auto textSize = button->fontMetrics().size(Qt::TextShowMnemonic, button->text());
        button->setMaximumWidth(
            textSize.width() + style()->pixelMetric(QStyle::PM_ButtonMargin) * 2);
    }

    connect(ui->neverButton, &QPushButton::clicked, this,
        [this]() { emit failoverPriorityClicked(nx::vms::api::FailoverPriority::never); });

    connect(ui->lowButton, &QPushButton::clicked, this,
        [this]() { emit failoverPriorityClicked(nx::vms::api::FailoverPriority::low); });

    connect(ui->mediumButton, &QPushButton::clicked, this,
        [this]() { emit failoverPriorityClicked(nx::vms::api::FailoverPriority::medium); });

    connect(ui->highButton, &QPushButton::clicked, this,
        [this]() { emit failoverPriorityClicked(nx::vms::api::FailoverPriority::high); });
}

FailoverPriorityPickerWidget::~FailoverPriorityPickerWidget()
{
}

void FailoverPriorityPickerWidget::switchToPicker()
{
    ui->stackedWidget->setCurrentWidget(ui->pickerPage);
}

void FailoverPriorityPickerWidget::switchToPlaceholder()
{
    ui->stackedWidget->setCurrentWidget(ui->placeholderPage);
}

} // namespace nx::vms::client::desktop
