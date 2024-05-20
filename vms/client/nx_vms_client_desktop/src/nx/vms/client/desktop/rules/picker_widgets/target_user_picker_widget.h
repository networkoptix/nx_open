// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <business/business_resource_validation.h>
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
#include <nx/vms/rules/user_validation_policy.h>

#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class TargetUserPicker: public ResourcePickerWidgetBase<vms::rules::TargetUserField>
{
    Q_OBJECT

public:
    TargetUserPicker(
        vms::rules::TargetUserField* field, SystemContext* context, ParamsWidget* parent);

protected:
    void onSelectButtonClicked() override;
    void updateUi() override;

private:
    std::unique_ptr<QnSubjectValidationPolicy> m_policy;
};

} // namespace nx::vms::client::desktop::rules
