// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/utils/scoped_connections.h>
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

    void onActionBuilderChanged() override
    {
        DropdownTextPickerWidgetBase<vms::rules::StateField>::onActionBuilderChanged();

        QSignalBlocker blocker{m_comboBox};

        auto stateValue = m_comboBox->currentData().value<api::rules::State>();
        m_comboBox->setCurrentIndex(m_comboBox->findData(QVariant::fromValue(stateValue)));

        if (const auto durationField =
            getActionField<vms::rules::OptionalTimeField>(vms::rules::utils::kDurationFieldName))
        {
            const auto updateState =
                [this, durationField, stateValue]
                {
                    if (durationField->value() == vms::rules::OptionalTimeField::value_type::zero())
                    {
                        setVisible(false);
                        m_comboBox->setCurrentIndex(-1);
                    }
                    else
                    {
                        m_comboBox->setCurrentIndex(
                            m_comboBox->findData(QVariant::fromValue(stateValue)));
                        setVisible(true);
                    }
                };

            updateState();

            m_scopedConnections << connect(
                durationField,
                &vms::rules::OptionalTimeField::valueChanged,
                this,
                updateState);
        }
    }

    void onCurrentIndexChanged() override
    {
        m_field->setValue(m_comboBox->currentData().value<api::rules::State>());
    }

private:
    utils::ScopedConnections m_scopedConnections;
};

} // namespace nx::vms::client::desktop::rules
