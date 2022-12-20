// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>

#include <nx/vms/rules/action_builder_fields/password_field.h>
#include <nx/vms/rules/action_builder_fields/text_field.h>
#include <nx/vms/rules/event_filter_fields/customizable_text_field.h>
#include <nx/vms/rules/event_filter_fields/keywords_field.h>
#include <nx/vms/rules/event_filter_fields/text_field.h>

#include "picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used for data types that could be represented as a one line text. The actual
 * implementation is depends on the field descriptor set.
 * Has implementations for:
 * - nx.events.fields.customizableText
 * - nx.events.fields.expectedString
 * - nx.events.fields.keywords
 *
 * - nx.actions.fields.text
 * - nx.actions.fields.password
 */
template<typename F>
class OnelineTextPickerWidget: public FieldPickerWidget<F>
{
public:
    OnelineTextPickerWidget(common::SystemContext* context, QWidget* parent = nullptr):
        FieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QHBoxLayout;

        m_lineEdit = createLineEdit();
        m_lineEdit->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        contentLayout->addWidget(m_lineEdit);

        m_contentWidget->setLayout(contentLayout);
    }

private:
    PICKER_WIDGET_COMMON_USINGS

    QLineEdit* m_lineEdit{};

    virtual void onDescriptorSet() override
    {
        FieldPickerWidget<F>::onDescriptorSet();
        m_lineEdit->setPlaceholderText(m_fieldDescriptor->description);
    };

    virtual void onFieldsSet() override
    {
        {
            const QSignalBlocker blocker{m_lineEdit};
            m_lineEdit->setText(text());
        }

        connect(m_lineEdit,
            &QLineEdit::textEdited,
            this,
            &OnelineTextPickerWidget<F>::onTextChanged,
            Qt::UniqueConnection);
    }

    void onTextChanged(const QString& text)
    {
        setText(text);
    }

    QString text() const
    {
        return m_field->value();
    }

    void setText(const QString& text)
    {
        m_field->setValue(text);
    }

    QLineEdit* createLineEdit()
    {
        return new QLineEdit;
    }
};

using ActionTextPicker = OnelineTextPickerWidget<vms::rules::ActionTextField>;
using CustomizableTextPicker = OnelineTextPickerWidget<vms::rules::CustomizableTextField>;
using EventTextPicker = OnelineTextPickerWidget<vms::rules::EventTextField>;
using KeywordsPicker = OnelineTextPickerWidget<vms::rules::KeywordsField>;
using PasswordPicker = OnelineTextPickerWidget<vms::rules::PasswordField>;

template<>
QLineEdit* PasswordPicker::createLineEdit()
{
    auto passwordLineEdit = new QLineEdit;
    passwordLineEdit->setEchoMode(QLineEdit::Password);

    return passwordLineEdit;
}

template<>
QString KeywordsPicker::text() const
{
    return m_field->string();
}

template<>
void KeywordsPicker::setText(const QString& text)
{
    m_field->setString(text);
}

} // namespace nx::vms::client::desktop::rules
