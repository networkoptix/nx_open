#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/event/event_fwd.h>

#include <client_core/connection_context_aware.h>

#include <ui/models/business_rules_view_model.h>

#include <utils/common/connective.h>

class QnAbstractBusinessParamsWidget : public Connective<QWidget>, public QnConnectionContextAware
{
    Q_OBJECT
    typedef Connective<QWidget> base_type;

public:
    explicit QnAbstractBusinessParamsWidget(QWidget *parent = 0);
    virtual ~QnAbstractBusinessParamsWidget();

    QnBusinessRuleViewModelPtr model();
    void setModel(const QnBusinessRuleViewModelPtr &model);

    virtual void updateTabOrder(QWidget* before, QWidget* after);

protected:
    using Field = QnBusinessRuleViewModel::Field;
    using Fields = QnBusinessRuleViewModel::Fields;

    virtual void at_model_dataChanged(Fields fields);

protected:
    bool m_updating;

private:
    QnBusinessRuleViewModelPtr m_model;

};
