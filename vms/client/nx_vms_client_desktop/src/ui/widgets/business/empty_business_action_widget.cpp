// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "empty_business_action_widget.h"
#include "ui_empty_business_action_widget.h"

using namespace nx::vms::client::desktop;

QnEmptyBusinessActionWidget::QnEmptyBusinessActionWidget(
    SystemContext* systemContext,
    nx::vms::event::ActionType actionType,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::EmptyBusinessActionWidget)
{
    ui->setupUi(this);

    switch (actionType)
    {
    case nx::vms::event::ActionType::panicRecordingAction:
        ui->label->setText(tr("Panic Recording mode switches recording settings for all cameras to maximum FPS and quality."));
        break;
    default:
        break;
    }
}

QnEmptyBusinessActionWidget::~QnEmptyBusinessActionWidget()
{
}
