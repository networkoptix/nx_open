// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/event/event_fwd.h>
#include <ui/models/business_rules_view_model.h>

class QnAbstractBusinessParamsWidget:
    public QWidget,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT
    typedef QWidget base_type;

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
