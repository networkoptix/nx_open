// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>

#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <ui/widgets/business/time_duration_widget.h>

#include "field_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class ActionDurationPickerWidget:
    public FieldPickerWidget<vms::rules::OptionalTimeField, PickerWidget>
{
public:
    ActionDurationPickerWidget(
        SystemContext* context,
        ParamsWidget* parent,
        const vms::rules::ItemDescriptor& eventDescriptor);
    virtual ~ActionDurationPickerWidget() override;

    void setReadOnly(bool value) override;
    void updateUi() override;

protected:
    void onDescriptorSet() override;

private:
    QComboBox* m_durationTypeComboBox{};
    QWidget* m_contentWidget{};
    QComboBox* m_actionStartComboBox{};
    TimeDurationWidget* m_timeDurationWidget{};

    void onDurationTypeChanged();
    void onDurationValueChanged();
    void onStateChanged();
};

} // namespace nx::vms::client::desktop::rules
