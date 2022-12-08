// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "picker_widget.h"

#include "ui_multiline_text_picker_widget.h"

#include <nx/vms/rules/action_builder_fields/text_with_fields.h>

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
    MultilineTextPickerWidget(common::SystemContext* context, QWidget* parent = nullptr):
        FieldPickerWidget<F>(context, parent),
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
    using FieldPickerWidget<F>::fieldDescriptor;
    using FieldPickerWidget<F>::field;

    QScopedPointer<Ui::MultilineTextPickerWidget> ui;

    virtual void onDescriptorSet() override
    {
        ui->label->setText(fieldDescriptor->displayName);
        ui->textEdit->setPlaceholderText(fieldDescriptor->description);
    };

    virtual void onFieldsSet() override
    {
        {
            const QSignalBlocker blocker{ui->textEdit};
            ui->textEdit->setText(text());
        }

        connect(ui->textEdit,
            &QTextEdit::textChanged,
            this,
            &MultilineTextPickerWidget<F>::onTextChanged,
            Qt::UniqueConnection);
    }

    void onTextChanged()
    {
        setText(ui->textEdit->toPlainText());
    }

    QString text();
    void setText(const QString& text);
};

using TextWithFieldsPicker = MultilineTextPickerWidget<vms::rules::TextWithFields>;

template<>
QString TextWithFieldsPicker::text()
{
    return field->text();
}

template<>
void TextWithFieldsPicker::setText(const QString& text)
{
    field->setText(text);
}

} // namespace nx::vms::client::desktop::rules
