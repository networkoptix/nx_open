// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields_picker_widget.h"

#include <range/v3/view/reverse.hpp>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/rules/model_view/event_parameters_model.h>
#include <nx/vms/common/html/html.h>

#include "../utils/completer.h"

namespace nx::vms::client::desktop::rules {

struct TextWithFieldsPicker::Private: public QObject
{
    Completer* completer{nullptr};
    bool connected = false;
    TextWithFieldsPicker* const q;

    Private(TextWithFieldsPicker* parent): q(parent){};
    void addFormattingToText(const nx::vms::rules::TextWithFields::ParsedValues& parsedValues)
    {
        QString formattedText = q->theField()->text();
        const auto highlightColor = nx::vms::client::core::colorTheme()->color("brand_core");
        const auto warningColor = nx::vms::client::core::colorTheme()->color("red_d3");

        for (auto& val: parsedValues | ranges::views::reverse)
        {
            if (val.type != nx::vms::rules::TextWithFields::FieldType::Substitution)
                continue;

            const QString valueInBrackets = "{" + val.value + "}";
            const auto htmlSubstitution = val.correct
                ? nx::vms::common::html::colored(valueInBrackets, highlightColor)
                : nx::vms::common::html::underlinedColored(valueInBrackets, warningColor);

            formattedText.replace(val.start, val.length, htmlSubstitution);
        }
        const QSignalBlocker blocker{q->m_textEdit};

        // To avoid reseting of cursor position need to save it,
        // to restore it after changing of text.
        auto cursorPos = q->m_textEdit->textCursor().position();

        q->m_textEdit->setHtml(formattedText);

        auto cursor = q->m_textEdit->textCursor();
        cursor.setPosition(cursorPos, QTextCursor::MoveAnchor);
        q->m_textEdit->setTextCursor(cursor);
    }
};

TextWithFieldsPicker::TextWithFieldsPicker(SystemContext* context, CommonParamsWidget* parent):
    base(context, parent),
    d(new Private(this))
{
    d->completer = new Completer{
        new EventParametersModel(vms::rules::TextWithFields::substitutionsByGroup(), this),
        m_textEdit,
        this};
}

TextWithFieldsPicker::~TextWithFieldsPicker(){}


void TextWithFieldsPicker::updateUi()
{
    if (!d->connected && theField())
    {
        connect(theField(),
            &nx::vms::rules::TextWithFields::parseFinished,
            d.get(),
            &TextWithFieldsPicker::Private::addFormattingToText);
        d->connected = true;
    }

    theField()->parseText();

    base::updateUi();
}

} // namespace nx::vms::client::desktop::rules
