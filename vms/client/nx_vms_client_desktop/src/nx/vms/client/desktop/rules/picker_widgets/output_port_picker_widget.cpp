// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "output_port_picker_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/action_builder_fields/target_devices_field.h>
#include <nx/vms/rules/utils/field.h>

#include "../utils/strings.h"

namespace nx::vms::client::desktop::rules {

void OutputPortPicker::updateUi()
{
    const auto targetDeviceField =
        getActionField<vms::rules::TargetDevicesField>(vms::rules::utils::kDeviceIdsFieldName);
    if (!NX_ASSERT(targetDeviceField))
        return;

    QnIOPortDataList outputPorts;
    bool initialized = false;

    const auto cameras =
        resourcePool()->getResourcesByIds<QnVirtualCameraResource>(targetDeviceField->ids());
    for (const auto& camera: cameras)
    {
        QnIOPortDataList cameraOutputs = camera->ioPortDescriptions(Qn::PT_Output);
        if (!initialized)
        {
            outputPorts = cameraOutputs;
            initialized = true;
        }
        else
        {
            for (auto itr = outputPorts.begin(); itr != outputPorts.end();)
            {
                const QnIOPortData& value = *itr;
                bool found = false;
                for (const auto& other: cameraOutputs)
                {
                    if (other.id == value.id && other.getName() == value.getName())
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                    ++itr;
                else
                    itr = outputPorts.erase(itr);
            }
        }
    }

    m_comboBox->clear();
    m_comboBox->addItem(Strings::autoValue(), QString{});
    for (const auto& relayOutput: outputPorts)
        m_comboBox->addItem(relayOutput.getName(), relayOutput.id);

    m_comboBox->setCurrentIndex(m_comboBox->findData(m_field->value()));
}

void OutputPortPicker::onActivated()
{
    m_field->setValue(m_comboBox->currentData().toString());

    DropdownTextPickerWidgetBase<vms::rules::OutputPortField>::onActivated();
}

} // namespace nx::vms::client::desktop::rules
