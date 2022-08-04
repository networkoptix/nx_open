// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "initializer.h"

#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/action_fields/builtin_fields.h>
#include <nx/vms/rules/actions/builtin_actions.h>
#include <nx/vms/rules/event_fields/builtin_fields.h>
#include <nx/vms/rules/events/builtin_events.h>

namespace nx::vms::rules {

Initializer::Initializer(nx::vms::common::SystemContext* context):
    SystemContextAware(context)
{
}

Initializer::~Initializer()
{
}

void Initializer::registerEvents() const
{
    // Register built-in events.
    registerEvent<AnalyticsEvent>();
    registerEvent<AnalyticsObjectEvent>();
    registerEvent<CameraInputEvent>();
    registerEvent<DeviceDisconnectedEvent>();
    registerEvent<DeviceIpConflictEvent>();
    registerEvent<DebugEvent>();
    registerEvent<FanErrorEvent>();
    registerEvent<GenericEvent>();
    registerEvent<LicenseIssueEvent>();
    registerEvent<MotionEvent>();
    registerEvent<NetworkIssueEvent>();
    registerEvent<PluginDiagnosticEvent>();
    registerEvent<PoeOverBudgetEvent>();
    registerEvent<ServerCertificateErrorEvent>();
    registerEvent<ServerConflictEvent>();
    registerEvent<ServerFailureEvent>();
    registerEvent<ServerStartedEvent>();
    registerEvent<SoftTriggerEvent>();
    registerEvent<StorageIssueEvent>();
}

void Initializer::registerActions() const
{
    // Register built-in actions.
    registerAction<HttpAction>();
    registerAction<NotificationAction>();
    registerAction<SendEmailAction>();
    registerAction<SpeakAction>();
    registerAction<TextOverlayAction>();
}

void Initializer::registerFields() const
{
    registerEventField<AnalyticsEngineField>();
    registerEventField<AnalyticsEventLevelField>();
    m_engine->registerEventField(
        fieldMetatype<AnalyticsEventTypeField>(),
        [this] { return new AnalyticsEventTypeField(this->m_context); });
    registerEventField<AnalyticsObjectAttributesField>();
    m_engine->registerEventField(
        fieldMetatype<AnalyticsObjectTypeField>(),
        [this] { return new AnalyticsObjectTypeField(this->m_context); });
    registerEventField<CustomizableIconField>();
    registerEventField<CustomizableTextField>();
    registerEventField<EventTextField>();
    registerEventField<ExpectedUuidField>();
    registerEventField<InputPortField>();
    registerEventField<IntField>();
    registerEventField<KeywordsField>();
    registerEventField<SourceCameraField>();
    registerEventField<SourceServerField>();
    m_engine->registerEventField(
        fieldMetatype<SourceUserField>(),
        [this] { return new SourceUserField(systemContext()); });
    registerEventField<StateField>();
    registerEventField<UniqueIdField>();

    registerActionField<ActionTextField>();
    registerActionField<FlagField>();
    registerActionField<ContentTypeField>();
    m_engine->registerActionField(
        fieldMetatype<EmailMessageField>(),
        [this] { return new EmailMessageField(this->m_context); });
    m_engine->registerActionField(
        fieldMetatype<ExtractDetailField>(),
        [this] { return new ExtractDetailField(this->m_context); });
    registerActionField<EventIdField>();
    registerActionField<HttpMethodField>();
    registerActionField<OptionalTimeField>();
    registerActionField<PasswordField>();
    registerActionField<TargetDeviceField>();
    m_engine->registerActionField(
        fieldMetatype<TargetUserField>(),
        [this] { return new TargetUserField(this->m_context); });
    m_engine->registerActionField(
        fieldMetatype<TextWithFields>(),
        [this] { return new TextWithFields(this->m_context); });
    registerActionField<Substitution>();
    registerActionField<VolumeField>();
}

} // namespace nx::vms::rules
