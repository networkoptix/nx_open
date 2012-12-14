#include "camera_input_business_event_widget.h"
#include "ui_camera_input_business_event_widget.h"

QnCameraInputBusinessEventWidget::QnCameraInputBusinessEventWidget(QWidget *parent) :
    QnAbstractBusinessEventWidget(parent),
    ui(new Ui::QnCameraInputBusinessEventWidget)
{
    ui->setupUi(this);
}

QnCameraInputBusinessEventWidget::~QnCameraInputBusinessEventWidget()
{
    delete ui;
}
