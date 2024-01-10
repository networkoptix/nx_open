// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields_picker_widget.h"

#include <range/v3/view/reverse.hpp>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/rules/model_view/event_parameters_model.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/rules/event_filter_fields/analytics_object_type_field.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>
#include <nx/vms/rules/utils/field.h>

#include "../utils/completer.h"

namespace nx::vms::client::desktop::rules {

using namespace vms::rules::utils;

struct TextWithFieldsPicker::Private: public QObject
{
    Completer* completer{nullptr};
    bool connected = false;
    TextWithFieldsPicker* const q;
    QString objectType;

    Private(TextWithFieldsPicker* parent): q(parent){};
    void addFormattingToText(const vms::rules::TextWithFields::ParsedValues& parsedValues) const
    {
        if (!NX_ASSERT(q->theField()))
            return;
        QString formattedText = q->theField()->text();
        const auto highlightColor = core::colorTheme()->color("brand_core");
        const auto warningColor = core::colorTheme()->color("red_d3");

        for (auto& val: parsedValues | ranges::views::reverse)
        {
            if (val.type != vms::rules::TextWithFields::FieldType::Substitution)
                continue;

            const QString valueInBrackets = EventParameterHelper::addBrackets(val.value);

            const bool isCorrectValue = completer->containsElement(val.value);
            const auto htmlSubstitution = isCorrectValue
                ? common::html::colored(valueInBrackets, highlightColor)
                : common::html::underlinedColored(valueInBrackets, warningColor);

            formattedText.replace(val.start, val.length, htmlSubstitution);
        }
        const QSignalBlocker blocker{q->m_textEdit};

        // To avoid reseting of cursor position need to save it,
        // to restore it after changing of text.
        const auto cursorPos = q->m_textEdit->textCursor().position();

        q->m_textEdit->setHtml(formattedText);

        auto cursor = q->m_textEdit->textCursor();
        cursor.setPosition(cursorPos, QTextCursor::MoveAnchor);
        q->m_textEdit->setTextCursor(cursor);
    }

    void setCompleterModel(const vms::rules::ItemDescriptor& desc)
    {
        const auto model =
            new EventParametersModel(EventParameterHelper::getVisibleEventParameters(
                desc.id, q->common::SystemContextAware::systemContext(), objectType));
        completer->setModel(model);
    }

    QString getObjectTypeFieldValue() const
    {
        const auto objectTypeField =
            q->getEventField<vms::rules::AnalyticsObjectTypeField>(kObjectTypeIdFieldName);
        return objectTypeField ? objectTypeField->value() : QString();
    }

    void setAnaliticsObjectType(const std::optional<vms::rules::ItemDescriptor>& desc)
    {
        if (!desc)
            return;

        const auto newValue = getObjectTypeFieldValue();
        if (newValue == objectType)
            return;

        objectType = newValue;

        // Updating completer model only if AnalyticsObjectType was changed.
        setCompleterModel(desc.value());

        if (q->theField())
            q->theField()->parseText(); //< Need to re-parse text and add new formatting.
    }
};

TextWithFieldsPicker::TextWithFieldsPicker(SystemContext* context, CommonParamsWidget* parent):
    base(context, parent),
    d(new Private(this))
{
    d->completer = new Completer{nullptr, m_textEdit, this};
}

TextWithFieldsPicker::~TextWithFieldsPicker()
{
}

void TextWithFieldsPicker::updateUi()
{
    if (!theField())
        return;

    const auto desc = getEventDescriptor();
    if (!d->connected && desc)
    {
        connect(theField(),
            &nx::vms::rules::TextWithFields::parseFinished,
            d.get(),
            &TextWithFieldsPicker::Private::addFormattingToText);
        d->setCompleterModel(desc.value());
        d->connected = true;
    }

    theField()->parseText();
    d->setAnaliticsObjectType(desc);
    base::updateUi();
}

} // namespace nx::vms::client::desktop::rules
