// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "field.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/ini.h>

#include "../action_builder_fields/event_devices_field.h"
#include "../action_builder_fields/extract_detail_field.h"
#include "../action_builder_fields/flag_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../event_filter_fields/state_field.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

template <class T>
FieldDescriptor makeInvisibleFieldDescriptor(const QString& fieldName,
    const TranslatableString& displayName,
    const QString& description,
    QVariantMap properties)
{
    properties["visible"] = false;
    return FieldDescriptor{.id = fieldMetatype<T>(),
        .fieldName = fieldName,
        .displayName = displayName,
        .description = description,
        .properties = properties};
}

// Invisible types
template<>
FieldDescriptor makeFieldDescriptor<EmailMessageField>(const QString& fieldName,
    const TranslatableString& displayName,
    const QString& description,
    const QVariantMap& properties)
{
    return makeInvisibleFieldDescriptor<EmailMessageField>(
        fieldName, displayName, description, properties);
}

template<>
FieldDescriptor makeFieldDescriptor<EventDevicesField>(const QString& fieldName,
    const TranslatableString& displayName,
    const QString& description,
    const QVariantMap& properties)
{
    return makeInvisibleFieldDescriptor<EventDevicesField>(
        fieldName, displayName, description, properties);
}

} // namespace nx::vms::rules

namespace nx::vms::rules::utils {

FieldDescriptor makeIntervalFieldDescriptor(
    const TranslatableString& displayName,
    const QString& description,
    const QVariantMap& properties)
{
    return makeTimeFieldDescriptor<OptionalTimeField>(
        kIntervalFieldName,
        displayName,
        description.isEmpty()
            ? QString{"Aggregation interval. <br/>"
                 "Limits the number of events that can trigger an action. <br/>"
                 "<b>For example</b>, if the value is set to 30 seconds, "
                 "the action will only be triggered <b>once</b> every 30 seconds, "
                 "regardless of how many events occur during that time."}
            : description,
        properties.isEmpty()
            ? TimeFieldProperties{.value = 1min, .minimumValue = 1s}.toVariantMap()
            : properties);
}

FieldDescriptor makeDurationFieldDescriptor(const QVariantMap& properties,
    const TranslatableString& displayName,
    const QString& description)
{
    return utils::makeTimeFieldDescriptor<OptionalTimeField>(
        kDurationFieldName,
        displayName,
        description.isEmpty()
            ? QString(
             "Duration of action. "
             "If the value is set to zero, the duration will be set to the duration of the event.")
            : description,
        properties);
}

FieldDescriptor makePlaybackFieldDescriptor(
    const TranslatableString& displayName,
    const QString& description)
{
    return makeTimeFieldDescriptor<OptionalTimeField>(
        kPlaybackTimeFieldName,
        displayName,
        description.isEmpty()
            ? QString("The amount of time before "
              "the current time when the navigation time should be set.")
            : description,
        TimeFieldProperties{
            .value = 0s,
            .maximumValue = 300s,
            .minimumValue = 0s}.toVariantMap());
}

FieldDescriptor makeStateFieldDescriptor(
    const TranslatableString& displayName,
    const QString& description,
    vms::rules::State defaultState)
{
    return makeFieldDescriptor<StateField>(
        kStateFieldName,
        displayName,
        description,
        {{"value", QVariant::fromValue(defaultState)}});
}

FieldDescriptor makeExtractDetailFieldDescriptor(
    const QString& fieldName,
    const QString& detailName)
{
    return makeFieldDescriptor<ExtractDetailField>(
        fieldName,
        {},
        {},
        {{ "detailName", detailName }, {"visible", false}});
}

FieldDescriptor makeNotificationTextWithFieldsDescriptor(const QString& fieldName,
    bool isVisibilityConfigurable,
    QString defaultText,
    TranslatableString displayName,
    QString description)
{
    const bool visibility = isVisibilityConfigurable ? ini().showHiddenTextFields : true;
    if (fieldName == kCaptionFieldName)
    {
        defaultText = defaultText.isEmpty() ? QString("{event.caption}") : defaultText;
        displayName = displayName.empty() ? NX_DYNAMIC_TRANSLATABLE(BasicAction::tr("Caption"))
                                          : displayName;
        description = QString("Notification title, displayed on the event tile.");
    }
    else if (fieldName == kDescriptionFieldName)
    {
        defaultText = defaultText.isEmpty() ? QString("{event.description}") : defaultText;
        displayName = displayName.empty() ? NX_DYNAMIC_TRANSLATABLE(BasicAction::tr("Description"))
                                          : displayName;
        description = QString("Description, displayed on the event tile.");
    }
    else if (fieldName == kTooltipFieldName)
    {
        defaultText = defaultText.isEmpty() ? QString("{event.extendedDescription}") : defaultText;
        displayName = displayName.empty()
            ? NX_DYNAMIC_TRANSLATABLE(BasicAction::tr("Tooltip text"))
            : displayName;
        description = QString("Text displayed in the event tile's tooltip.");
    }
    else
    {
        NX_ASSERT(false, "Unexpected field %1", fieldName);
    }
    return makeFieldDescriptor<TextWithFields>(
        fieldName, displayName, description, {{"text", defaultText}, {"visible", visibility}});
}

FieldDescriptor makeActionFlagFieldDescriptor(
    const QString& fieldName,
    const TranslatableString& displayName,
    const QString& description,
    bool defaultValue)
{
    return makeFieldDescriptor<ActionFlagField>(
        fieldName, displayName, description, {{"value", defaultValue}});
}

} // namespace nx::vms::rules::utils
