#include "motion_business_event_widget.h"
#include "ui_motion_business_event_widget.h"

QnMotionBusinessEventWidget::QnMotionBusinessEventWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnMotionBusinessEventWidget)
{
    ui->setupUi(this);
}

QnMotionBusinessEventWidget::~QnMotionBusinessEventWidget()
{
    delete ui;
}

void QnMotionBusinessEventWidget::loadParameters(const QnBusinessParams &params) {
    Q_UNUSED(params)
}

QnBusinessParams QnMotionBusinessEventWidget::parameters() const {
    return QnBusinessParams();
}

QString QnMotionBusinessEventWidget::description() const {
    return QObject::tr("When motion");
}
