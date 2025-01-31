// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QTextEdit>

#include <nx/vms/rules/action_builder_fields/http_headers_field.h>

#include "field_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class HttpHeadersPickerWidget: public PlainFieldPickerWidget<vms::rules::HttpHeadersField>
{
    Q_OBJECT

public:
    HttpHeadersPickerWidget(
        vms::rules::HttpHeadersField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        PlainFieldPickerWidget<vms::rules::HttpHeadersField>(field, context, parent)
    {
        auto contentLayout = new QHBoxLayout{m_contentWidget};

        m_textEdit = new QTextEdit;
        m_textEdit->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        m_textEdit->setAcceptRichText(false);
        contentLayout->addWidget(m_textEdit);

        connect(
            m_textEdit,
            &QTextEdit::textChanged,
            this,
            &HttpHeadersPickerWidget::onTextChanged);
    }

protected:
    void updateUi() override
    {
        PlainFieldPickerWidget<vms::rules::HttpHeadersField>::updateUi();

        QString textToDisplay = m_textEdit->toPlainText();
        if (!textToDisplay.isEmpty())
            return;

        QStringList result;
        for (const auto& [key, value]: m_field->value())
            result << QString{"%1: %2"}.arg(key, value);

        textToDisplay = result.join('\n');

        const QSignalBlocker blocker{m_textEdit};
        m_textEdit->setPlainText(textToDisplay);
    }

private:
    QTextEdit* m_textEdit{nullptr};

    void onTextChanged()
    {
        QList<vms::rules::KeyValueObject> headers;
        const auto lines = m_textEdit->toPlainText().split('\n');
        for (const auto& line: lines)
        {
            const auto trimmed = line.trimmed();
            if (trimmed.isEmpty())
                continue;

            const auto firstColonIndex = trimmed.indexOf(":");
            if (firstColonIndex <= 0) //< Empty header name is not allowed.
                continue;

            vms::rules::KeyValueObject header;
            header.key = trimmed.first(firstColonIndex).trimmed();
            if (firstColonIndex < trimmed.size())
                header.value = trimmed.sliced(firstColonIndex + 1).trimmed();

            headers.push_back(header);
        }

        m_field->setValue(headers);
    }
};

} // namespace nx::vms::client::desktop::rules
