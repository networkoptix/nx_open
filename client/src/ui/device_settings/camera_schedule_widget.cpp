#include "camera_schedule_widget.h"
#include "ui_camera_schedule.h"

CameraScheduleWidget::CameraScheduleWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CameraSchedule)
{
    ui->setupUi(this);
}
