// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/camera_validation_policy.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/field.h>

#include "../utils/strings.h"
#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

template<typename Policy>
class SingleTargetDevicePicker:
    public ResourcePickerWidgetBase<vms::rules::TargetDeviceField>
{
public:
    SingleTargetDevicePicker(
        vms::rules::TargetDeviceField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        ResourcePickerWidgetBase<vms::rules::TargetDeviceField>(field, context, parent)
    {
    }

private:
    Policy policy;

    void onSelectButtonClicked() override
    {
        auto selectedCameras = UuidSet{m_field->id()};
        auto useSource = m_field->useSource();

        preparePolicy();

        if (Policy::canUseSourceCamera())
        {
            if (CameraSelectionDialog::selectCameras(
                systemContext(), selectedCameras, useSource, policy, this))
            {
                m_field->setId(selectedCameras.empty() ? nx::Uuid{} : *selectedCameras.begin());
                m_field->setUseSource(useSource);
            }
        }
        else
        {
            if (CameraSelectionDialog::selectCameras(systemContext(), selectedCameras, policy, this)
                && !selectedCameras.empty())
            {
                m_field->setId(*selectedCameras.begin());
            }
        }

        ResourcePickerWidgetBase<vms::rules::TargetDeviceField>::onSelectButtonClicked();
    }

    void preparePolicy();
};

template<typename Policy>
void SingleTargetDevicePicker<Policy>::preparePolicy()
{
}

template<>
void SingleTargetDevicePicker<QnFullscreenCameraPolicy>::preparePolicy()
{
    const auto layoutField =
        getActionField<vms::rules::TargetLayoutsField>(vms::rules::utils::kLayoutIdsFieldName);
    if (!NX_ASSERT(layoutField))
        return;

    const auto layouts = resourcePool()->getResourcesByIds<QnLayoutResource>(layoutField->value());
    policy.setLayouts(layouts);
}

} // namespace nx::vms::client::desktop::rules
