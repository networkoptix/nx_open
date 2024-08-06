// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields_picker_widget.h"

#include <QTextCursor>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/rules/model_view/event_parameters_model.h>
#include <nx/vms/rules/event_filter_fields/analytics_object_type_field.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/field_validator.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>
#include <nx/vms/rules/utils/field.h>

#include "../utils/event_parameter_completer.h"

namespace nx::vms::client::desktop::rules {

using namespace vms::rules::utils;

struct TextWithFieldsPicker::Private: public QObject
{
    struct EventTrackableData
    {
        QString objectType;
        QString eventId;
        api::rules::State eventState = api::rules::State::none;
        bool operator==(const EventTrackableData&) const = default;
    };

    EventParameterCompleter* completer{nullptr};
    TextWithFieldsPicker* const q;
    EventTrackableData currentEventData;

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

    QTextCharFormat getDefaultCharFormat(QTextCharFormat charFormat) const
    {
        charFormat.setForeground(q->palette().text());
        charFormat.setUnderlineStyle(QTextCharFormat::UnderlineStyle::NoUnderline);
        return charFormat;
    }

    void addFormattingToText(const vms::rules::TextWithFields::ParsedValues& parsedValues)
    {
        if (q->m_textEdit->toPlainText().isEmpty())
            return;

        auto textCursor = q->m_textEdit->textCursor();
        const auto currentCharFormat = textCursor.charFormat();
        const auto defaultCharFormat = getDefaultCharFormat(currentCharFormat);
        const auto correctCharFormat = getCorrectCharFormat(defaultCharFormat);
        const auto incorrectCharFormat = getIncorrectCharFormat(defaultCharFormat);

        // To change color of part of text from QTextEditor and keep normal behaviour of cursor,
        // have to create a selection by copied cursor, and change char format of selection.
        // The original cursor and text from QTextEditor kept intact.
        for (auto& val: parsedValues)
        {
            QTextCursor cursor = textCursor;
            cursor.setPosition(val.start);
            cursor.setPosition(val.start + val.length, QTextCursor::KeepAnchor);

            if (val.type == vms::rules::TextWithFields::FieldType::Substitution)
                cursor.setCharFormat(val.isValid ? correctCharFormat : incorrectCharFormat);
            else
                cursor.setCharFormat(defaultCharFormat);
        }

        // Resetting char format, to avoid overlapping of char format outside the selection.
        textCursor.setCharFormat(currentCharFormat);
        q->m_textEdit->setTextCursor(textCursor);
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

    void updateCompleterModel(const vms::rules::ItemDescriptor& eventDesc)
    {
        EventTrackableData newData{.objectType = getObjectTypeFieldValue(),
            .eventId = eventDesc.id,
            .eventState = getStateFieldValue()};

        if (currentEventData == newData)
            return;

        currentEventData = newData;

        const auto model = new EventParametersModel(
            EventParameterHelper::getVisibleEventParameters(currentEventData.eventId,
                q->common::SystemContextAware::systemContext(),
                currentEventData.objectType,
                currentEventData.eventState));
        completer->setModel(model);
    }
};

TextWithFieldsPicker::TextWithFieldsPicker(
    vms::rules::TextWithFields* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    base(field, context, parent),
    d(new Private(this))
{
    d->completer = new EventParameterCompleter{new EventParametersModel({}), m_textEdit, this};
}

TextWithFieldsPicker::~TextWithFieldsPicker()
{
}

void TextWithFieldsPicker::updateUi()
{
    const auto eventDescriptor = getEventDescriptor();
    if (!eventDescriptor)
        return;

    base::updateUi();
    d->updateCompleterModel(eventDescriptor.value());
    m_field->parseText();
    if (auto validator = fieldValidator())
        setValidity(validator->validity(m_field, parentParamsWidget()->rule(), systemContext()));
}

void TextWithFieldsPicker::setValidity(const vms::rules::ValidationResult& validationResult)
{
    if (!NX_ASSERT(validationResult.additionalData
        .canConvert<vms::rules::TextWithFields::ParsedValues>(), "Invalid input data"))
    {
        return;
    }

    d->addFormattingToText(
        validationResult.additionalData.value<vms::rules::TextWithFields::ParsedValues>());
}

} // namespace nx::vms::client::desktop::rules
