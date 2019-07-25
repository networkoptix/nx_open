#pragma once

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui { class PluginDiagnosticEventWidget; }

namespace nx::vms::client::desktop {
namespace ui {

class PluginDiagnosticEventModel;

class PluginDiagnosticEventWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit PluginDiagnosticEventWidget(QWidget* parent = nullptr);
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

} // namespace ui
} // namespace nx::vms::client::desktop
