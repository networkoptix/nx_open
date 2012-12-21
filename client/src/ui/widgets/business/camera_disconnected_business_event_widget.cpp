#include "camera_disconnected_business_event_widget.h"
#include "ui_camera_disconnected_business_event_widget.h"

QnCameraDisconnectedBusinessEventWidget::QnCameraDisconnectedBusinessEventWidget(QWidget *parent) :
    QnAbstractBusinessParamsWidget(parent),
    ui(new Ui::QnCameraDisconnectedBusinessEventWidget)
{
    ui->setupUi(this);
}

QnCameraDisconnectedBusinessEventWidget::~QnCameraDisconnectedBusinessEventWidget()
{
    delete ui;
}

void QnCameraDisconnectedBusinessEventWidget::loadParameters(const QnBusinessParams &params) {
    Q_UNUSED(params)
}

QnBusinessParams QnCameraDisconnectedBusinessEventWidget::parameters() const {
    return QnBusinessParams();
}

QString QnCameraDisconnectedBusinessEventWidget::description() const {
    return tr("If camera goes offline");
}
