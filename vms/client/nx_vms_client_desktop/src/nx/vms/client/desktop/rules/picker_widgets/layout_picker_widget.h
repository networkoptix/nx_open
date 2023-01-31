// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource_dialogs/multiple_layout_selection_dialog.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <ui/widgets/select_resources_button.h>

#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class TargetLayoutPicker:
    public ResourcePickerWidgetBase<vms::rules::TargetLayoutField, QnSelectLayoutsButton>
{
public:
    using ResourcePickerWidgetBase<vms::rules::TargetLayoutField,
        QnSelectLayoutsButton>::ResourcePickerWidgetBase;

protected:
    void onSelectButtonClicked() override
    {
        auto selectedLayouts = m_field->value();

        MultipleLayoutSelectionDialog::selectLayouts(selectedLayouts, this);

        m_field->setValue(selectedLayouts);

        updateValue();
    }

    void updateValue() override
    {
        auto layoutList = resourcePool()->getResourcesByIds<QnLayoutResource>(m_field->value());

        m_selectButton->selectLayouts(layoutList);
    }
};

} // namespace nx::vms::client::desktop::rules
