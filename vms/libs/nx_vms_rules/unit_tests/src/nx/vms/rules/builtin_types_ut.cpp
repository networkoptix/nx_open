// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/qobject.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/actions/builtin_actions.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/group.h>
#include <nx/vms/rules/plugin.h>
#include <nx/vms/rules/utils/serialization.h>

#include "test_event.h"
#include "test_router.h"

namespace nx::vms::rules::test {

class BuiltinTypesTest:
    public common::test::ContextBasedTest,
    public TestEngineHolder,
    public Plugin
{
public:
    BuiltinTypesTest(): TestEngineHolder(context()->systemContext())
    {
        initialize(engine.get());
    }

    bool testGroupValidity(const std::string& id, const Group& group)
    {
        if (id == group.id)
            return true;

        for (const auto& g : group.groups)
        {
            if (testGroupValidity(id, g))
                return true;
        }

        return false;
    }

    template<class T>
    void testManifestValidity()
    {
        const auto& manifest = T::manifest();
        const auto& meta = T::staticMetaObject;
        QSet<QString> fieldNames;

        EXPECT_FALSE(manifest.id.isEmpty());
        EXPECT_FALSE(manifest.displayName.isEmpty());

        // Check all manifest fields correspond to properties with the same name.
        for (const auto& field: manifest.fields)
        {
            SCOPED_TRACE(
                nx::format("Field name: %1, type %2", field.fieldName, field.id).toStdString());
            fieldNames.insert(field.fieldName);

            EXPECT_FALSE(field.id.isEmpty());
            EXPECT_FALSE(field.fieldName.isEmpty());

            const auto propIndex = meta.indexOfProperty(field.fieldName.toUtf8().data());
            EXPECT_GE(propIndex, 0);

            const auto prop = meta.property(propIndex);
            EXPECT_TRUE(prop.isValid());
            EXPECT_NE(prop.userType(), QMetaType::UnknownType);
        }

        ASSERT_EQ(fieldNames.size(), manifest.fields.size());

        // Check linked fields are exist.
        for (const auto& field: manifest.fields)
        {
            for (const auto& link: field.linkedFields)
            {
                SCOPED_TRACE(nx::format("Linked field: %1->%2", field.id, link).toStdString());
                EXPECT_TRUE(fieldNames.contains(link));
            }
        }
    }

    template<class T>
    void testPermissionsValidity()
    {
        const auto& manifest = T::manifest();
        const auto& meta = T::staticMetaObject;

        // Check all permission fields correspond to properties with the same name.
        for (const auto& [fieldName, permissions]: manifest.permissions.resourcePermissions)
        {
            SCOPED_TRACE(nx::format("Resource permission field: %1", fieldName).toStdString());
            ASSERT_FALSE(fieldName.empty());
            ASSERT_FALSE(!permissions);


            const auto propIndex = meta.indexOfProperty(fieldName.c_str());
            EXPECT_GE(propIndex, 0);

            const auto prop = meta.property(propIndex);
            EXPECT_TRUE(prop.isValid());
            EXPECT_TRUE(prop.userType() == qMetaTypeId<QnUuid>()
                || prop.userType() == qMetaTypeId<QnUuidList>());
        }
    }

    template<class T, class... Args>
    void testActionFieldRegistration(Args... args)
    {
        SCOPED_TRACE(fieldMetatype<T>().toStdString());

        EXPECT_TRUE(m_engine->registerActionField(
            fieldMetatype<T>(),
            [args...]{ return new T(args...); }));
    }

    template<class T, class... Args>
    void testEventFieldRegistration(Args... args)
    {
        SCOPED_TRACE(fieldMetatype<T>().toStdString());

        EXPECT_TRUE(m_engine->registerEventField(
            fieldMetatype<T>(),
            [args...]{ return new T(args...); }));

        // Check for serialization assertions.
        const auto field = m_engine->buildEventField(fieldMetatype<T>());
        const auto data = serializeProperties(field.get(), nx::utils::propertyNames(field.get()));
        deserializeProperties(data, field.get());
    }

    template<class T>
    void testEventRegistration()
    {
        const auto& manifest = T::manifest();
        SCOPED_TRACE(nx::format("Event id: %1", manifest.id).toStdString());

        testManifestValidity<T>();
        testPermissionsValidity<T>();

        SCOPED_TRACE(nx::format("Group id: %1", manifest.groupId).toStdString());
        EXPECT_TRUE(testGroupValidity(manifest.groupId, defaultEventGroups()));

        // Check if all fields are registered.
        for (const auto& field : manifest.fields)
        {
            SCOPED_TRACE(nx::format("Event field id: %1", field.id).toStdString());
            EXPECT_TRUE(engine->isEventFieldRegistered(field.id));
        }

        ASSERT_TRUE(registerEvent<T>());

        const auto filter = engine->buildEventFilter(manifest.id);
        EXPECT_TRUE(filter);

        // Check for serialization assertions.
        const auto event = QSharedPointer<T>::create();
        event->setState(State::instant);

        const auto data = serializeProperties(event.get(), nx::utils::propertyNames(event.get()));
        deserializeProperties(data, event.get());
    }

    template<class T>
    void testActionRegistration()
    {
        const auto& manifest = T::manifest();
        SCOPED_TRACE(nx::format("Action id: %1", manifest.id).toStdString());

        testManifestValidity<T>();
        testPermissionsValidity<T>();

        // Check if all fields are registered.
        for (const auto& field : manifest.fields)
        {
            SCOPED_TRACE(nx::format("Action field id: %1", field.id).toStdString());
            EXPECT_TRUE(engine->isActionFieldRegistered(field.id));
        }

        ASSERT_TRUE(registerAction<T>());

        const auto builder = engine->buildActionBuilder(manifest.id);
        ASSERT_TRUE(builder);

        const auto testEvent = AggregatedEventPtr::create(QSharedPointer<TestEvent>::create());
        const auto& meta = T::staticMetaObject;
        const auto fields = builder->fields();

        // Test property type compatibility.
        for(const auto [name, field]: nx::utils::constKeyValueRange(fields))
        {
            SCOPED_TRACE(nx::format("Field name: %1", name).toStdString());
            const auto value = field->build(testEvent);
            EXPECT_TRUE(value.isValid());

            const auto prop = meta.property(meta.indexOfProperty(name.toUtf8().data()));

            SCOPED_TRACE(nx::format(
                "Property type: %1, value type: %2",
                QMetaType(prop.userType()).name(),
                QMetaType(value.userType()).name()).toStdString());
            EXPECT_EQ(prop.userType(), value.userType());
        }
    }
};

TEST_F(BuiltinTypesTest, BuiltinEvents)
{
    // Event fields need to be registered first.
    testEventFieldRegistration<AnalyticsEngineField>();
    testEventFieldRegistration<AnalyticsEventLevelField>();
    testEventFieldRegistration<AnalyticsEventTypeField>(systemContext());
    testEventFieldRegistration<AnalyticsObjectAttributesField>();
    testEventFieldRegistration<AnalyticsObjectTypeField>(systemContext());
    testEventFieldRegistration<CustomizableFlagField>();
    testEventFieldRegistration<CustomizableIconField>();
    testEventFieldRegistration<CustomizableTextField>();
    testEventFieldRegistration<DummyField>();
    testEventFieldRegistration<EventFlagField>();
    testEventFieldRegistration<EventTextField>();
    testEventFieldRegistration<ExpectedUuidField>();
    testEventFieldRegistration<InputPortField>();
    testEventFieldRegistration<IntField>();
    testEventFieldRegistration<KeywordsField>();
    testEventFieldRegistration<ObjectLookupField>(systemContext());
    testEventFieldRegistration<SourceCameraField>();
    testEventFieldRegistration<SourceServerField>();
    testEventFieldRegistration<SourceUserField>(systemContext());
    testEventFieldRegistration<StateField>();
    testEventFieldRegistration<TextLookupField>(systemContext());
    testEventFieldRegistration<UniqueIdField>();

    testEventRegistration<AnalyticsEvent>();
    testEventRegistration<AnalyticsObjectEvent>();
    testEventRegistration<BackupFinishedEvent>();
    testEventRegistration<CameraInputEvent>();
    testEventRegistration<DebugEvent>();
    testEventRegistration<DeviceDisconnectedEvent>();
    testEventRegistration<DeviceIpConflictEvent>();
    testEventRegistration<FanErrorEvent>();
    testEventRegistration<GenericEvent>();
    testEventRegistration<LicenseIssueEvent>();
    testEventRegistration<MotionEvent>();
    testEventRegistration<NetworkIssueEvent>();
    testEventRegistration<PluginDiagnosticEvent>();
    testEventRegistration<PoeOverBudgetEvent>();
    testEventRegistration<ServerCertificateErrorEvent>();
    testEventRegistration<ServerConflictEvent>();
    testEventRegistration<ServerFailureEvent>();
    testEventRegistration<ServerStartedEvent>();
    testEventRegistration<SoftTriggerEvent>();
    testEventRegistration<StorageIssueEvent>();
}

TEST_F(BuiltinTypesTest, BuiltinActions)
{
    // Action fields need to be registered first.
    testActionFieldRegistration<ActionIntField>();
    testActionFieldRegistration<ActionTextField>();
    testActionFieldRegistration<ContentTypeField>();
    testActionFieldRegistration<EmailMessageField>(systemContext());
    testActionFieldRegistration<EventIdField>();
    testActionFieldRegistration<EventDevicesField>();
    testActionFieldRegistration<ExtractDetailField>(systemContext());
    testActionFieldRegistration<ActionFlagField>();
    testActionFieldRegistration<FpsField>();
    testActionFieldRegistration<HttpAuthTypeField>();
    testActionFieldRegistration<HttpMethodField>();
    testActionFieldRegistration<LayoutField>();
    testActionFieldRegistration<OptionalTimeField>();
    testActionFieldRegistration<OutputPortField>();
    testActionFieldRegistration<PasswordField>();
    testActionFieldRegistration<PtzPresetField>();
    testActionFieldRegistration<SoundField>();
    testActionFieldRegistration<StreamQualityField>();
    testActionFieldRegistration<Substitution>();
    testActionFieldRegistration<TargetDeviceField>();
    testActionFieldRegistration<TargetServerField>();
    testActionFieldRegistration<TargetUserField>(systemContext());
    testActionFieldRegistration<TextFormatter>(systemContext());
    testActionFieldRegistration<TextWithFields>(systemContext());
    testActionFieldRegistration<TimeField>();
    testActionFieldRegistration<TargetLayoutField>();
    testActionFieldRegistration<TargetSingleDeviceField>();
    testActionFieldRegistration<VolumeField>();
    testActionFieldRegistration<HttpAuthField>();

    testActionRegistration<BookmarkAction>();
    testActionRegistration<BuzzerAction>();
    testActionRegistration<DeviceOutputAction>();
    testActionRegistration<DeviceRecordingAction>();
    testActionRegistration<EnterFullscreenAction>();
    testActionRegistration<ExitFullscreenAction>();
    testActionRegistration<HttpAction>();
    testActionRegistration<NotificationAction>();
    testActionRegistration<OpenLayoutAction>();
    testActionRegistration<PanicRecordingAction>();
    testActionRegistration<PlaySoundAction>();
    testActionRegistration<PtzPresetAction>();
    testActionRegistration<PushNotificationAction>();
    testActionRegistration<RepeatSoundAction>();
    testActionRegistration<SendEmailAction>();
    testActionRegistration<ShowOnAlarmLayoutAction>();
    testActionRegistration<SpeakAction>();
    testActionRegistration<TextOverlayAction>();
    testActionRegistration<WriteToLogAction>();
}

} // namespace nx::vms::rules::test
