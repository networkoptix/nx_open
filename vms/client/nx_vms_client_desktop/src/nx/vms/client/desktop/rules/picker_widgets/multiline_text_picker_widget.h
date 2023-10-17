// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QTextEdit>

#include <nx/vms/rules/action_builder_fields/text_with_fields.h>

#include "../utils/completer.h"
#include "picker_widget.h"
#include "picker_widget_utils.h"
#include "plain_picker_widget.h"

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
    MultilineTextPickerWidget(SystemContext* context, CommonParamsWidget* parent):
        base(context, parent)
    {
        auto contentLayout = new QHBoxLayout;

        m_textEdit = new QTextEdit;
        m_textEdit->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        contentLayout->addWidget(m_textEdit);

        m_completer = new Completer{wordsToComplete(), m_textEdit, this};

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_textEdit,
            &QTextEdit::textChanged,
            this,
            &MultilineTextPickerWidget<F>::onTextChanged);
    }

private:
    BASE_COMMON_USINGS

    QTextEdit* m_textEdit{nullptr};
    Completer* m_completer{nullptr};

    virtual void onDescriptorSet() override
    {
        base::onDescriptorSet();
        m_textEdit->setPlaceholderText(m_fieldDescriptor->description);
    };

    void updateUi() override
    {
        if (m_textEdit->toPlainText() == theField()->text())
            return;

        const QSignalBlocker blocker{m_textEdit};
        m_textEdit->setPlainText(theField()->text());
    }

    void onTextChanged()
    {
        theField()->setText(m_textEdit->toPlainText());
    }

    QStringList wordsToComplete() const
    {
        return {};
    }
};

using TextWithFieldsPicker = MultilineTextPickerWidget<vms::rules::TextWithFields>;

template<>
QStringList TextWithFieldsPicker::wordsToComplete() const
{
    return vms::rules::TextWithFields::availableFunctions();
}

} // namespace nx::vms::client::desktop::rules
