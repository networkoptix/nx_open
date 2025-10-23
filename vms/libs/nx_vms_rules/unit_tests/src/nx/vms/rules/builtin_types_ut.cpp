// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/qobject.h>
#include <nx/utils/range_adapters.h>
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
#include <nx/vms/rules/utils/common.h>
#include <nx/vms/rules/utils/event_details.h>
#include <nx/vms/rules/utils/serialization.h>

#include "test_event.h"
#include "test_router.h"

namespace nx::vms::rules::test {

namespace {

using namespace nx::vms::rules::utils;

/** These details are expected to be filled for every built-in event. */
static const QStringList kRequiredDetails{{
    kAggregationKeyDetailName,
    kAggregationKeyIconDetailName,
    kCaptionDetailName,
    kExtendedCaptionDetailName,
    kIconDetailName,
    kLevelDetailName,
    kSourceTextDetailName,
}};

} // namespace

class BuiltinTypesTest:
    public EngineBasedTest,
    public Plugin
{
public:
    BuiltinTypesTest()
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
    void testManifestValidity(auto... args)
    {
        const auto& manifest = T::manifest(args...);
        const auto& meta = T::staticMetaObject;
        QSet<QString> fieldNames;

        EXPECT_FALSE(manifest.id.isEmpty());
        EXPECT_FALSE(manifest.displayName.value().isEmpty());

        // Duration must be provided.
        EXPECT_TRUE(manifest.flags.testFlag(ItemFlag::instant)
            || manifest.flags.testFlag(ItemFlag::prolonged));

        if constexpr(std::derived_from<T, BasicAction>)
        {
            // Action duration cannot be mixed.
            EXPECT_FALSE(manifest.flags.testFlag(ItemFlag::instant)
                && manifest.flags.testFlag(ItemFlag::prolonged));
        }

        // Check all manifest fields correspond to properties with the same name.
        for (const auto& field: manifest.fields)
        {
            SCOPED_TRACE(
                nx::format("Field name: %1, type %2", field.fieldName, field.type).toStdString());
            fieldNames.insert(field.fieldName);

            EXPECT_FALSE(field.type.isEmpty());
            EXPECT_FALSE(field.fieldName.isEmpty());

            const auto propIndex = meta.indexOfProperty(field.fieldName.toUtf8().data());
            EXPECT_GE(propIndex, 0);

            const auto prop = meta.property(propIndex);
            EXPECT_TRUE(prop.isValid());
            EXPECT_NE(prop.userType(), QMetaType::UnknownType);
        }

        ASSERT_EQ(fieldNames.size(), manifest.fields.size());
    }

    template<class T>
    void testResourceDescriptors(bool needField, auto... args)
    {
        const auto& manifest = T::manifest(args...);
        const auto& meta = T::staticMetaObject;

        static const auto resProps = QSet<QString>{
            utils::kDeviceIdFieldName,
            utils::kDeviceIdsFieldName,
            utils::kEngineIdFieldName,
            utils::kLayoutIdFieldName,
            utils::kLayoutIdsFieldName,
            utils::kServerIdFieldName,
            utils::kServerIdsFieldName,
            utils::kUserIdFieldName,
            utils::kUsersFieldName,
        };

        // Check resource named fields for descriptors.
        for (int i = 0; i < meta.propertyCount(); ++i)
        {
            const auto prop = meta.property(i);
            const auto propName = prop.name();

            if (resProps.contains(propName))
            {
                SCOPED_TRACE(NX_FMT("%1 %2", manifest.id, propName).toStdString());

                if (needField)
                {
                    if (const auto field = rules::utils::fieldByName(propName, manifest);
                        !field || field->type.contains("event"))
                    {
                        continue;
                    }
                }

                auto it = manifest.resources.find(propName);
                EXPECT_NE(it, manifest.resources.end());
            }
        }
    }

    template<class T>
    void testPermissionsValidity(auto... args)
    {
        const auto& manifest = T::manifest(args...);
        const auto& meta = T::staticMetaObject;
        static const auto kResourceTypes = std::set{
            qMetaTypeId<nx::Uuid>(),
            qMetaTypeId<UuidList>(),
            qMetaTypeId<UuidSet>(),
            qMetaTypeId<UuidSelection>()};

        int targetCount = 0;

        // Check all permission fields correspond to properties with the same name.
        for (const auto& [fieldName, descriptor]: manifest.resources)
        {
            SCOPED_TRACE(nx::format("Resource field: %1", fieldName).toStdString());
            ASSERT_FALSE(fieldName.empty());

            const auto propIndex = meta.indexOfProperty(fieldName.c_str());
            EXPECT_GE(propIndex, 0);

            const auto prop = meta.property(propIndex);
            EXPECT_TRUE(prop.isValid());
            EXPECT_TRUE(kResourceTypes.contains(prop.userType()));

            if (descriptor.flags.testFlag(FieldFlag::target))
                ++targetCount;
        }

        // Check routing target resource validity.
        EXPECT_LE(targetCount, 1);
        if (manifest.targetServers == TargetServers::resourceOwner)
            EXPECT_EQ(targetCount, 1);

        // Check users target field.
        if (manifest.executionTargets.testFlag(ExecutionTarget::clients))
        {
            const auto it = std::find_if(
                manifest.fields.begin(),
                manifest.fields.end(),
                [](const auto& field) { return field.fieldName == utils::kUsersFieldName; });
            EXPECT_NE(it, manifest.fields.end());
        }

    }

    template<class F>
    void testActionFieldRegistration(auto... args)
    {
        SCOPED_TRACE(fieldMetatype<F>().toStdString());

        EXPECT_TRUE(Plugin::registerActionFieldWithEngine<F>(m_engine, args...));

        const auto& meta = F::staticMetaObject;

        for (const auto& propName: encryptedProperties<F>())
        {
            SCOPED_TRACE(propName.toStdString());
            const auto idx = meta.indexOfProperty(propName.toUtf8().constData());
            ASSERT_GE(idx, 0);

            const auto prop = meta.property(idx);
            EXPECT_EQ(prop.userType(), qMetaTypeId<QString>());
        }
    }

    template<class T>
    void testEventFieldRegistration(auto... args)
    {
        SCOPED_TRACE(fieldMetatype<T>().toStdString());

        EXPECT_TRUE(m_engine->registerEventField(
            fieldMetatype<T>(),
            [args...](const FieldDescriptor* descriptor){ return new T(args..., descriptor); }));

        // Check for serialization assertions.
        const FieldDescriptor tmpDescriptor{.type = fieldMetatype<T>()};
        const auto field = m_engine->buildEventField(&tmpDescriptor);
        const auto data = serializeProperties(field.get(), nx::utils::propertyNames(field.get()));
        deserializeProperties(data, field.get());
    }

    template<class T>
    void testEventRegistration(auto... args)
    {
        const auto& manifest = T::manifest(args...);
        SCOPED_TRACE(nx::format("Event id: %1", manifest.id).toStdString());
        testManifestValidity<T>(args...);
        testResourceDescriptors<T>(/*needField*/ false, args...);
        testPermissionsValidity<T>(args...);

        SCOPED_TRACE(nx::format("Group id: %1", manifest.groupId).toStdString());
        EXPECT_TRUE(testGroupValidity(manifest.groupId,
            defaultEventGroups(engine->systemContext())));

        // Check if all fields are registered.
        for (const auto& field : manifest.fields)
        {
            SCOPED_TRACE(nx::format("Event field type: %1", field.type).toStdString());
            EXPECT_TRUE(engine->isEventFieldRegistered(field.type));
        }

        ASSERT_TRUE(registerEvent<T>(args...));

        const auto filter = engine->buildEventFilter(manifest.id);
        ASSERT_TRUE(filter);

        if (manifest.flags.testFlag(ItemFlag::prolonged))
            EXPECT_TRUE(filter->template fieldByName<StateField>(rules::utils::kStateFieldName));

        // Check for serialization assertions.
        const auto event = QSharedPointer<T>::create();
        event->setState(State::instant);

        const auto data = serializeProperties(event.get(), nx::utils::propertyNames(event.get()));
        deserializeProperties(data, event.get());

        const QVariantMap details = event->details(engine->systemContext(), Qn::RI_NameOnly);
        for (auto detailName: kRequiredDetails)
            EXPECT_TRUE(details.value(detailName).isValid()) << detailName.toStdString();
    }

    template<class T>
    void testActionRegistration()
    {
        const auto& manifest = T::manifest();
        SCOPED_TRACE(nx::format("Action id: %1", manifest.id).toStdString());

        testManifestValidity<T>();
        testResourceDescriptors<T>(/*needFields*/ true);
        testPermissionsValidity<T>();

        // Check if all fields are registered.
        for (const auto& field : manifest.fields)
        {
            SCOPED_TRACE(nx::format("Action field type: %1", field.type).toStdString());
            EXPECT_TRUE(engine->isActionFieldRegistered(field.type));

            // Check if prolonged actions are running on client.
            if (field.fieldName == rules::utils::kUsersFieldName
                && manifest.flags.testFlag(ItemFlag::prolonged))
            {
                EXPECT_TRUE(manifest.executionTargets.testFlag(ExecutionTarget::clients));
            }
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
    testEventFieldRegistration<AnalyticsAttributesField>();
    testEventFieldRegistration<AnalyticsObjectTypeField>(systemContext());
    testEventFieldRegistration<CustomizableFlagField>();
    testEventFieldRegistration<CustomizableIconField>();
    testEventFieldRegistration<CustomizableTextField>();
    testEventFieldRegistration<EventFlagField>();
    testEventFieldRegistration<EventTextField>();
    testEventFieldRegistration<ExpectedUuidField>();
    testEventFieldRegistration<InputPortField>();
    testEventFieldRegistration<IntField>();
    testEventFieldRegistration<ObjectLookupField>(systemContext());
    testEventFieldRegistration<SourceCameraField>();
    testEventFieldRegistration<SourceServerField>();
    testEventFieldRegistration<SourceUserField>(systemContext());
    testEventFieldRegistration<StateField>();
    testEventFieldRegistration<TextLookupField>(systemContext());
    testEventFieldRegistration<UniqueIdField>();

    testEventRegistration<AnalyticsEvent>();
    testEventRegistration<AnalyticsObjectEvent>();
    testEventRegistration<CameraInputEvent>(systemContext());
    testEventRegistration<DeviceDisconnectedEvent>(systemContext());
    testEventRegistration<DeviceIpConflictEvent>(systemContext());
    testEventRegistration<FanErrorEvent>();
    testEventRegistration<GenericEvent>();
    testEventRegistration<IntegrationDiagnosticEvent>();
    testEventRegistration<LicenseIssueEvent>();
    testEventRegistration<MotionEvent>();
    testEventRegistration<NetworkIssueEvent>();
    testEventRegistration<PoeOverBudgetEvent>();
    testEventRegistration<SaasIssueEvent>();
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
    testActionFieldRegistration<EventDevicesField>();
    testActionFieldRegistration<ExtractDetailField>(systemContext());
    testActionFieldRegistration<ActionFlagField>();
    testActionFieldRegistration<FpsField>();
    testActionFieldRegistration<HttpAuthTypeField>();
    testActionFieldRegistration<HttpHeadersField>();
    testActionFieldRegistration<HttpMethodField>();
    testActionFieldRegistration<OptionalTimeField>();
    testActionFieldRegistration<OutputPortField>();
    testActionFieldRegistration<PasswordField>();
    testActionFieldRegistration<PtzPresetField>();
    testActionFieldRegistration<SoundField>();
    testActionFieldRegistration<StreamQualityField>();
    testActionFieldRegistration<TargetDevicesField>();
    testActionFieldRegistration<TargetServersField>();
    testActionFieldRegistration<TargetUsersField>(systemContext());
    testActionFieldRegistration<TextWithFields>(systemContext());
    testActionFieldRegistration<TimeField>();
    testActionFieldRegistration<TargetLayoutField>();
    testActionFieldRegistration<TargetLayoutsField>();
    testActionFieldRegistration<TargetDeviceField>();
    testActionFieldRegistration<VolumeField>();
    testActionFieldRegistration<HttpAuthField>();
    testActionFieldRegistration<StringSelectionField>();

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
    testActionRegistration<SiteHttpAction>();
    testActionRegistration<SpeakAction>();
    testActionRegistration<TextOverlayAction>();
    testActionRegistration<WriteToLogAction>();
}

} // namespace nx::vms::rules::test
