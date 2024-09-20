// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/assert.h>
#include <nx/utils/uuid.h>
#include <nx/vms/rules/action_builder_fields/email_message_field.h>
#include <nx/vms/rules/action_builder_fields/event_devices_field.h>
#include <nx/vms/rules/event_filter_fields/unique_id_field.h>
#include <nx/vms/rules/utils/openapi_doc.h>

#include "../field.h"
#include "../manifest.h"
#include "../strings.h"
#include "field_names.h"

namespace nx::vms::rules {

template<class T>
FieldDescriptor makeFieldDescriptor(
    const QString& fieldName,
    const TranslatableString& displayName,
    const QString& description = {},
    const QVariantMap& properties = {})
{
    return FieldDescriptor{
        .id = fieldMetatype<T>(),
        .fieldName = fieldName,
        .displayName = displayName,
        .description = description,
        .properties = utils::addOpenApiToProperties<T>(properties)
    };
}

// The template specialization must be declared in the header because MSVC produces a linkage error.
// Using the inline keyword is not a viable solution, as the specialization is ignored in this case.
// TODO: #vbutkevich Delete this declarations after the CI is updated to use a newer version of MSVC.

template<>
NX_VMS_RULES_API FieldDescriptor makeFieldDescriptor<EmailMessageField>(const QString& fieldName,
    const TranslatableString& displayName,
    const QString& description,
    const QVariantMap& properties);

template<>
NX_VMS_RULES_API FieldDescriptor makeFieldDescriptor<EventDevicesField>(const QString& fieldName,
    const TranslatableString& displayName,
    const QString& description,
    const QVariantMap& properties);

namespace utils {

template <class T>
FieldDescriptor makeTimeFieldDescriptor(
    const QString& fieldName,
    const TranslatableString& displayName,
    const QString& description = {},
    const QVariantMap& properties = {})
{
    return makeFieldDescriptor<T>(
        fieldName,
        displayName,
        description,
        properties);
}

NX_VMS_RULES_API FieldDescriptor makeIntervalFieldDescriptor(
    const TranslatableString& displayName,
    const QString& description = {},
    const QVariantMap& properties = {});

NX_VMS_RULES_API FieldDescriptor makeDurationFieldDescriptor(const QVariantMap& properties,
    const TranslatableString& displayName = Strings::duration(),
    const QString& description = {});

FieldDescriptor makePlaybackFieldDescriptor(
    const TranslatableString& displayName,
    const QString& description = {});

FieldDescriptor makeStateFieldDescriptor(
    const TranslatableString& displayName,
    const QString& description = {},
    vms::rules::State defaultState = vms::rules::State::started);

FieldDescriptor makeExtractDetailFieldDescriptor(
    const QString& fieldName,
    const QString& detailName);

FieldDescriptor makeNotificationTextWithFieldsDescriptor(const QString& fieldName,
    bool isVisibilityConfigurable = false,
    QString defaultText = {},
    TranslatableString displayName = {},
    QString description = {});

FieldDescriptor makeActionFlagFieldDescriptor(
    const QString& fieldName,
    const TranslatableString& displayName,
    const QString& description = {},
    bool defaultValue = false);

template <class T, class E>
T getFieldValue(const E& event, const char* fieldName, T&& defaultValue = T())
{
    if (!NX_ASSERT(event))
        return std::forward<T>(defaultValue);

    const QVariant value = event->property(fieldName);
    if (!value.isValid() || !value.canConvert<T>())
        return std::forward<T>(defaultValue);

    return value.value<T>();
}

} // namespace utils
} // namespace nx::vms::rules
