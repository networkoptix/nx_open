// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "initializer.h"

#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/actions/builtin_actions.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/ini.h>

#include "action_builder_field_validators/action_text_field_validator.h"
#include "action_builder_field_validators/http_auth_field_validator.h"
#include "action_builder_field_validators/http_headers_field_validator.h"
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
#include "event_filter_field_validators/analytics_event_type_field_validator.h"
#include "event_filter_field_validators/analytics_object_type_field_validator.h"
#include "event_filter_field_validators/object_lookup_field_validator.h"
#include "event_filter_field_validators/source_camera_field_validator.h"
#include "event_filter_field_validators/source_server_field_validator.h"
#include "event_filter_field_validators/source_user_field_validator.h"
#include "event_filter_field_validators/state_field_validator.h"
#include "event_filter_field_validators/text_lookup_field_validator.h"

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
    registerEvent<CameraInputEvent>(context);
    registerEvent<DeviceDisconnectedEvent>(context);
    registerEvent<DeviceIpConflictEvent>(context);
    registerEvent<FanErrorEvent>();
    registerEvent<GenericEvent>();
    registerEvent<IntegrationDiagnosticEvent>();
    registerEvent<LdapSyncIssueEvent>();
    registerEvent<LicenseIssueEvent>();
    registerEvent<MotionEvent>();
    registerEvent<NetworkIssueEvent>();
    registerEvent<PoeOverBudgetEvent>();
    registerEvent<SaasIssueEvent>();
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
    registerAction<SiteHttpAction>();
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
            return new AnalyticsEventTypeField(systemContext(), descriptor);
        });
    registerEventField<AnalyticsAttributesField>();
    m_engine->registerEventField(
        fieldMetatype<AnalyticsObjectTypeField>(),
        [this](const FieldDescriptor* descriptor)
        {
            return new AnalyticsObjectTypeField(systemContext(), descriptor);
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
            return new ObjectLookupField(systemContext(), descriptor);
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
            return new TextLookupField(systemContext(), descriptor);
        });
    registerEventField<UniqueIdField>();

    // Action builder fields.
    registerActionField<ActionIntField>();
    registerActionField<ActionTextField>();
    registerActionField<ActionFlagField>();
    registerActionField<FpsField>();
    registerActionField<ContentTypeField>();
    registerActionField<EmailMessageField>(systemContext());
    registerActionField<ExtractDetailField>(systemContext());
    registerActionField<EventDevicesField>();
    registerActionField<HttpAuthTypeField>();
    registerActionField<HttpHeadersField>();
    registerActionField<HttpMethodField>();
    registerActionField<StringSelectionField>();
    registerActionField<TargetLayoutField>();
    registerActionField<OptionalTimeField>();
    registerActionField<OutputPortField>();
    registerActionField<PasswordField>();
    registerActionField<PtzPresetField>();
    registerActionField<SoundField>();
    registerActionField<StreamQualityField>();
    registerActionField<TargetDevicesField>();
    registerActionField<TargetServersField>();
    registerActionField<TargetUsersField>(systemContext());
    registerActionField<TextWithFields>(systemContext());
    registerActionField<TargetLayoutsField>();
    registerActionField<TargetDeviceField>();
    registerActionField<TimeField>();
    registerActionField<VolumeField>();
    registerActionField<HttpAuthField>();
}

void Initializer::registerFieldValidators() const
{
    // Event field validators.
    registerEventFieldValidator<AnalyticsEventTypeField, AnalyticsEventTypeFieldValidator>();
    registerEventFieldValidator<AnalyticsObjectTypeField, AnalyticsObjectTypeFieldValidator>();
    registerEventFieldValidator<ObjectLookupField, ObjectLookupFieldValidator>();
    registerEventFieldValidator<SourceCameraField, SourceCameraFieldValidator>();
    registerEventFieldValidator<SourceServerField, SourceServerFieldValidator>();
    registerEventFieldValidator<SourceUserField, SourceUserFieldValidator>();
    registerEventFieldValidator<StateField, StateFieldValidator>();
    registerEventFieldValidator<TextLookupField, TextLookupFieldValidator>();

    // Action field validators.
    registerActionFieldValidator<ActionTextField, ActionTextFieldValidator>();
    registerActionFieldValidator<HttpAuthField, HttpAuthFieldValidator>();
    registerActionFieldValidator<HttpHeadersField, HttpHeadersFieldValidator>();
    registerActionFieldValidator<HttpMethodField, HttpMethodFieldValidator>();
    registerActionFieldValidator<TargetLayoutField, LayoutFieldValidator>();
    registerActionFieldValidator<OptionalTimeField, OptionalTimeFieldValidator>();
    registerActionFieldValidator<SoundField, SoundFieldValidator>();
    registerActionFieldValidator<TargetDevicesField, TargetDeviceFieldValidator>();
    registerActionFieldValidator<TargetLayoutsField, TargetLayoutFieldValidator>();
    registerActionFieldValidator<TargetServersField, TargetServerFieldValidator>();
    registerActionFieldValidator<TargetDeviceField, TargetSingleDeviceFieldValidator>();
    registerActionFieldValidator<TargetUsersField, TargetUserFieldValidator>();
    registerActionFieldValidator<TextWithFields, TextWithFieldsValidator>();
    registerActionFieldValidator<TimeField, TimeFieldValidator>();
}

} // namespace nx::vms::rules
