// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "initializer.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <common/common_module.h>
#include <nx/vms/rules/action_fields/flag_field.h>
#include <nx/vms/rules/action_fields/optional_time_field.h>
#include <nx/vms/rules/action_fields/substitution.h>
#include <nx/vms/rules/action_fields/target_user_field.h>
#include <nx/vms/rules/action_fields/text_field.h>
#include <nx/vms/rules/action_fields/text_with_fields.h>
#include <nx/vms/rules/event_fields/analytics_event_type_field.h>
#include <nx/vms/rules/event_fields/analytics_object_type_field.h>
#include <nx/vms/rules/event_fields/customizable_icon.h>
#include <nx/vms/rules/event_fields/customizable_text.h>
#include <nx/vms/rules/event_fields/expected_string_field.h>
#include <nx/vms/rules/event_fields/expected_uuid_field.h>
#include <nx/vms/rules/event_fields/keywords_field.h>
#include <nx/vms/rules/event_fields/source_camera_field.h>
#include <nx/vms/rules/event_fields/source_server_field.h>
#include <nx/vms/rules/event_fields/source_user_field.h>
#include <nx/vms/rules/actions/send_email_action.h>

namespace nx::vms::rules {

namespace {

ItemDescriptor engineItemDescriptorFromJson(const QJsonValue& json)
{
    const auto eventTypeObject = json.toObject();

    ItemDescriptor descriptor;

    descriptor.id = eventTypeObject["id"].toString();
    descriptor.displayName = eventTypeObject["name"].toString();
    descriptor.description = eventTypeObject["description"].toString();

    const auto fieldsArray = eventTypeObject["fields"].toArray();

    for (auto fieldIterator = fieldsArray.cbegin(); fieldIterator != fieldsArray.cend();
        ++fieldIterator)
    {
        const auto fieldObject = fieldIterator->toObject();

        FieldDescriptor fieldDescriptor;
        fieldDescriptor.id = fieldObject["id"].toString();
        fieldDescriptor.displayName = fieldObject["displayName"].toString();
        fieldDescriptor.description = fieldObject["description"].toString();
        fieldDescriptor.fieldName = fieldObject["fieldName"].toString();
        fieldDescriptor.properties = fieldObject["properties"].toObject().toVariantMap();

        descriptor.fields.push_back(fieldDescriptor);
    }

    return descriptor;
}

} // namespace

Initializer::Initializer(Engine* engine):
    Plugin(engine)
{
}

Initializer::~Initializer()
{
}

void Initializer::registerEvents() const
{
    // Register built-in events.
    const auto eventTypesManifest = R"json(
        {
            "eventTypes":
            [
                {
                    "id": "nx.events.motion",
                    "name": "Motion Event",
                    "description": "Motion event description",
                    "fields":
                    [
                        {
                            "id": "nx.events.fields.sourceCamera",
                            "displayName": "at",
                            "description": "Cameras id's",
                            "fieldName": "sources",
                            "properties":
                            {
                                "initialValue": []
                            }
                        }
                    ]
                }
            ]
        }
    )json";

    const auto eventTypesDocument = QJsonDocument::fromJson(eventTypesManifest);
    const auto eventTypesObject = eventTypesDocument.object();
    const auto eventTypesArray = eventTypesObject["eventTypes"].toArray();

    for (auto eventIterator = eventTypesArray.cbegin(); eventIterator != eventTypesArray.cend();
        ++eventIterator)
    {
        const auto eventTypeObject = eventIterator->toObject();

        const ItemDescriptor descriptor = engineItemDescriptorFromJson(eventTypeObject);

        if (!m_engine->registerEvent(descriptor))
        {
            // TODO: Log the registration error.
        }
    }
}

void Initializer::registerActions() const
{
    // Register built-in actions.
    const auto actionTypesManifest = R"json(
        {
            "actionTypes":
            [
                {
                    "id": "nx.actions.showNotification",
                    "name": "Show Notification",
                    "description": "Show Notification Action",
                    "fields":
                    [
                        {
                            "id": "nx.actions.fields.textWithFields",
                            "displayName": "Caption",
                            "description": "Notification caption",
                            "fieldName": "caption",
                            "properties":
                            {
                                "placeholder": "Enter notification caption",
                                "initialValue": "",
                                "validator": ""
                            }
                        },
                        {
                            "id": "nx.actions.fields.textWithFields",
                            "displayName": "Description",
                            "description": "Notification description",
                            "fieldName": "description",
                            "properties":
                            {
                                "placeholder": "Enter notification caption",
                                "initialValue": "",
                                "validator": ""
                            }
                        }
                    ]
                }
            ]
        }
    )json";

    const auto actionTypesDocument = QJsonDocument::fromJson(actionTypesManifest);
    const auto actionTypesObject = actionTypesDocument.object();
    const auto actionTypesArray = actionTypesObject["actionTypes"].toArray();

    for (auto actionIterator = actionTypesArray.cbegin(); actionIterator != actionTypesArray.cend();
        ++actionIterator)
    {
        const auto actionTypeObject = actionIterator->toObject();

        const ItemDescriptor descriptor = engineItemDescriptorFromJson(actionTypeObject);

        if (!m_engine->registerAction(descriptor))
        {
            // TODO: Log the registration error.
        }
    }

    registerAction<SendEmailAction>();
}

void Initializer::registerFields() const
{
    registerEventField<AnalyticsEventTypeField>();
    registerEventField<AnalyticsObjectTypeField>();
    registerEventField<CustomizableIcon>();
    registerEventField<CustomizableText>();
    registerEventField<ExpectedStringField>();
    registerEventField<ExpectedUuidField>();
    registerEventField<KeywordsField>();
    registerEventField<SourceCameraField>();
    registerEventField<SourceServerField>();
    registerEventField<SourceUserField>();

    registerActionField<FlagField>();
    registerActionField<OptionalTimeField>();
    registerActionField<TargetUserField>();
    registerActionField<TextField>();
    registerActionField<TextWithFields>();
    registerActionField<Substitution>();
}

} // namespace nx::vms::rules
