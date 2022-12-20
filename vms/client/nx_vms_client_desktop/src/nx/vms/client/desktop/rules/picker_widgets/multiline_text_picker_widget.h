// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QTextEdit>

#include <nx/vms/rules/action_builder_fields/text_with_fields.h>

#include "picker_widget.h"
#include "picker_widget_utils.h"

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
        FieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QHBoxLayout;

        m_textEdit = new QTextEdit;
        m_textEdit->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        contentLayout->addWidget(m_textEdit);

        m_contentWidget->setLayout(contentLayout);
    }

private:
    PICKER_WIDGET_COMMON_USINGS

    QTextEdit* m_textEdit;

    virtual void onDescriptorSet() override
    {
        FieldPickerWidget<F>::onDescriptorSet();
        m_textEdit->setPlaceholderText(m_fieldDescriptor->description);
    };

    virtual void onFieldsSet() override
    {
        {
            const QSignalBlocker blocker{m_textEdit};
            m_textEdit->setText(text());
        }

        connect(m_textEdit,
            &QTextEdit::textChanged,
            this,
            &MultilineTextPickerWidget<F>::onTextChanged,
            Qt::UniqueConnection);
    }

    void onTextChanged()
    {
        setText(m_textEdit->toPlainText());
    }

    QString text();
    void setText(const QString& text);
};

using TextWithFieldsPicker = MultilineTextPickerWidget<vms::rules::TextWithFields>;

template<>
QString TextWithFieldsPicker::text()
{
    return m_field->text();
}

template<>
void TextWithFieldsPicker::setText(const QString& text)
{
    m_field->setText(text);
}

} // namespace nx::vms::client::desktop::rules
