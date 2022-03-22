// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "initializer.h"

#include <nx/vms/rules/action_fields/builtin_fields.h>
#include <nx/vms/rules/actions/http_action.h>
#include <nx/vms/rules/actions/send_email_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/event_fields/builtin_fields.h>
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
    registerEvent<DeviceDisconnectedEvent>();
    registerEvent<DeviceIpConflictEvent>();
    registerEvent<DebugEvent>();
    registerEvent<GenericEvent>();
    registerEvent<LicenseIssueEvent>();
    registerEvent<NetworkIssueEvent>();
    registerEvent<ServerConflictEvent>();
    registerEvent<ServerFailureEvent>();
    registerEvent<SoftTriggerEvent>();
    registerEvent<StorageIssueEvent>();
}

void Initializer::registerActions() const
{
    // Register built-in actions.
    registerAction<HttpAction>();
    registerAction<NotificationAction>();
    registerAction<SendEmailAction>();
}

void Initializer::registerFields() const
{
    registerEventField<AnalyticsEventTypeField>();
    registerEventField<AnalyticsObjectTypeField>();
    registerEventField<CustomizableIconField>();
    registerEventField<CustomizableTextField>();
    registerEventField<EventTextField>();
    registerEventField<ExpectedUuidField>();
    registerEventField<IntField>();
    registerEventField<KeywordsField>();
    registerEventField<SourceCameraField>();
    registerEventField<SourceServerField>();
    registerEventField<SourceUserField>();

    registerActionField<ActionTextField>();
    registerActionField<ContentTypeField>();
    registerActionField<FlagField>();
    registerActionField<HttpMethodField>();
    registerActionField<OptionalTimeField>();
    registerActionField<PasswordField>();
    registerActionField<TargetUserField>();
    registerActionField<TextWithFields>();
    registerActionField<Substitution>();
}

} // namespace nx::vms::rules
