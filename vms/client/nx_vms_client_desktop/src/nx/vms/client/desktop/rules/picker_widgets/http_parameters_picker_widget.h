// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_builder_fields/content_type_field.h>
#include <nx/vms/rules/action_builder_fields/http_method_field.h>

#include "dropdown_text_picker_widget_base.h"
#include "picker_widget_strings.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class HttpParametersPickerBase: public DropdownTextPickerWidgetBase<F>
{
    using base = DropdownTextPickerWidgetBase<F>;

public:
    using DropdownTextPickerWidgetBase<F>::DropdownTextPickerWidgetBase;

protected:
    BASE_COMMON_USINGS
    using DropdownTextPickerWidgetBase<F>::m_comboBox;

    void updateUi() override
    {
        const auto fieldValue = theField()->value();
        {
            QSignalBlocker blocker{m_comboBox};
            if (fieldValue.isEmpty())
                m_comboBox->setCurrentText(DropdownTextPickerWidgetStrings::autoValue());
            else
                m_comboBox->setCurrentText(fieldValue);
        }
    }

    void onCurrentIndexChanged() override
    {
        const auto value = m_comboBox->currentText().trimmed();
        if (value != DropdownTextPickerWidgetStrings::autoValue())
            theField()->setValue(value);
        else
            theField()->setValue({});
    }
};

class HttpContentTypePicker: public HttpParametersPickerBase<vms::rules::ContentTypeField>
{
public:
    HttpContentTypePicker(SystemContext* context, ParamsWidget* parent):
        HttpParametersPickerBase<vms::rules::ContentTypeField>(context, parent)
    {
        QSignalBlocker blocker{m_comboBox};
        m_comboBox->addItem(DropdownTextPickerWidgetStrings::autoValue());
        m_comboBox->addItem("text/plain");
        m_comboBox->addItem("text/html");
        m_comboBox->addItem("application/html");
        m_comboBox->addItem("application/json");
        m_comboBox->addItem("application/xml");
    }
};

class HttpMethodPicker: public HttpParametersPickerBase<vms::rules::HttpMethodField>
{
public:
    HttpMethodPicker(SystemContext* context, ParamsWidget* parent):
        HttpParametersPickerBase<vms::rules::HttpMethodField>(context, parent)
    {
        QSignalBlocker blocker{m_comboBox};
        m_comboBox->addItem(DropdownTextPickerWidgetStrings::autoValue());
        m_comboBox->addItem("GET");
        m_comboBox->addItem("POST");
        m_comboBox->addItem("PUT");
        m_comboBox->addItem("DELETE");
    }
};

} // namespace nx::vms::client::desktop::rules
