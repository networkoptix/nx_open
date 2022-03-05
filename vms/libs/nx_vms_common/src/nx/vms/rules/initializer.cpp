// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "initializer.h"

#include <nx/vms/rules/action_fields/flag_field.h>
#include <nx/vms/rules/action_fields/optional_time_field.h>
#include <nx/vms/rules/action_fields/substitution.h>
#include <nx/vms/rules/action_fields/target_user_field.h>
#include <nx/vms/rules/action_fields/text_field.h>
#include <nx/vms/rules/action_fields/text_with_fields.h>
#include <nx/vms/rules/actions/send_email_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/event_fields/analytics_event_type_field.h>
#include <nx/vms/rules/event_fields/analytics_object_type_field.h>
#include <nx/vms/rules/event_fields/customizable_icon_field.h>
#include <nx/vms/rules/event_fields/customizable_text_field.h>
#include <nx/vms/rules/event_fields/expected_string_field.h>
#include <nx/vms/rules/event_fields/expected_uuid_field.h>
#include <nx/vms/rules/event_fields/int_field.h>
#include <nx/vms/rules/event_fields/keywords_field.h>
#include <nx/vms/rules/event_fields/source_camera_field.h>
#include <nx/vms/rules/event_fields/source_server_field.h>
#include <nx/vms/rules/event_fields/source_user_field.h>
#include <nx/vms/rules/event_fields/text_field.h>
#include <nx/vms/rules/events/debug_event.h>
#include <nx/vms/rules/events/device_disconnected_event.h>
#include <nx/vms/rules/events/device_ip_conflict_event.h>
#include <nx/vms/rules/events/generic_event.h>
#include <nx/vms/rules/events/license_issue_event.h>
#include <nx/vms/rules/events/network_issue_event.h>
#include <nx/vms/rules/events/server_conflict_event.h>
#include <nx/vms/rules/events/server_failure_event.h>
#include <nx/vms/rules/events/soft_trigger_event.h>
#include <nx/vms/rules/events/storage_issue_event.h>

namespace nx::vms::rules {

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
    registerEvent<DeviceIpConflictEvent>();
    registerEvent<DebugEvent>();
    registerEvent<StorageIssueEvent>();
    registerEvent<DeviceDisconnectedEvent>();
    registerEvent<LicenseIssueEvent>();
    registerEvent<ServerConflictEvent>();
    registerEvent<ServerFailureEvent>();
    registerEvent<SoftTriggerEvent>();
    registerEvent<NetworkIssueEvent>();
    registerEvent<GenericEvent>();
}

void Initializer::registerActions() const
{
    // Register built-in actions.
    registerAction<SendEmailAction>();
    registerAction<NotificationAction>();
}

void Initializer::registerFields() const
{
    registerEventField<AnalyticsEventTypeField>();
    registerEventField<AnalyticsObjectTypeField>();
    registerEventField<CustomizableIconField>();
    registerEventField<CustomizableTextField>();
    registerEventField<EventTextField>();
    registerEventField<ExpectedStringField>();
    registerEventField<ExpectedUuidField>();
    registerEventField<IntField>();
    registerEventField<KeywordsField>();
    registerEventField<SourceCameraField>();
    registerEventField<SourceServerField>();
    registerEventField<SourceUserField>();

    registerActionField<ActionTextField>();
    registerActionField<FlagField>();
    registerActionField<OptionalTimeField>();
    registerActionField<TargetUserField>();
    registerActionField<TextWithFields>();
    registerActionField<Substitution>();
}

} // namespace nx::vms::rules
