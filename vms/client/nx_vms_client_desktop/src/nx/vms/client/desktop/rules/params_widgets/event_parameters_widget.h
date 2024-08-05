// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include "params_widget.h"

namespace nx::vms::client::desktop::rules {

class PickerWidget;

class EventParametersWidget: public ParamsWidget
{
    Q_OBJECT

public:
    explicit EventParametersWidget(WindowContext* context, QWidget* parent = nullptr);

    virtual std::optional<vms::rules::ItemDescriptor> descriptor() const override;
    void setEdited();

    virtual void updateUi() override;

    const QString& eventType() const { return m_eventType; }

private:
    virtual void onRuleSet(bool isNewRule) override;

    QString m_eventType; //< Event for which the widget was created.
    std::vector<PickerWidget*> m_pickers;
};

} // namespace nx::vms::client::desktop::rules
