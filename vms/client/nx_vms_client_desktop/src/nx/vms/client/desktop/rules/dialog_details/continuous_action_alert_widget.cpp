// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "continuous_action_alert_widget.h"

#include "ui_continuous_action_alert_widget.h"

namespace nx::vms::client::desktop::rules {

ContinuousActionAlertWidget::ContinuousActionAlertWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::ContinuousActionAlertWidget)
{
    ui->setupUi(this);
}

ContinuousActionAlertWidget::~ContinuousActionAlertWidget()
{
}

} // namespace nx::vms::client::desktop::rules
