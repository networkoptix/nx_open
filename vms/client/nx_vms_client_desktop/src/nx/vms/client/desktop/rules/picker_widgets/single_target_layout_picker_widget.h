// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <business/business_resource_validation.h>
#include <nx/vms/rules/action_builder_fields/layout_field.h>

#include "resource_picker_widget_base.h"

class QnLayoutAccessValidationPolicy;

namespace nx::vms::client::desktop::rules {

class SingleTargetLayoutPicker:
    public ResourcePickerWidgetBase<vms::rules::LayoutField>
{
    Q_OBJECT

public:
    SingleTargetLayoutPicker(SystemContext* context, ParamsWidget* parent);

    void updateUi() override;
    void onSelectButtonClicked() override;

private:
    QnLayoutAccessValidationPolicy m_policy;
    bool m_hasWarning{false};
};

} // namespace nx::vms::client::desktop::rules
