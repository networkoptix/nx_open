// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "picker_widget.h"

#include "ui_oneline_text_picker_widget.h"

#include <nx/vms/rules/action_fields/text_field.h>
#include <nx/vms/rules/event_fields/customizable_text_field.h>
#include <nx/vms/rules/event_fields/expected_string_field.h>
#include <nx/vms/rules/event_fields/keywords_field.h>
#include <nx/vms/rules/event_fields/text_field.h>

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
 */
template<typename F>
class OnelineTextPickerWidget: public FieldPickerWidget<F>
{
public:
    explicit OnelineTextPickerWidget(QWidget* parent = nullptr):
        FieldPickerWidget<F>(parent),
        ui(new Ui::OnelineTextPickerWidget)
    {
        ui->setupUi(this);
    }

    virtual void setReadOnly(bool value) override
    {
        ui->lineEdit->setReadOnly(value);
    }

private:
    using FieldPickerWidget<F>::connect;
    using FieldPickerWidget<F>::edited;
    using FieldPickerWidget<F>::fieldDescriptor;
    using FieldPickerWidget<F>::field;

    QScopedPointer<Ui::OnelineTextPickerWidget> ui;

    virtual void onDescriptorSet() override
    {
        ui->label->setText(fieldDescriptor->displayName);
        ui->lineEdit->setPlaceholderText(fieldDescriptor->description);
    };

    virtual void onFieldSet() override
    {
        ui->lineEdit->setText(text<F>());

        connect(ui->lineEdit, &QLineEdit::textEdited, this,
            [this](const QString& text)
            {
                setText<F>(text);
                emit edited();
            });
    }

    template<typename T>
    QString text() const
    {
        return field->value();
    }

    template<typename T>
    void setText(const QString& text)
    {
        field->setValue(text);
    }

    template<>
    QString text<vms::rules::CustomizableTextField>() const
    {
        return field->text();
    }

    template<>
    void setText<vms::rules::CustomizableTextField>(const QString& text)
    {
        field->setText(text);
    }

    template<>
    QString text<vms::rules::ExpectedStringField>() const
    {
        return field->expected();
    }

    template<>
    void setText<vms::rules::ExpectedStringField>(const QString& text)
    {
        field->setExpected(text);
    }

    template<>
    QString text<vms::rules::KeywordsField>() const
    {
        return field->string();
    }

    template<>
    void setText<vms::rules::KeywordsField>(const QString& text)
    {
        field->setString(text);
    }
};

} // namespace nx::vms::client::desktop::rules
