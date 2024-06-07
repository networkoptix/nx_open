// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/resource_dialogs/multiple_layout_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <ui/widgets/select_resources_button.h>

#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class TargetLayoutPicker:
    public ResourcePickerWidgetBase<vms::rules::TargetLayoutField>
{
    Q_OBJECT
public:
    using ResourcePickerWidgetBase<vms::rules::TargetLayoutField>::ResourcePickerWidgetBase;

protected:
    void onSelectButtonClicked() override
    {
        auto selectedLayouts = m_field->value();

        if (!MultipleLayoutSelectionDialog::selectLayouts(selectedLayouts, this))
            return;

        m_field->setValue(selectedLayouts);
    }
};

} // namespace nx::vms::client::desktop::rules
