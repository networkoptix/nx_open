// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/integration_action.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/rules/action_builder_fields/flag_field.h>
#include <nx/vms/rules/action_builder_fields/int_field.h>
#include <nx/vms/rules/action_builder_fields/password_field.h>
#include <nx/vms/rules/action_builder_fields/string_selection_field.h>
#include <nx/vms/rules/action_builder_fields/target_layouts_field.h>
#include <nx/vms/rules/action_builder_fields/target_servers_field.h>
#include <nx/vms/rules/action_builder_fields/text_field.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <nx/vms/rules/action_builder_fields/time_field.h>
#include <nx/vms/rules/integration_action_initializer.h>

#include "test_action.h"
#include "test_router.h"

using AnalyticsEngineResource = nx::vms::common::AnalyticsEngineResource;
using AnalyticsEngineResourcePtr = nx::vms::common::AnalyticsEngineResourcePtr;
using AnalyticsPluginResource = nx::vms::common::AnalyticsPluginResource;
using AnalyticsPluginResourcePtr = nx::vms::common::AnalyticsPluginResourcePtr;

static const QList<nx::vms::api::analytics::FieldModel> kIntegrationActionFields{
    {.type = "flag", .fieldName = "Flag"},
    {.type = "int", .fieldName = "Int"},
    {.type = "text", .fieldName = "Text"},
    {.type = "password", .fieldName = "Password"},
    {
        .type = "stringSelection",
        .fieldName = "StringSelection",
        .properties = QJsonDocument::fromJson(
                        R"({"items": ["Option1", "Option2", "Option3"]})").object(),
    },
    {.type = "devices", .fieldName = "TargetDevices"},
    {.type = "servers", .fieldName = "TargetServers"},
    {.type = "users", .fieldName = "TargetUsers"},
    {
        .type = "textWithFields",
        .fieldName = "TextWithFields",
        .properties = QJsonDocument::fromJson(R"({"text": "Some text" })").object(),
    },
    {.type = "layouts", .fieldName = "TargetLayouts"},
    {.type = "time", .fieldName = "Time"},
};

namespace nx::vms::rules::test {

class PluginHelper: public nx::vms::rules::Plugin
{
public:
    PluginHelper(Engine* engine)
    {
        initialize(engine);
    };

    void registerFields() const override
    {
        registerActionField<ActionIntField>();
        registerActionField<ActionTextField>();
        registerActionField<ActionFlagField>();
        registerActionField<StringSelectionField>();
        registerActionField<PasswordField>();
        registerActionField<TargetDevicesField>();
        registerActionField<TargetServersField>();
        registerActionField<TargetUsersField>(m_engine->systemContext());
        registerActionField<TextWithFields>(m_engine->systemContext());
        registerActionField<TargetLayoutsField>();
        registerActionField<TimeField>();
    }

    void registerActions() const override
    {
        registerAction<TestAction>();
    }
};

// Test Integration Actions Initializer and Executor.
class IntegrationActionsTest: public EngineBasedTest
{
public:
    IntegrationActionsTest(): plugin(engine.get()) {}

protected:
    void addPluginWithIntegrationAction(
        const QList<nx::vms::api::analytics::IntegrationAction>& integrationActions)
    {
        nx::vms::api::analytics::EngineManifest engineManifest;
        engineManifest.integrationActions = integrationActions;

        auto pluginResource = AnalyticsPluginResourcePtr(new AnalyticsPluginResource());
        pluginResource->setIdUnsafe(nx::Uuid::createUuid());
        systemContext()->resourcePool()->addResource(pluginResource);

        auto engineResource = AnalyticsEngineResourcePtr(new AnalyticsEngineResource());
        engineResource->setIdUnsafe(nx::Uuid::createUuid());
        engineResource->setParentId(pluginResource->getId());
        systemContext()->resourcePool()->addResource(engineResource);
        engineResource->setManifest(std::move(engineManifest));
    }

protected:
    PluginHelper plugin;
    std::unique_ptr<IntegrationActionInitializer> actionInitializer =
        std::make_unique<IntegrationActionInitializer>(systemContext());
};

//-------------------------------------------------------------------------------------------------
// Content tests

TEST_F(IntegrationActionsTest, initialize)
{
    // Check registering Integration Action with valid descriptor.
    const QString validIntegrationActionId1 = "test.action1";
    const nx::vms::api::analytics::IntegrationAction validIntegrationActionDescriptor1{
        .id = validIntegrationActionId1,
        .name = "Valid Integration Action",
        .fields = kIntegrationActionFields,
    };
    addPluginWithIntegrationAction({validIntegrationActionDescriptor1});

    actionInitializer->initialize(engine.get());
    ASSERT_TRUE(engine->actions().contains("integration." + validIntegrationActionId1));
    ASSERT_EQ(engine->actions().size(), 2);

    // Check unregistering Integration Action on deinitialize.
    actionInitializer->deinitialize();
    ASSERT_FALSE(engine->actions().contains("integration." + validIntegrationActionId1));

    // Non-dynamic actions should be still present.
    ASSERT_EQ(engine->actions().size(), 1);

    // Integration Action having field descriptor with unknown type can't be registered.
    QList<nx::vms::api::analytics::FieldModel> fieldsWithUnknownType{kIntegrationActionFields};
    fieldsWithUnknownType.append({.type = "unknown", .fieldName = "Unknown"});
    const QString invalidIntegrationActionId1 = "test.action2";
    const nx::vms::api::analytics::IntegrationAction invalidIntegrationActionDescriptor1{
        .id = invalidIntegrationActionId1,
        .name = "Duplicated Integration Action",
        .fields = fieldsWithUnknownType,
    };
    addPluginWithIntegrationAction({invalidIntegrationActionDescriptor1});

    actionInitializer->initialize(engine.get());
    ASSERT_FALSE(engine->actions().contains("integration." + invalidIntegrationActionId1));

    // Integration Action having field descriptors with duplicated names can't be registered.
    QList<nx::vms::api::analytics::FieldModel> duplicatedFields{kIntegrationActionFields};
    duplicatedFields.append({.type = "flag", .fieldName = "Flag"});
    const QString invalidIntegrationActionId2 = "test.action";
    const nx::vms::api::analytics::IntegrationAction invalidIntegrationActionDescriptor2{
        .id = invalidIntegrationActionId2,
        .name = "Invalid Integration Action",
        .fields = duplicatedFields,
    };
    addPluginWithIntegrationAction({invalidIntegrationActionDescriptor2});

    actionInitializer->initialize(engine.get());
    ASSERT_FALSE(engine->actions().contains("integration." + invalidIntegrationActionId2));

    // Integration Action with valid descriptor should still be present.
    ASSERT_TRUE(engine->actions().contains("integration." + validIntegrationActionId1));
    ASSERT_EQ(engine->actions().size(), 2);
}

} // namespace nx::vms::rules::test
