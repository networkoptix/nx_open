#include "camera_input_business_event_widget.h"
#include "ui_camera_input_business_event_widget.h"

#include <events/camera_input_business_event.h>

#include <utils/common/scoped_value_rollback.h>

QnCameraInputBusinessEventWidget::QnCameraInputBusinessEventWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnCameraInputBusinessEventWidget)
{
    ui->setupUi(this);

    connect(ui->inputIdLineEdit, SIGNAL(textChanged(QString)), this, SLOT(paramsChanged()));
}

QnCameraInputBusinessEventWidget::~QnCameraInputBusinessEventWidget()
{
    delete ui;
}

void QnCameraInputBusinessEventWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    if (fields & QnBusiness::EventParamsField) {
        QString text = BusinessEventParameters::getInputPortId(model->eventParams());
        if (ui->inputIdLineEdit->text() != text)
            ui->inputIdLineEdit->setText(text);
    }
    //TODO: #GDM update on resource change
}

void QnCameraInputBusinessEventWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessParams params;
    BusinessEventParameters::setInputPortId(&params, ui->inputIdLineEdit->text());
    model()->setActionParams(params);
}
