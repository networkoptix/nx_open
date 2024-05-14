// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/action_builder_fields/fps_field.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/utils/field.h>

#include "number_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class FpsPicker: public NumberPicker<vms::rules::FpsField>
{
public:
    using NumberPicker<vms::rules::FpsField>::NumberPicker;

private:
    void updateUi() override
    {
        NumberPicker<vms::rules::FpsField>::updateUi();

        auto targetDeviceField = getActionField<vms::rules::TargetDeviceField>(
            vms::rules::utils::kDeviceIdsFieldName);

        if (!NX_ASSERT(targetDeviceField))
            return;

        const auto cameras =
            resourcePool()->getResourcesByIds<QnVirtualCameraResource>(targetDeviceField->ids());
        int maxFps{0};
        for (const auto& camera: cameras)
            maxFps = (maxFps == 0 ? camera->getMaxFps() : qMax(maxFps, camera->getMaxFps()));

        QSignalBlocker blocker{m_spinBox};
        m_spinBox->setEnabled(maxFps > 0);
        m_spinBox->setMaximum(maxFps);

        if (m_spinBox->value() == 0 && maxFps > 0)
        {
            m_spinBox->setValue(maxFps);
            m_field->setValue(maxFps);
        }

        m_spinBox->setMinimum(maxFps > 0 ? 1 : 0);
        m_spinBox->setEnabled(!cameras.empty());
    }
};

} // namespace nx::vms::client::desktop::rules
