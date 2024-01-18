// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields_picker_widget.h"

#include <QTextCursor>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/rules/model_view/event_parameters_model.h>
#include <nx/vms/rules/event_filter_fields/analytics_object_type_field.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
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
    api::rules::State eventState;

    Private(TextWithFieldsPicker* parent): q(parent){};

    QTextCharFormat getIncorrectCharFormat(QTextCharFormat charFormat) const
    {
        const auto warningColor = core::colorTheme()->color("red_d3");
        charFormat.setUnderlineStyle(QTextCharFormat::UnderlineStyle::WaveUnderline);
        charFormat.setUnderlineColor(warningColor);
        charFormat.setForeground(q->palette().text());
        return charFormat;
    }

    QTextCharFormat getCorrectCharFormat(QTextCharFormat charFormat) const
    {
        const auto highlightColor = core::colorTheme()->color("brand_core");
        QBrush brush = q->palette().text();
        brush.setColor(highlightColor);
        charFormat.setForeground(brush);
        charFormat.setUnderlineStyle(QTextCharFormat::UnderlineStyle::NoUnderline);
        return charFormat;
    }

    void addFormattingToText(const vms::rules::TextWithFields::ParsedValues& parsedValues)
    {
        if (!NX_ASSERT(q->theField()))
            return;

        if (q->m_textEdit->toPlainText().isEmpty())
            return;

        auto textCursor = q->m_textEdit->textCursor();
        const auto charFormat = textCursor.charFormat();
        const auto correctCharFormat = getCorrectCharFormat(charFormat);
        const auto incorrectCharFromat = getIncorrectCharFormat(charFormat);

        // To change color of part of text from QTextEditor and keep normal behaviour of cursor,
        // have to create a selection by copied cursor, and change char format of selection.
        // The original cursor and text from QTextEditor kept intact.
        for (auto& val: parsedValues)
        {
            if (val.type != vms::rules::TextWithFields::FieldType::Substitution)
                continue;

            QTextCursor cursor = textCursor;
            cursor.setPosition(val.start);
            cursor.setPosition(val.start + val.length, QTextCursor::KeepAnchor);

            const bool isCorrectValue = completer->containsElement(val.value);
            cursor.setCharFormat(isCorrectValue ? correctCharFormat : incorrectCharFromat);
        }

        // Resetting char format, to avoid overlapping of char format outside the selection.
        textCursor.setCharFormat({});
        q->m_textEdit->setTextCursor(textCursor);
    }

    void setCompleterModel(const vms::rules::ItemDescriptor& desc)
    {
        const auto model =
            new EventParametersModel(EventParameterHelper::getVisibleEventParameters(
                desc.id, q->common::SystemContextAware::systemContext(), objectType, eventState));
        completer->setModel(model);
    }

    QString getObjectTypeFieldValue() const
    {
        const auto objectTypeField =
            q->getEventField<vms::rules::AnalyticsObjectTypeField>(kObjectTypeIdFieldName);
        return objectTypeField ? objectTypeField->value() : QString();
    }

    api::rules::State getStateFieldValue() const
    {
        const auto stateField =
            q->getEventField<vms::rules::StateField>(kStateFieldName);
        return stateField ? stateField->value() : api::rules::State::none;
    }

    void updateCompleterModel(const std::optional<vms::rules::ItemDescriptor>& desc)
    {
        if (!desc)
            return;

        const auto newObjectTypeValue = getObjectTypeFieldValue();
        const auto newStateValue = getStateFieldValue();

        // Updating completer model only if AnalyticsObjectType or Event State was changed.
        if (newObjectTypeValue == objectType && newStateValue == eventState)
            return;

        objectType = newObjectTypeValue;
        eventState = newStateValue;

        setCompleterModel(desc.value());
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

    base::updateUi();
    d->updateCompleterModel(desc);
    theField()->parseText();
}

} // namespace nx::vms::client::desktop::rules
