#include "abstract_business_params_widget.h"

QnAbstractBusinessParamsWidget::QnAbstractBusinessParamsWidget(QWidget *parent):
    QWidget(parent),
    m_updating(false),
    m_model(NULL)
{
}

QnAbstractBusinessParamsWidget::~QnAbstractBusinessParamsWidget() {

}

QnBusinessRuleViewModel* QnAbstractBusinessParamsWidget::model() {
    return m_model;
}

void QnAbstractBusinessParamsWidget::setModel(QnBusinessRuleViewModel *model) {
    if (m_model)
        disconnect(m_model, 0, this, 0);

    m_model = model;

    if(!m_model)
        return;

    connect(m_model, SIGNAL(dataChanged(QnBusinessRuleViewModel*,QnBusiness::Fields)),
            this, SLOT(at_model_dataChanged(QnBusinessRuleViewModel*,QnBusiness::Fields)));
    at_model_dataChanged(model, QnBusiness::AllFieldsMask);
}

void QnAbstractBusinessParamsWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    Q_UNUSED(model)
    Q_UNUSED(fields)
}
