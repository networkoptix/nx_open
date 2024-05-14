// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "output_port_picker_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/utils/field.h>

#include "picker_widget_strings.h"

namespace nx::vms::client::desktop::rules {

void OutputPortPicker::updateUi()
{
    const auto targetDeviceField =
        getActionField<vms::rules::TargetDeviceField>(vms::rules::utils::kDeviceIdsFieldName);
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

    const QSignalBlocker blocker{m_comboBox};
    m_comboBox->clear();
    m_comboBox->addItem(
        QString{"<%1>"}.arg(DropdownTextPickerWidgetStrings::automaticValue()), QString{});
    for (const auto& relayOutput: outputPorts)
        m_comboBox->addItem(relayOutput.getName(), relayOutput.id);

    m_comboBox->setCurrentIndex(m_comboBox->findData(m_field->value()));
}

void OutputPortPicker::onCurrentIndexChanged()
{
    m_field->setValue(m_comboBox->currentData().toString());
}

} // namespace nx::vms::client::desktop::rules
