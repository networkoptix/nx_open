// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include "params_widget.h"

namespace nx::vms::client::desktop::rules {

class PickerWidget;

class ActionParametersWidget: public ParamsWidget
{
    Q_OBJECT

public:
    explicit ActionParametersWidget(WindowContext* context, QWidget* parent = nullptr);

    virtual std::optional<vms::rules::ItemDescriptor> descriptor() const override;

private:
    void onRuleSet() override;
    void updateUi() override;

    void onActionFieldChanged(const QString& fieldName);
    void onActionDurationChanged() const;

    std::vector<PickerWidget*> m_pickers;
};

} // namespace nx::vms::client::desktop::rules
