// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "initializer.h"

#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/actions/builtin_actions.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/events/builtin_events.h>

#include "action_builder_field_validators/http_auth_field_validator.h"
#include "action_builder_field_validators/http_method_field_validator.h"
#include "action_builder_field_validators/layout_field_validator.h"
#include "action_builder_field_validators/optional_time_field_validator.h"
#include "action_builder_field_validators/sound_field_validator.h"
#include "action_builder_field_validators/target_device_field_validator.h"
#include "action_builder_field_validators/target_layout_field_validator.h"
#include "action_builder_field_validators/target_server_field_validator.h"
#include "action_builder_field_validators/target_single_device_field_validator.h"
#include "action_builder_field_validators/target_user_field_validator.h"
#include "action_builder_field_validators/text_with_fields_validator.h"
#include "action_builder_field_validators/time_field_validator.h"
#include "event_filter_field_validators/source_camera_field_validator.h"
#include "event_filter_field_validators/source_server_field_validator.h"
#include "event_filter_field_validators/source_user_field_validator.h"
#include "event_filter_field_validators/state_field_validator.h"

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
    auto context = systemContext();

    // Register built-in events.
    registerEvent<AnalyticsEvent>();
    registerEvent<AnalyticsObjectEvent>();
    registerEvent<BackupFinishedEvent>();
    registerEvent<CameraInputEvent>(context);
    registerEvent<DeviceDisconnectedEvent>(context);
    registerEvent<DeviceIpConflictEvent>(context);
    registerEvent<FanErrorEvent>();
    registerEvent<GenericEvent>();
    registerEvent<LdapSyncIssueEvent>();
    registerEvent<LicenseIssueEvent>();
    registerEvent<SaasIssueEvent>();
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
    registerAction<AcknowledgeAction>();
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
    // Event filter fields.
    registerEventField<AnalyticsEngineField>();
    registerEventField<AnalyticsEventLevelField>();
    m_engine->registerEventField(
        fieldMetatype<AnalyticsEventTypeField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new AnalyticsEventTypeField(this->m_context, descriptor);
        });
    registerEventField<AnalyticsObjectAttributesField>();
    m_engine->registerEventField(
        fieldMetatype<AnalyticsObjectTypeField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new AnalyticsObjectTypeField(this->m_context, descriptor);
        });
    registerEventField<CustomizableFlagField>();
    registerEventField<CustomizableIconField>();
    registerEventField<CustomizableTextField>();
    registerEventField<EventFlagField>();
    registerEventField<EventTextField>();
    registerEventField<ExpectedUuidField>();
    registerEventField<InputPortField>();
    registerEventField<IntField>();
    m_engine->registerEventField(
        fieldMetatype<ObjectLookupField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new ObjectLookupField(this->m_context, descriptor);
        });
    registerEventField<SourceCameraField>();
    registerEventField<SourceServerField>();
    m_engine->registerEventField(
        fieldMetatype<SourceUserField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new SourceUserField(systemContext(), descriptor);
        });
    registerEventField<StateField>();
    m_engine->registerEventField(
        fieldMetatype<TextLookupField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new TextLookupField(this->m_context, descriptor);
        });
    registerEventField<UniqueIdField>();

    // Action builder fields.
    registerActionField<ActionIntField>();
    registerActionField<ActionTextField>();
    registerActionField<ActionFlagField>();
    registerActionField<FpsField>();
    registerActionField<ContentTypeField>();
    registerActionField<EmailMessageField>(this->m_context);
    registerActionField<ExtractDetailField>(this->m_context);
    registerActionField<EventDevicesField>();
    registerActionField<HttpAuthTypeField>();
    registerActionField<HttpMethodField>();
    registerActionField<TargetLayoutField>();
    registerActionField<OptionalTimeField>();
    registerActionField<OutputPortField>();
    registerActionField<PasswordField>();
    registerActionField<PtzPresetField>();
    registerActionField<SoundField>();
    registerActionField<StreamQualityField>();
    registerActionField<TargetDevicesField>();
    registerActionField<TargetServersField>();
    registerActionField<TargetUsersField>(this->m_context);
    registerActionField<TextFormatter>(this->m_context);
    registerActionField<TextWithFields>(this->m_context);
    registerActionField<TargetLayoutsField>();
    registerActionField<TargetDeviceField>();
    registerActionField<TimeField>();
    registerActionField<VolumeField>();
    registerActionField<HttpAuthField>();
}

void Initializer::registerFieldValidators() const
{
    // Event field validators.
    registerFieldValidator<SourceCameraField>(new SourceCameraFieldValidator);
    registerFieldValidator<SourceServerField>(new SourceServerFieldValidator);
    registerFieldValidator<SourceUserField>(new SourceUserFieldValidator);
    registerFieldValidator<StateField>(new StateFieldValidator);

    // Action field validators.
    registerFieldValidator<HttpAuthField>(new HttpAuthFieldValidator);
    registerFieldValidator<HttpMethodField>(new HttpMethodFieldValidator);
    registerFieldValidator<TargetLayoutField>(new LayoutFieldValidator);
    registerFieldValidator<OptionalTimeField>(new OptionalTimeFieldValidator);
    registerFieldValidator<SoundField>(new SoundFieldValidator);
    registerFieldValidator<TargetDevicesField>(new TargetDeviceFieldValidator);
    registerFieldValidator<TargetLayoutsField>(new TargetLayoutFieldValidator);
    registerFieldValidator<TargetServersField>(new TargetServerFieldValidator);
    registerFieldValidator<TargetDeviceField>(new TargetSingleDeviceFieldValidator);
    registerFieldValidator<TargetUsersField>(new TargetUserFieldValidator);
    registerFieldValidator<TextFormatter>(new TextWithFieldsValidator);
    registerFieldValidator<TextWithFields>(new TextWithFieldsValidator);
    registerFieldValidator<TimeField>(new TimeFieldValidator);
}

} // namespace nx::vms::rules
