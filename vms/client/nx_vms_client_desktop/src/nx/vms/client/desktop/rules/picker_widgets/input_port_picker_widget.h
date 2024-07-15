// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/event_filter_fields/input_port_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/field.h>

#include "../utils/strings.h"
#include "dropdown_text_picker_widget_base.h"

#pragma once

namespace nx::vms::client::desktop::rules {

class InputPortPicker: public DropdownTextPickerWidgetBase<vms::rules::InputPortField>
{
public:
    using DropdownTextPickerWidgetBase<vms::rules::InputPortField>::DropdownTextPickerWidgetBase;

protected:
    void onActivated() override
    {
        m_field->setValue(m_comboBox->currentData().toString());
    }

private:
    void updateUi() override
    {
        DropdownTextPickerWidgetBase<vms::rules::InputPortField>::updateUi();

        auto sourceCameraField =
            getEventField<vms::rules::SourceCameraField>(vms::rules::utils::kCameraIdFieldName);

        if (!NX_ASSERT(sourceCameraField))
            return;

        m_comboBox->clear();

        m_comboBox->addItem(Strings::autoValue(), QString());

        QnVirtualCameraResourceList cameras;
        if (sourceCameraField->acceptAll())
            cameras = resourcePool()->getAllCameras();
        else
            cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(sourceCameraField->ids());

        const auto inputComparator =
            [](const QnIOPortData& l, const QnIOPortData& r)
            {
                return l.id + l.getName() < r.id + r.getName();
            };

        const auto getSortedInputs =
            [&inputComparator](const QnVirtualCameraResourcePtr& camera)
            {
                auto cameraInputs = camera->ioPortDescriptions(Qn::PT_Input);
                std::sort(cameraInputs.begin(), cameraInputs.end(), inputComparator);

                return cameraInputs;
            };

        QnIOPortDataList totalInputs;
        bool isFirstIteration = true;

        // If multiple cameras chosen, only ports present in all the chosen cameras must be
        // available to be selected by the user.
        for (const auto& camera: cameras)
        {
            if (isFirstIteration)
            {
                totalInputs = getSortedInputs(camera);
                isFirstIteration = false;
                continue;
            }

            if (totalInputs.empty())
                return; //< There is no sense to check other cameras, only auto value is available.

            QnIOPortDataList intersection;
            const auto cameraInputs = getSortedInputs(camera);

            std::set_intersection(
                totalInputs.cbegin(),
                totalInputs.cend(),
                cameraInputs.cbegin(),
                cameraInputs.cend(),
                std::back_inserter(intersection),
                inputComparator);

            totalInputs = std::move(intersection);
        }

        for (const auto& cameraInput: totalInputs)
            m_comboBox->addItem(cameraInput.getName(), cameraInput.id);

        const auto valueIndex = m_comboBox->findData(m_field->value());
        m_comboBox->setCurrentIndex(valueIndex != -1 ? valueIndex : 0);
    }
};

} // namespace nx::vms::client::desktop::rules
