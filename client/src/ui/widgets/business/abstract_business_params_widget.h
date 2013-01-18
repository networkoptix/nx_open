#ifndef ABSTRACT_BUSINESS_PARAMS_WIDGET_H
#define ABSTRACT_BUSINESS_PARAMS_WIDGET_H

#include <QWidget>

#include <events/business_logic_common.h>
#include <ui/models/business_rules_view_model.h>

class QnAbstractBusinessParamsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnAbstractBusinessParamsWidget(QWidget *parent = 0);
    virtual ~QnAbstractBusinessParamsWidget();

    void setModel(QnBusinessRuleViewModel* model);
protected slots:
    virtual void at_model_dataChanged(QnBusinessRuleViewModel* model, QnBusiness::Fields fields);

    QnBusinessRuleViewModel* model();

protected:
    bool m_updating;

private:
    QnBusinessRuleViewModel* m_model;

};

#endif // ABSTRACT_BUSINESS_PARAMS_WIDGET_H
