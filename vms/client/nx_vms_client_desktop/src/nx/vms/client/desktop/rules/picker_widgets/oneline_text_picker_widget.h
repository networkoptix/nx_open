// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/action_builder_fields/password_field.h>
#include <nx/vms/rules/action_builder_fields/text_field.h>
#include <nx/vms/rules/event_filter_fields/customizable_text_field.h>
#include <nx/vms/rules/event_filter_fields/keywords_field.h>
#include <nx/vms/rules/event_filter_fields/text_field.h>
#include <ui/widgets/common/elided_label.h>

#include "picker_widget.h"

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
        auto mainLayout = new QHBoxLayout;
        mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        label = new QnElidedLabel;
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        mainLayout->addWidget(label);

        lineEdit = createLineEdit();
        lineEdit->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        mainLayout->addWidget(lineEdit);

        mainLayout->setStretch(0, 1);
        mainLayout->setStretch(1, 5);

        setLayout(mainLayout);
    }

    virtual void setReadOnly(bool value) override
    {
        lineEdit->setReadOnly(value);
    }

private:
    using FieldPickerWidget<F>::connect;
    using FieldPickerWidget<F>::setLayout;
    using FieldPickerWidget<F>::fieldDescriptor;
    using FieldPickerWidget<F>::field;

    QnElidedLabel* label{};
    QLineEdit* lineEdit{};

    virtual void onDescriptorSet() override
    {
        label->setText(fieldDescriptor->displayName);
        lineEdit->setPlaceholderText(fieldDescriptor->description);
    };

    virtual void onFieldsSet() override
    {
        {
            const QSignalBlocker blocker{lineEdit};
            lineEdit->setText(text());
        }

        connect(lineEdit,
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
        return field->value();
    }

    void setText(const QString& text)
    {
        field->setValue(text);
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
    return field->string();
}

template<>
void KeywordsPicker::setText(const QString& text)
{
    field->setString(text);
}

} // namespace nx::vms::client::desktop::rules
