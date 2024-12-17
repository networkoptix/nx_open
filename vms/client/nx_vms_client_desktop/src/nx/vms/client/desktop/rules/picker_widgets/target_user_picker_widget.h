// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_builder_fields/target_users_field.h>
#include <nx/vms/rules/user_validation_policy.h>

#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class TargetUserPicker: public ResourcePickerWidgetBase<vms::rules::TargetUsersField>
{
    Q_OBJECT

public:
    TargetUserPicker(
        vms::rules::TargetUsersField* field, SystemContext* context, ParamsWidget* parent);

protected:
    void onSelectButtonClicked() override;

private:
    std::unique_ptr<QnSubjectValidationPolicy> m_policy;
};

} // namespace nx::vms::client::desktop::rules
