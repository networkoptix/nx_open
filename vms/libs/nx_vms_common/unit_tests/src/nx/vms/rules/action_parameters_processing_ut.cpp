// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QRandomGenerator>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/properties.h>
#include <nx/core/access/access_types.h>
#include <nx/core/resource/server_mock.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/rules/action_parameters_processing.h>

namespace nx::vms::rules::test {

using ActionParameters = nx::vms::event::ActionParameters;
using EventParameters = nx::vms::event::EventParameters;

using ActionType = nx::vms::api::ActionType;
using EventType = nx::vms::api::EventType;

namespace {

EventParameters createGenericEvent(
    const QString& source,
    const QString& caption,
    const QString& description)
{
    EventParameters result;
    result.eventType = EventType::userDefinedEvent;
    result.resourceName = source;
    result.caption = caption;
    result.description = description;
    return result;
}

EventParameters createAnalyticsEvent(
    const QnUuid& cameraId,
    const QString& caption,
    const QString& description,
    const QString& eventType)
{
    EventParameters result;
    result.eventType = EventType::analyticsSdkEvent;
    result.eventResourceId = cameraId;
    result.caption = caption;
    result.description = description;
    result.setAnalyticsEventTypeId(eventType);
    return result;
}

EventParameters createRandomEvent()
{
    EventParameters result;

    // Right now poeOverBudgetEvent is the maximum enum value, except UserDefinedEvent.
    result.eventType = static_cast<EventType>(
        QRandomGenerator::global()->generate() % ((quint32) EventType::poeOverBudgetEvent + 1));
    if (result.eventType == EventType::undefinedEvent)
        result.eventType = EventType::userDefinedEvent;

    result.resourceName = "Some source";
    result.caption = "Some caption";
    result.description = "Some description";
    result.eventResourceId = QnUuid::createUuid();
    result.setAnalyticsEventTypeId("Some event type");
    return result;
}

EventParameters createUnsupportedEvent()
{
    EventParameters result = createRandomEvent();
    while (result.eventType == EventType::undefinedEvent
        || result.eventType == EventType::analyticsSdkEvent
        || result.eventType == EventType::userDefinedEvent)
    {
        // Right now poeOverBudgetEvent is the maximum enum value, except UserDefinedEvent.
        result.eventType = static_cast<EventType>(
            QRandomGenerator::global()->generate() % ((quint32) EventType::poeOverBudgetEvent + 1));
    }
    return result;
}

enum Keywords {kSource, kCaption, kDescription, kCameraId, kCameraName, kEventType, kEventName};

QString createText(const QHash<Keywords, QString>& filled)
{
    using Placeholder = nx::vms::rules::SubstitutionKeywords::Event;

    QString result;
    result += filled.value(kSource, Placeholder::source);
    result += filled.value(kCaption, Placeholder::caption);
    result += filled.value(kDescription, Placeholder::description);
    result += filled.value(kCameraId, Placeholder::cameraId);
    result += filled.value(kCameraName, Placeholder::cameraName);
    result += filled.value(kEventType, Placeholder::eventType);
    result += filled.value(kEventName, Placeholder::eventName);
    return result;
}

ActionParameters createActionParams()
{
    ActionParameters result;
    result.text = createText({});
    return result;
}

const char* kSerializedDescriptors = R"json(
{
    "pluginDescriptors": {
        "id": "nx.dummy",
        "name": "Dummy Plugin"
    },
    "engineDescriptors": {
        "{fa80d3c3-0c28-496b-949d-42650850a7ee}": {
            "id": "{fa80d3c3-0c28-496b-949d-42650850a7ee}",
            "name": "Dummy Plugin",
            "pluginId": "nx.dummy"
        }
    },
    "eventTypeDescriptors": {
        "nx.dummy.DummyEvent": {
            "id": "nx.dummy.DummyEvent",
            "name": "Dummy Event Created For The Testing Purposes",
            "scopes": [{"engineId":"{fa80d3c3-0c28-496b-949d-42650850a7ee}"}]
        }
    }
}
)json";

const QString kDummyEventType = "nx.dummy.DummyEvent";
const QString kDummyEventName = "Dummy Event Created For The Testing Purposes";

} // namespace

class ActionParametersProcessingTest: public nx::vms::common::test::ContextBasedTest
{
protected:
    virtual void SetUp() override
    {
        m_camera = addCamera();
        m_camera->setName("BigBro");

        // We need a whole Server just to make analytics descriptors work.
        QnMediaServerResourcePtr server(new nx::core::resource::ServerMock());
        server->setIdUnsafe(peerId());
        resourcePool()->addResource(server);

        server->setProperty(nx::analytics::kDescriptorsProperty, QString(kSerializedDescriptors));
        server->saveProperties();
    }

    const QnVirtualCameraResourcePtr& camera() const
    {
        return m_camera;
    }

private:
    QnVirtualCameraResourcePtr m_camera;
};

TEST_F(ActionParametersProcessingTest, genericEvent)
{
    const auto actionParameters = nx::vms::rules::actualActionParameters(
        ActionType::execHttpRequestAction,
        createActionParams(),
        createGenericEvent("S", "C", "D"),
        systemContext());

    EXPECT_EQ(actionParameters.text,
        createText({ {kSource, "S"}, {kCaption, "C"}, {kDescription, "D"} }));
}

TEST_F(ActionParametersProcessingTest, genericEventWithKeywords)
{
    using Placeholder = nx::vms::rules::SubstitutionKeywords::Event;
    const auto actionParameters = nx::vms::rules::actualActionParameters(
        ActionType::execHttpRequestAction,
        createActionParams(),
        createGenericEvent(Placeholder::description, Placeholder::source, Placeholder::caption),
        systemContext());

    EXPECT_EQ(actionParameters.text,
        createText({
            {kSource, Placeholder::description},
            {kCaption, Placeholder::source},
            {kDescription, Placeholder::caption}}));
}

TEST_F(ActionParametersProcessingTest, analyticsEvent)
{
    static const auto id = camera()->getId();
    const auto actionParameters = nx::vms::rules::actualActionParameters(
        ActionType::execHttpRequestAction,
        createActionParams(),
        createAnalyticsEvent(id, "C", "D", kDummyEventType),
        systemContext());

    EXPECT_EQ(actionParameters.text,
        createText({
            {kCameraId, id.toString()},
            {kCameraName, camera()->getUserDefinedName()},
            {kCaption, "C"},
            {kDescription, "D"},
            {kEventType, kDummyEventType},
            {kEventName, kDummyEventName}}));
}

TEST_F(ActionParametersProcessingTest, analyticsEventWithKeywords)
{
    using Placeholder = nx::vms::rules::SubstitutionKeywords::Event;
    static const auto id = QnUuid::createUuid();
    const auto actionParameters = nx::vms::rules::actualActionParameters(
        ActionType::execHttpRequestAction,
        createActionParams(),
        createAnalyticsEvent(id, Placeholder::description, Placeholder::eventType, Placeholder::caption),
        systemContext());

    EXPECT_EQ(actionParameters.text,
        createText({
            {kCameraId, id.toString()},
            {kCameraName, Placeholder::cameraName}, //< Invalid camera id, name should not be processed.
            {kCaption, Placeholder::description},
            {kDescription, Placeholder::eventType},
            {kEventType, Placeholder::caption},
            {kEventName, Placeholder::eventName}})); //< Invalid event id, name should not be processed.
}

TEST_F(ActionParametersProcessingTest, unsupportedEventType)
{
    const auto originalParameters = createActionParams();
    const auto actionParameters = nx::vms::rules::actualActionParameters(
        ActionType::execHttpRequestAction,
        originalParameters,
        createUnsupportedEvent(),
        systemContext());

    EXPECT_EQ(actionParameters.text, originalParameters.text);
}

TEST_F(ActionParametersProcessingTest, unsupportedActionType)
{
    ActionType actionType = ActionType::undefinedAction;
    while (actionType == ActionType::undefinedAction || actionType == ActionType::execHttpRequestAction)
    {
        // Right now pushNotificationAction is the maximum enum value.
        actionType = static_cast<ActionType>(
            QRandomGenerator::global()->generate() % ((quint32) ActionType::pushNotificationAction + 1));
    }

    const auto originalParameters = createActionParams();
    const auto actionParameters = nx::vms::rules::actualActionParameters(
        actionType,
        originalParameters,
        createUnsupportedEvent(),
        systemContext());

    EXPECT_EQ(actionParameters.text, originalParameters.text);
}

} // namespace nx::vms::rules::test
