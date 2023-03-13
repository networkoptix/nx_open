// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/utils/field.h>

#include "dropdown_text_picker_widget_base.h"
#include "picker_widget_strings.h"

#pragma once

namespace nx::vms::client::desktop::rules {

class StatePicker: public DropdownTextPickerWidgetBase<vms::rules::StateField>
{
public:
    using DropdownTextPickerWidgetBase<vms::rules::StateField>::DropdownTextPickerWidgetBase;

protected:
    void onDescriptorSet() override
    {
        DropdownTextPickerWidgetBase<vms::rules::StateField>::onDescriptorSet();

        const auto itemFlags = parentParamsWidget()->descriptor().flags;
        QSignalBlocker blocker{m_comboBox};

        if (itemFlags.testFlag(vms::rules::ItemFlag::instant))
        {
            m_comboBox->addItem(
                DropdownTextPickerWidgetStrings::state(api::rules::State::instant),
                QVariant::fromValue(api::rules::State::instant));
        }

        if (itemFlags.testFlag(vms::rules::ItemFlag::prolonged))
        {
            m_comboBox->addItem(
                DropdownTextPickerWidgetStrings::state(api::rules::State::started),
                QVariant::fromValue(api::rules::State::started));
            m_comboBox->addItem(
                DropdownTextPickerWidgetStrings::state(api::rules::State::stopped),
                QVariant::fromValue(api::rules::State::stopped));
        }
    }

    void updateUi() override
    {
        const auto actionDescriptor = parentParamsWidget()->actionDescriptor();
        const auto hasProlongedFlag = actionDescriptor->flags.testFlag(vms::rules::ItemFlag::prolonged);

        const auto durationField =
            getActionField<vms::rules::OptionalTimeField>(vms::rules::utils::kDurationFieldName);
        const auto hasDuration = durationField
            && durationField->value() != vms::rules::OptionalTimeField::value_type::zero();

        const auto isProlonged = hasProlongedFlag && !hasDuration;

        const auto state = theField()->value();

        if (isProlonged && state != api::rules::State::none)
        {
            theField()->setValue(api::rules::State::none);
            return;
        }

        if (!isProlonged && state == api::rules::State::none)
        {
            const auto defaultValue =
                m_fieldDescriptor->properties.value("value").value<api::rules::State>();

            NX_ASSERT(defaultValue != api::rules::State::none);

            theField()->setValue(defaultValue);
            return;
        }

        this->setVisible(!isProlonged);

        QSignalBlocker blocker{m_comboBox};
        m_comboBox->setCurrentIndex(m_comboBox->findData(QVariant::fromValue(state)));
    }

    void onCurrentIndexChanged() override
    {
        theField()->setValue(m_comboBox->currentData().value<api::rules::State>());
    }
};

} // namespace nx::vms::client::desktop::rules
