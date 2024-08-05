// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/utils/stream_quality_strings.h>
#include <nx/vms/rules/action_builder_fields/stream_quality_field.h>

#include "dropdown_text_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class StreamQualityPicker: public DropdownTextPickerWidgetBase<vms::rules::StreamQualityField>
{
public:
    StreamQualityPicker(
        vms::rules::StreamQualityField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        DropdownTextPickerWidgetBase<vms::rules::StreamQualityField>(field, context, parent)
    {
        QSignalBlocker blocker{m_comboBox};
        using Quality = vms::api::StreamQuality;
        for (auto i: {Quality::lowest, Quality::low, Quality::normal, Quality::high, Quality::highest})
            m_comboBox->addItem(vms::client::desktop::toDisplayString(i), QVariant::fromValue(i));
    }

protected:
    void updateUi() override
    {
        DropdownTextPickerWidgetBase<vms::rules::StreamQualityField>::updateUi();

        m_comboBox->setCurrentIndex(m_comboBox->findData(QVariant::fromValue(m_field->value())));
    }

    void onActivated() override
    {
        m_field->setValue(m_comboBox->currentData().value<vms::api::StreamQuality>());

        DropdownTextPickerWidgetBase<vms::rules::StreamQualityField>::onActivated();
    }
};

} // namespace nx::vms::client::desktop::rules
