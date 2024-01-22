// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <ui/widgets/business/time_duration_widget.h>

#include "field_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class OptionalDurationPicker: public TitledFieldPickerWidget<vms::rules::OptionalTimeField>
{
    Q_OBJECT

public:
    OptionalDurationPicker(SystemContext* context, ParamsWidget* parent);

protected:
    void onDescriptorSet() override;
    void updateUi() override;
    void onEnabledChanged(bool isEnabled) override;

private:
    QnElidedLabel* m_label{nullptr};
    TimeDurationWidget* m_timeDurationWidget;
};

} // namespace nx::vms::client::desktop::rules
