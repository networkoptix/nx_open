// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui { class PluginDiagnosticEventWidget; }

namespace nx::vms::client::desktop::ui {

class PluginDiagnosticEventModel;

class PluginDiagnosticEventWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit PluginDiagnosticEventWidget(SystemContext* systemContext, QWidget* parent = nullptr);
    virtual ~PluginDiagnosticEventWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;

private slots:
    void paramsChanged();

private:
    void updatePluginDiagnosticEventTypesModel();
    void updateSelectedEventType();
    nx::vms::event::EventParameters createEventParameters();

private:
    QScopedPointer<Ui::PluginDiagnosticEventWidget> ui;
    PluginDiagnosticEventModel* m_pluginDiagnosticEventModel;
};

} // namespace nx::vms::client::desktop::ui
