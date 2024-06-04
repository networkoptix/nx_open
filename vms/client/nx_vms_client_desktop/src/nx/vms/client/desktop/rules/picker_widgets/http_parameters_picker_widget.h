// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_builder_fields/content_type_field.h>
#include <nx/vms/rules/action_builder_fields/http_method_field.h>

#include "../utils/strings.h"
#include "dropdown_text_picker_widget_base.h"

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
        DropdownTextPickerWidgetBase<F>::updateUi();

        const auto fieldValue = m_field->value();
        {
            QSignalBlocker blocker{m_comboBox};
            if (fieldValue.isEmpty())
                m_comboBox->setCurrentText(Strings::autoValue());
            else
                m_comboBox->setCurrentText(fieldValue);
        }
    }

    void onCurrentIndexChanged() override
    {
        const auto value = m_comboBox->currentText().trimmed();
        if (value != Strings::autoValue())
            m_field->setValue(value);
        else
            m_field->setValue({});
    }
};

class HttpContentTypePicker: public HttpParametersPickerBase<vms::rules::ContentTypeField>
{
public:
    HttpContentTypePicker(
        vms::rules::ContentTypeField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        HttpParametersPickerBase<vms::rules::ContentTypeField>(field, context, parent)
    {
        QSignalBlocker blocker{m_comboBox};
        m_comboBox->addItem(Strings::autoValue());
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
    HttpMethodPicker(
        vms::rules::HttpMethodField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        HttpParametersPickerBase<vms::rules::HttpMethodField>(field, context, parent)
    {
        QSignalBlocker blocker{m_comboBox};
        m_comboBox->addItem(Strings::autoValue());
        m_comboBox->addItem("GET");
        m_comboBox->addItem("POST");
        m_comboBox->addItem("PUT");
        m_comboBox->addItem("DELETE");
    }
};

} // namespace nx::vms::client::desktop::rules
