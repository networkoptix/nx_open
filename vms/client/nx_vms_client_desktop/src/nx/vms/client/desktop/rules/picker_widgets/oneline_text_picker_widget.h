// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>

#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/rules/action_builder_fields/password_field.h>
#include <nx/vms/rules/action_builder_fields/text_field.h>
#include <nx/vms/rules/event_filter_fields/customizable_text_field.h>
#include <nx/vms/rules/event_filter_fields/text_field.h>

#include "field_picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used for data types that could be represented as a one line text. The actual
 * implementation is depends on the field descriptor set.
 * Has implementations for event fields:
 * - customizableText
 * - expectedString
 *
 * Has implementations for action fields:
 * - text
 * - password
 */

template<typename F>
class OnelineTextPickerWidgetBase: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    OnelineTextPickerWidgetBase(F* field, SystemContext* context, ParamsWidget* parent):
        PlainFieldPickerWidget<F>(field, context, parent)
    {
        auto contentLayout = new QHBoxLayout;

        m_lineEdit = new QLineEdit;
        m_lineEdit->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        contentLayout->addWidget(m_lineEdit);

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_lineEdit,
            &QLineEdit::textEdited,
            this,
            &OnelineTextPickerWidgetBase<F>::onTextChanged);
    }

protected:
    BASE_COMMON_USINGS

    QLineEdit* m_lineEdit{nullptr};

    virtual void onTextChanged(const QString& /*text*/)
    {
        setEdited();
    }
};

template<typename F>
class OnelineTextPickerWidgetCommon: public OnelineTextPickerWidgetBase<F>
{
    using base = OnelineTextPickerWidgetBase<F>;

public:
    OnelineTextPickerWidgetCommon(F* field, SystemContext* context, ParamsWidget* parent):
        OnelineTextPickerWidgetBase<F>(field, context, parent)
    {
        if (std::is_same<F, vms::rules::PasswordField>())
            m_lineEdit->setEchoMode(QLineEdit::Password);
    }

    void setValidity(const vms::rules::ValidationResult& validationResult) override
    {
        OnelineTextPickerWidgetBase<F>::setValidity(validationResult);

        if (validationResult.validity == QValidator::State::Invalid)
            setErrorStyle(m_lineEdit);
        else
            resetErrorStyle(m_lineEdit);
    }

protected:
    BASE_COMMON_USINGS
    using OnelineTextPickerWidgetBase<F>::m_lineEdit;

    void updateUi() override
    {
        OnelineTextPickerWidgetBase<F>::updateUi();

        if (m_lineEdit->text() == m_field->value())
            return;

        const QSignalBlocker blocker{m_lineEdit};
        m_lineEdit->setText(m_field->value());
    }

    void onTextChanged(const QString& text) override
    {
        m_field->setValue(text);

        OnelineTextPickerWidgetBase<F>::onTextChanged(text);
    }
};

using ActionTextPicker = OnelineTextPickerWidgetCommon<vms::rules::ActionTextField>;
using CustomizableTextPicker = OnelineTextPickerWidgetCommon<vms::rules::CustomizableTextField>;
using EventTextPicker = OnelineTextPickerWidgetCommon<vms::rules::EventTextField>;
using PasswordPicker = OnelineTextPickerWidgetCommon<vms::rules::PasswordField>;

} // namespace nx::vms::client::desktop::rules
