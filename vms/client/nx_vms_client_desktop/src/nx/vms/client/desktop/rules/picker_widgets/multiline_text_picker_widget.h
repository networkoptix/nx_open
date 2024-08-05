// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QTextEdit>

#include <nx/vms/rules/action_builder_fields/text_with_fields.h>

#include "field_picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used for data types that could be represented as a multiline text. The actual
 * implementation is depends on the field descriptor set.
 * Has implementations for:
 * - nx.actions.fields.textWithFields
 */
template<typename F>
class MultilineTextPickerWidget: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    MultilineTextPickerWidget(F* field, SystemContext* context, ParamsWidget* parent):
        base(field, context, parent)
    {
        auto contentLayout = new QHBoxLayout;

        m_textEdit = new QTextEdit;
        m_textEdit->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        contentLayout->addWidget(m_textEdit);

        m_contentWidget->setLayout(contentLayout);

        m_textEdit->setPlaceholderText(field->descriptor()->description);

        connect(
            m_textEdit,
            &QTextEdit::textChanged,
            this,
            &MultilineTextPickerWidget<F>::onTextChanged);
    }

protected:
    QTextEdit* m_textEdit{nullptr};

    BASE_COMMON_USINGS

    virtual void updateUi() override
    {
        PlainFieldPickerWidget<F>::updateUi();

        if (m_textEdit->toPlainText() == m_field->text())
            return;

        const QSignalBlocker blocker{m_textEdit};
        m_textEdit->setPlainText(m_field->text());
    }

    void onTextChanged()
    {
        m_field->setText(m_textEdit->toPlainText());

        setEdited();
    }
};

} // namespace nx::vms::client::desktop::rules
