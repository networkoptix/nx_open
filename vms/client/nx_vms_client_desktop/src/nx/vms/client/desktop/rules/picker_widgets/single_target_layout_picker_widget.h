// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_builder_fields/target_layout_field.h>

#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class SingleTargetLayoutPicker:
    public ResourcePickerWidgetBase<vms::rules::TargetLayoutField>
{
    Q_OBJECT

public:
    SingleTargetLayoutPicker(
        vms::rules::TargetLayoutField* field, SystemContext* systemContext, ParamsWidget* parent);

    void onSelectButtonClicked() override;

private:
    bool m_hasWarning{false};
};

} // namespace nx::vms::client::desktop::rules
