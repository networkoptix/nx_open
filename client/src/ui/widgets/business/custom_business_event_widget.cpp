#include "custom_business_event_widget.h"
#include "ui_custom_business_event_widget.h"

#include <core/resource/camera_resource.h>

#include <utils/common/scoped_value_rollback.h>

QnCustomBusinessEventWidget::QnCustomBusinessEventWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::CustomBusinessEventWidget)
{
    ui->setupUi(this);

    connect(ui->deviceNameEdit,  &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);
    connect(ui->captionEdit,     &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);
    connect(ui->descriptionEdit, &QLineEdit::textChanged, this, &QnCustomBusinessEventWidget::paramsChanged);
}

QnCustomBusinessEventWidget::~QnCustomBusinessEventWidget()
{
}

void QnCustomBusinessEventWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, ui->deviceNameEdit);
    setTabOrder(ui->deviceNameEdit, ui->captionEdit);
    setTabOrder(ui->captionEdit, ui->descriptionEdit);
    setTabOrder(ui->descriptionEdit, after);
}

void QnCustomBusinessEventWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields & QnBusiness::EventResourcesField) {
        // todo: notking to do so far. waiting for POS integration
    }

    if (fields & QnBusiness::EventParamsField) 
    {
        QString resName = model->eventParams().resourceName;
        if (ui->deviceNameEdit->text() != resName)
            ui->deviceNameEdit->setText(resName);

        QString caption = model->eventParams().caption;
        if (ui->captionEdit->text() != caption)
            ui->captionEdit->setText(caption);

        QString description = model->eventParams().description;
        if (ui->descriptionEdit->text() != description)
            ui->descriptionEdit->setText(description);
    }
}

void QnCustomBusinessEventWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    auto eventParams = model()->eventParams();
    eventParams.resourceName = ui->deviceNameEdit->text();
    eventParams.caption = ui->captionEdit->text();
    eventParams.description = ui->descriptionEdit->text();
    model()->setEventParams(eventParams);
}
