// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <business/business_resource_validation.h>
#include <nx/vms/rules/event_filter_fields/source_user_field.h>

#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class SourceUserPicker: public ResourcePickerWidgetBase<vms::rules::SourceUserField>
{
    Q_OBJECT

public:
    SourceUserPicker(SystemContext* context, CommonParamsWidget* parent);

protected:
    void onDescriptorSet() override;
    void onSelectButtonClicked() override;
    void updateUi() override;

private:
    std::unique_ptr<QnSubjectValidationPolicy> m_validationPolicy;
};

} // namespace nx::vms::client::desktop::rules
