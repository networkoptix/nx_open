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
    void setEdited();

    void updateUi() override;

    const QString& actionType() const { return m_actionType; }

private:
    void onRuleSet(bool isNewRule) override;

    PickerWidget* createStatePickerIfRequired();

    QString m_actionType; //< Action for which the widget was created.
    std::vector<PickerWidget*> m_pickers;
};

} // namespace nx::vms::client::desktop::rules
