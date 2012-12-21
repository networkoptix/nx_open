#include "storage_failure_business_event_widget.h"
#include "ui_storage_failure_business_event_widget.h"

QnStorageFailureBusinessEventWidget::QnStorageFailureBusinessEventWidget(QWidget *parent) :
    QnAbstractBusinessParamsWidget(parent),
    ui(new Ui::QnStorageFailureBusinessEventWidget)
{
    ui->setupUi(this);
}

QnStorageFailureBusinessEventWidget::~QnStorageFailureBusinessEventWidget()
{
    delete ui;
}

void QnStorageFailureBusinessEventWidget::loadParameters(const QnBusinessParams &params) {
    Q_UNUSED(params)
}

QnBusinessParams QnStorageFailureBusinessEventWidget::parameters() const {
    return QnBusinessParams();
}

QString QnStorageFailureBusinessEventWidget::description() const {
    return tr("If storage goes offline on %servername");
}
