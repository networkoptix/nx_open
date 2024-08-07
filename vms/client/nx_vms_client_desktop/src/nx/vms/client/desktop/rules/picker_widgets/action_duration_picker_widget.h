// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>

#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <ui/widgets/business/time_duration_widget.h>

#include "../utils/state_combo_box.h"
#include "field_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class ActionDurationPickerWidget:
    public FieldPickerWidget<vms::rules::OptionalTimeField, PickerWidget>
{
    Q_DECLARE_TR_FUNCTIONS(ActionDurationPickerWidget)

public:
    ActionDurationPickerWidget(
        vms::rules::OptionalTimeField* field,
        SystemContext* context,
        ParamsWidget* parent);
    virtual ~ActionDurationPickerWidget() override;

    void setReadOnly(bool value) override;
    void updateUi() override;

private:
    QComboBox* m_durationTypeComboBox{};
    QWidget* m_contentWidget{};
    StateComboBox* m_actionStartComboBox{};
    TimeDurationWidget* m_timeDurationWidget{};

    void onDurationTypeChanged();
    void onDurationValueChanged();
    void onStateChanged();
};

} // namespace nx::vms::client::desktop::rules
