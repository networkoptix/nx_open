#include "software_trigger_business_event_widget.h"
#include "ui_software_trigger_business_event_widget.h"
#include <QtCore/QScopedValueRollback>
#include <business/business_strings_helper.h>


//TODO: #vkutin This implementation is a stub
// Actual implementation will be done as soon as software triggers specification is ready.


QnSoftwareTriggerBusinessEventWidget::QnSoftwareTriggerBusinessEventWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::SoftwareTriggerBusinessEventWidget)
{
    ui->setupUi(this);

    ui->triggerIdLineEdit->setPlaceholderText(QnBusinessStringsHelper::defaultSoftwareTriggerName());

    connect(ui->triggerIdLineEdit, &QLineEdit::textChanged, this,
        &QnSoftwareTriggerBusinessEventWidget::paramsChanged);
}

QnSoftwareTriggerBusinessEventWidget::~QnSoftwareTriggerBusinessEventWidget()
{
}

void QnSoftwareTriggerBusinessEventWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->triggerIdLineEdit);
    setTabOrder(ui->triggerIdLineEdit, after);
}

void QnSoftwareTriggerBusinessEventWidget::at_model_dataChanged(QnBusiness::Fields fields)
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    if (fields.testFlag(QnBusiness::EventParamsField))
    {
        const auto text = model()->eventParams().inputPortId.trimmed();
        ui->triggerIdLineEdit->setText(text);
    }
}

void QnSoftwareTriggerBusinessEventWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    auto eventParams = model()->eventParams();
    eventParams.inputPortId = ui->triggerIdLineEdit->text().trimmed();
    model()->setEventParams(eventParams);
}
