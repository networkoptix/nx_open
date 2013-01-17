#include "camera_input_business_event_widget.h"
#include "ui_camera_input_business_event_widget.h"

#include <events/camera_input_business_event.h>

QnCameraInputBusinessEventWidget::QnCameraInputBusinessEventWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnCameraInputBusinessEventWidget)
{
    ui->setupUi(this);
}

QnCameraInputBusinessEventWidget::~QnCameraInputBusinessEventWidget()
{
    delete ui;
}

void QnCameraInputBusinessEventWidget::loadParameters(const QnBusinessParams &params) {
    ui->inputIdLineEdit->setText(BusinessEventParameters::getInputPortId(params));
}

QnBusinessParams QnCameraInputBusinessEventWidget::parameters() const {
    QnBusinessParams params;
    BusinessEventParameters::setInputPortId(&params, ui->inputIdLineEdit->text());
    return params;
}

QString QnCameraInputBusinessEventWidget::description() const {
    //TODO: #GDM remove me
    return QString();
}
