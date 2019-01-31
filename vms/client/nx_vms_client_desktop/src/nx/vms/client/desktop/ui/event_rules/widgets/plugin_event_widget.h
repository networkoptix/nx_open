#pragma once

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui { class PluginEventWidget; }

namespace nx::vms::client::desktop {
namespace ui {

class PluginEventModel;

class PluginEventWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit PluginEventWidget(QWidget* parent = nullptr);
    virtual ~PluginEventWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;

private slots:
    void paramsChanged();

private:
    void updatePluginEventTypesModel();
    void updateSelectedEventType();
    nx::vms::event::EventParameters createEventParameters();

private:
    QScopedPointer<Ui::PluginEventWidget> ui;
    PluginEventModel* m_pluginEventModel;

};

} // namespace ui
} // namespace nx::vms::client::desktop
