// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields_picker_widget.h"

#include <QTextCursor>

#include <nx/vms/client/core/analytics/taxonomy/attribute.h>
#include <nx/vms/client/core/analytics/taxonomy/attribute_set.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>
#include <nx/vms/client/core/analytics/taxonomy/state_view.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/rules/model_view/event_parameters_model.h>
#include <nx/vms/client/desktop/style/custom_style.h>
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

    TextWithFieldsPicker* const q;
    EventParameterCompleter* completer{nullptr};
    bool highlightErrors{true};
    EventTrackableData currentEventData;
    QPointer<core::analytics::taxonomy::StateView> taxonomy;

    Private(TextWithFieldsPicker* parent): q(parent){};

    QTextCharFormat getIncorrectCharFormat(QTextCharFormat charFormat) const
    {
        const auto warningColor = core::colorTheme()->color("red");
        charFormat.setUnderlineStyle(QTextCharFormat::UnderlineStyle::WaveUnderline);
        charFormat.setUnderlineColor(warningColor);
        charFormat.setForeground(q->palette().text());
        return charFormat;
    }

    QTextCharFormat getCorrectCharFormat(QTextCharFormat charFormat) const
    {
        const auto highlightColor = core::colorTheme()->color("brand");
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
            {
                // TODO: #vbutkevich Move value validation to TextWithFieldsValidator once it
                // supports collection of all attributes.
                cursor.setCharFormat(completer->containsElement(val.value)
                    ? correctCharFormat
                    : incorrectCharFormat);
            }
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

    QStringList collectAttributeNames(const QString& objectTypeId)
    {
        if (objectTypeId.isEmpty())
            return {};

        if (!NX_ASSERT(!taxonomy.isNull(), "Taxonomy must be initialized"))
            return {};

        const auto objectType = taxonomy->objectTypeById(objectTypeId);
        if (objectType == nullptr)
            return {};

        using namespace core::analytics::taxonomy;

        QStringList attributeNames;
        std::function<void(const std::vector<Attribute*>&, const QString&)> collectNames =
            [&](
                const std::vector<Attribute*>& attributes,
                const QString& parentAttributeName)
            {
                for (const auto& attribute: attributes)
                {
                    const QString fullAttributeName = parentAttributeName.isEmpty()
                        ? attribute->name
                        : parentAttributeName + "." + attribute->name;
                    attributeNames.append(fullAttributeName);

                    if (attribute->type == Attribute::Type::attributeSet)
                        collectNames(attribute->attributeSet->attributes(), fullAttributeName);
                }
            };

        collectNames(objectType->attributes(), QString{});
        return attributeNames;
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
            EventParameterHelper::instance()->getVisibleEventParameters(currentEventData.eventId,
                q->common::SystemContextAware::systemContext(),
                currentEventData.objectType,
                currentEventData.eventState,
                collectAttributeNames(currentEventData.objectType)));
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
    d->highlightErrors = field->properties().highlightErrors;
    d->completer = new EventParameterCompleter{new EventParametersModel({}), m_textEdit, this};

    const auto taxonomyManager = systemContext()->taxonomyManager();
    if (!NX_CRITICAL(taxonomyManager, "Common module singletons must be initialized"))
        return;

    d->taxonomy = taxonomyManager->createStateView(d.get());

    connect(taxonomyManager,
        &core::analytics::TaxonomyManager::currentTaxonomyChanged,
        this,
        [this, taxonomyManager]()
        {
            d->taxonomy.clear();
            d->taxonomy = taxonomyManager->createStateView(d.get());
        });
}

TextWithFieldsPicker::~TextWithFieldsPicker()
{
}

void TextWithFieldsPicker::updateUi()
{
    const auto eventDescriptor = getEventDescriptor();
    if (!eventDescriptor)
        return;

    d->updateCompleterModel(eventDescriptor.value());
    m_field->parseText();

    base::updateUi();
}

void TextWithFieldsPicker::setValidity(const vms::rules::ValidationResult& validationResult)
{
    if (!NX_ASSERT(validationResult.additionalData
        .canConvert<vms::rules::TextWithFields::ParsedValues>(), "Invalid input data"))
    {
        return;
    }

    if (d->highlightErrors)
    {
        d->addFormattingToText(
            validationResult.additionalData.value<vms::rules::TextWithFields::ParsedValues>());
    }

    if (validationResult.isValid())
        resetErrorStyle(m_textEdit);
    else
        setErrorStyle(m_textEdit);

    base::setValidity(validationResult);
}

} // namespace nx::vms::client::desktop::rules
