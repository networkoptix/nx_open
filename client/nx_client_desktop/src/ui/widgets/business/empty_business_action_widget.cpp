#include "empty_business_action_widget.h"
#include "ui_empty_business_action_widget.h"

QnEmptyBusinessActionWidget::QnEmptyBusinessActionWidget(nx::vms::event::ActionType actionType, QWidget *parent) :
    base_type(parent),
    ui(new Ui::EmptyBusinessActionWidget)
{
    ui->setupUi(this);

    switch (actionType)
    {
    case nx::vms::event::ActionType::panicRecordingAction:
        ui->label->setText(tr("Panic Recording mode switches recording settings for all Cameras to maximum FPS and Quality"));
        break;
    default:
        break;
    }
}

QnEmptyBusinessActionWidget::~QnEmptyBusinessActionWidget()
{
}
