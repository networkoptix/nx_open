// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "initializer.h"

#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/actions/builtin_actions.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
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
    registerEvent<LdapSyncIssueEvent>();
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
    registerAction<BuzzerAction>();
    registerAction<BookmarkAction>();
    registerAction<DeviceOutputAction>();
    registerAction<DeviceRecordingAction>();
    registerAction<EnterFullscreenAction>();
    registerAction<ExitFullscreenAction>();
    registerAction<HttpAction>();
    registerAction<NotificationAction>();
    registerAction<OpenLayoutAction>();
    registerAction<PanicRecordingAction>();
    registerAction<PlaySoundAction>();
    registerAction<PtzPresetAction>();
    registerAction<PushNotificationAction>();
    registerAction<RepeatSoundAction>();
    registerAction<SendEmailAction>();
    registerAction<ShowOnAlarmLayoutAction>();
    registerAction<SpeakAction>();
    registerAction<TextOverlayAction>();
    registerAction<WriteToLogAction>();
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
    registerEventField<CustomizableFlagField>();
    registerEventField<CustomizableIconField>();
    registerEventField<CustomizableTextField>();
    registerEventField<DummyField>();
    registerEventField<EventFlagField>();
    registerEventField<EventTextField>();
    registerEventField<ExpectedUuidField>();
    registerEventField<InputPortField>();
    registerEventField<IntField>();
    registerEventField<KeywordsField>();
    m_engine->registerEventField(
        fieldMetatype<ObjectLookupField>(),
        [this] { return new ObjectLookupField(this->m_context); });
    registerEventField<SourceCameraField>();
    registerEventField<SourceServerField>();
    m_engine->registerEventField(
        fieldMetatype<SourceUserField>(),
        [this] { return new SourceUserField(systemContext()); });
    registerEventField<StateField>();
    m_engine->registerEventField(
        fieldMetatype<TextLookupField>(),
        [this] { return new TextLookupField(this->m_context); });
    registerEventField<UniqueIdField>();

    registerActionField<ActionIntField>();
    registerActionField<ActionTextField>();
    registerActionField<ActionFlagField>();
    registerActionField<FpsField>();
    registerActionField<ContentTypeField>();
    m_engine->registerActionField(
        fieldMetatype<EmailMessageField>(),
        [this] { return new EmailMessageField(this->m_context); });
    m_engine->registerActionField(
        fieldMetatype<ExtractDetailField>(),
        [this] { return new ExtractDetailField(this->m_context); });
    registerActionField<EventIdField>();
    registerActionField<EventDevicesField>();
    registerActionField<HttpAuthTypeField>();
    registerActionField<HttpMethodField>();
    registerActionField<LayoutField>();
    registerActionField<OptionalTimeField>();
    registerActionField<OutputPortField>();
    registerActionField<PasswordField>();
    registerActionField<PtzPresetField>();
    registerActionField<SoundField>();
    registerActionField<StreamQualityField>();
    registerActionField<TargetDeviceField>();
    registerActionField<TargetServerField>();
    m_engine->registerActionField(
        fieldMetatype<TargetUserField>(),
        [this] { return new TargetUserField(this->m_context); });
    m_engine->registerActionField(
        fieldMetatype<TextFormatter>(),
        [this] { return new TextFormatter(this->m_context); });
    m_engine->registerActionField(
        fieldMetatype<TextWithFields>(),
        [this] { return new TextWithFields(this->m_context); });
    registerActionField<Substitution>();
    registerActionField<TargetLayoutField>();
    registerActionField<TargetSingleDeviceField>();
    registerActionField<TimeField>();
    registerActionField<VolumeField>();
}

} // namespace nx::vms::rules
