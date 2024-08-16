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
        m_comboBox->setCurrentText(!fieldValue.isEmpty() ? fieldValue : Strings::autoValue());
    }

    void onActivated() override
    {
        const auto value = m_comboBox->currentText().trimmed();
        m_field->setValue(value != Strings::autoValue() ? value : QString{});

        DropdownTextPickerWidgetBase<F>::onActivated();
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
        m_comboBox->addItem(Strings::autoValue());
        m_comboBox->addItem(network::http::Method::get.data());
        m_comboBox->addItem(network::http::Method::post.data());
        m_comboBox->addItem(network::http::Method::put.data());
        m_comboBox->addItem(network::http::Method::patch.data());
        m_comboBox->addItem(network::http::Method::delete_.data());
    }
};

} // namespace nx::vms::client::desktop::rules
