// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <client/client_message_processor.h>
#include <nx/reflect/json.h>
#include <nx/vms/api/data/server_runtime_event_data.h>
#include <nx/vms/api/data/site_health_message.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {
namespace test {

using namespace nx::vms::api;

namespace {

const nx::Uuid kIntercomId("a92a5ef6-d5e4-79bb-3778-7e70a051b123");

SiteHealthMessage intercomMessage(SiteHealthMessageType type, bool active)
{
    return SiteHealthMessage{
        .type = type,
        .active = active,
        .timestampUs = std::chrono::microseconds(1791196156735000),
        .resourceIds = {kIntercomId},
    };
}

/** The client message processor without a connection: the test emits its signals directly. */
class TestMessageProcessor: public QnClientMessageProcessor
{
public:
    using QnClientMessageProcessor::QnClientMessageProcessor;

protected:
    virtual QnResourceFactory* getResourceFactory() const override { return nullptr; }
};

} // namespace

class ServerRuntimeEventConnectorTest: public ::testing::Test
{
protected:
    ServerRuntimeEventConnectorTest()
    {
        systemContext.reset(appContext.createSystemContext(SystemContext::Mode::unitTests));
        appContext.addSystemContext(systemContext.get());

        messageProcessor = std::make_unique<TestMessageProcessor>(systemContext.get());
        connector.setMessageProcessor(messageProcessor.get());

        QObject::connect(&connector,
            &ServerRuntimeEventConnector::siteHealthMessage,
            [this](const SiteHealthMessage& message) { received.push_back(message); });
    }

    /** Sends the message the way the server does: as a JSON payload of a server runtime event. */
    void whenServerSends(const SiteHealthMessage& message)
    {
        ServerRuntimeEventData event;
        event.eventType = ServerRuntimeEventType::siteHealthMessage;
        event.eventData = QByteArray::fromStdString(nx::reflect::json::serialize(message));
        emit messageProcessor->serverRuntimeEventOccurred(event);
    }

    std::vector<bool> receivedActiveFlags() const
    {
        std::vector<bool> result;
        for (const auto& message: received)
            result.push_back(message.active);
        return result;
    }

    std::vector<SiteHealthMessageType> receivedTypes() const
    {
        std::vector<SiteHealthMessageType> result;
        for (const auto& message: received)
            result.push_back(message.type);
        return result;
    }

protected:
    ApplicationContext appContext{
        ApplicationContext::Mode::unitTests, Qn::SerializationFormat::json};
    std::unique_ptr<SystemContext> systemContext;
    std::unique_ptr<TestMessageProcessor> messageProcessor;
    ServerRuntimeEventConnector connector;
    std::vector<SiteHealthMessage> received;
};

/**
 * The server hides the "Calling..." informer by sending showIntercomInformer with active=false.
 * The connector must deliver it as inactive.
 */
TEST_F(ServerRuntimeEventConnectorTest, hiddenIntercomInformerArrivesInactive)
{
    whenServerSends(
        intercomMessage(SiteHealthMessageType::showIntercomInformer, /*active*/ false));

    ASSERT_EQ(1u, received.size());
    EXPECT_FALSE(received[0].active);
    EXPECT_EQ(SiteHealthMessageType::showIntercomInformer, received[0].type);
    EXPECT_EQ(kIntercomId, received[0].resourceId());
}

/**
 * The connector keeps no state: messages are relayed one to one, in order, each with its own
 * flag. The sequence is what the server sends for a missed call followed by the next call.
 */
TEST_F(ServerRuntimeEventConnectorTest, messageSequenceIsRelayedInOrderWithEachFlag)
{
    whenServerSends(intercomMessage(SiteHealthMessageType::showIntercomInformer, true));
    whenServerSends(intercomMessage(SiteHealthMessageType::showIntercomInformer, false));
    whenServerSends(intercomMessage(SiteHealthMessageType::showMissedCallInformer, true));
    whenServerSends(intercomMessage(SiteHealthMessageType::showIntercomInformer, true));

    EXPECT_EQ((std::vector<SiteHealthMessageType>{SiteHealthMessageType::showIntercomInformer,
                  SiteHealthMessageType::showIntercomInformer,
                  SiteHealthMessageType::showMissedCallInformer,
                  SiteHealthMessageType::showIntercomInformer}),
        receivedTypes());
    EXPECT_EQ((std::vector<bool>{true, false, true, true}), receivedActiveFlags());
}

} // namespace test
} // namespace nx::vms::client::core
