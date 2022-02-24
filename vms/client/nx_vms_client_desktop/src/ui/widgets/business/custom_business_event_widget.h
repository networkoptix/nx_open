// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef CUSTOM_BUSINESS_EVENT_WIDGET_H
#define CUSTOM_BUSINESS_EVENT_WIDGET_H

#include <QtWidgets/QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class CustomBusinessEventWidget;
}

class QnCustomBusinessEventWidget:
    public QnAbstractBusinessParamsWidget,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnCustomBusinessEventWidget(QWidget *parent = 0);
    ~QnCustomBusinessEventWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;
    void setOmitLogging(bool state);
private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::CustomBusinessEventWidget> ui;
};

#endif // CUSTOM_BUSINESS_EVENT_WIDGET_H
