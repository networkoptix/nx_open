// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_business_params_widget.h"

QnAbstractBusinessParamsWidget::QnAbstractBusinessParamsWidget(QWidget *parent)
    : base_type(parent)
    , m_updating(false)
    , m_model()
{
}

QnAbstractBusinessParamsWidget::~QnAbstractBusinessParamsWidget() {

}

QnBusinessRuleViewModelPtr QnAbstractBusinessParamsWidget::model() {
    return m_model;
}

void QnAbstractBusinessParamsWidget::setModel(const QnBusinessRuleViewModelPtr &model) {
    if (m_model)
        disconnect(m_model.get(), nullptr, this, nullptr);

    m_model = model;

    if(!m_model)
        return;

    connect(m_model.get(), &QnBusinessRuleViewModel::dataChanged, this, &QnAbstractBusinessParamsWidget::at_model_dataChanged);
    at_model_dataChanged(Field::all);
}

void QnAbstractBusinessParamsWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, after);
}

void QnAbstractBusinessParamsWidget::at_model_dataChanged(Fields fields) {
    Q_UNUSED(fields)
}
