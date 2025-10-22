// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_builder_fields/string_selection_field.h>

#include "dropdown_text_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class ListPicker: public DropdownTextPickerWidgetBase<vms::rules::StringSelectionField>
{
public:
    ListPicker(
        vms::rules::StringSelectionField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        DropdownTextPickerWidgetBase<vms::rules::StringSelectionField>(field, context, parent)
    {
        for (const auto& item: field->properties().items)
            m_comboBox->addItem(item.first, item.second);
    }

protected:
    void updateUi() override
    {
        DropdownTextPickerWidgetBase<vms::rules::StringSelectionField>::updateUi();

        m_comboBox->setCurrentIndex(m_comboBox->findData(QVariant::fromValue(m_field->value())));
    }

    void onActivated() override
    {
        m_field->setValue(m_comboBox->currentData().value<QString>());

        DropdownTextPickerWidgetBase<vms::rules::StringSelectionField>::onActivated();
    }
};

} // namespace nx::vms::client::desktop::rules
