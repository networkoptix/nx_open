// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "picker_widget.h"

#include "ui_multiline_text_picker_widget.h"

#include <nx/vms/rules/action_fields/text_with_fields.h>

namespace nx::vms::client::desktop::rules {

/**
 * Used for data types that could be represented as a multiline text. The actual
 * implementation is depends on the field descriptor set.
 * Has implementations for:
 * - nx.actions.fields.textWithFields
 */
template<typename F>
class MultilineTextPickerWidget: public FieldPickerWidget<F>
{
public:
    explicit MultilineTextPickerWidget(QWidget* parent = nullptr):
        FieldPickerWidget<F>(parent),
        ui(new Ui::MultilineTextPickerWidget)
    {
        ui->setupUi(this);
    }

    virtual void setReadOnly(bool value) override
    {
        ui->textEdit->setReadOnly(value);
    };

private:
    using FieldPickerWidget<F>::connect;
    using FieldPickerWidget<F>::edited;
    using FieldPickerWidget<F>::fieldDescriptor;
    using FieldPickerWidget<F>::field;

    QScopedPointer<Ui::MultilineTextPickerWidget> ui;

    virtual void onDescriptorSet() override
    {
        ui->label->setText(fieldDescriptor->displayName);
        ui->textEdit->setPlaceholderText(fieldDescriptor->description);
    };

    virtual void onFieldSet() override
    {
        ui->textEdit->setText(text<F>());

        connect(ui->textEdit, &QTextEdit::textChanged, this,
            [this]
            {
                setText<F>(ui->textEdit->toPlainText());
                emit edited();
            });
    }

    template<typename T>
    QString text();

    template<typename T>
    void setText(const QString& text);

    template<>
    QString text<vms::rules::TextWithFields>()
    {
        return field->text();
    }

    template<>
    void setText<vms::rules::TextWithFields>(const QString& text)
    {
        field->setText(text);
    }
};

} // namespace nx::vms::client::desktop::rules
