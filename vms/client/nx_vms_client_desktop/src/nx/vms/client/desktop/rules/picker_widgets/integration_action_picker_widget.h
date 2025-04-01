// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/rules/action_builder_fields/integration_action_field.h>

#include "dropdown_text_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class IntegrationActionPicker:
    public DropdownTextPickerWidgetBase<vms::rules::IntegrationActionField>
{
public:
    IntegrationActionPicker(
        vms::rules::IntegrationActionField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        DropdownTextPickerWidgetBase<vms::rules::IntegrationActionField>(field, context, parent)
    {
        auto engines = resourcePool()->getResources<vms::common::AnalyticsEngineResource>();
        for (const auto& engine: engines)
        {
            auto manifest = engine->manifest();
            if (!manifest.objectActions.isEmpty())
            {
                for (const auto& action: manifest.objectActions)
                    m_comboBox->addItem(action.name, action.id);
            }
        }
    }

protected:
    void updateUi() override
{
        DropdownTextPickerWidgetBase<vms::rules::IntegrationActionField>::updateUi();

        m_comboBox->setCurrentIndex(m_comboBox->findData(QVariant::fromValue(m_field->value())));
    }

    void onActivated() override
    {
        m_field->setValue(m_comboBox->currentData().value<QString>());

        DropdownTextPickerWidgetBase<vms::rules::IntegrationActionField>::onActivated();
    }
};

} // namespace nx::vms::client::desktop::rules
