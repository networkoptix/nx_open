// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <future>

#include <gtest/gtest.h>

#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_connector.h>

#include "test_event.h"
#include "test_router.h"

namespace nx::vms::rules::test {

class EventConnectorTest: public EngineBasedTest
{
};

class TestEventConnector: public nx::vms::rules::EventConnector
{
public:
    void sendEventBatch(std::vector<nx::vms::rules::EventPtr> events)
    {
        emit analyticsEvents(events);
    }

    void sendEvent(nx::vms::rules::EventPtr event)
    {
        emit this->event(event);
    }
};

TEST_F(EventConnectorTest, eventSignals)
{
   engine->registerEvent(TestEvent::manifest(), [] { return new TestEvent(); });

   auto connector = std::make_unique<TestEventConnector>();
   engine->addEventConnector(connector.get());

   // Test if all event types are properly registered to send signals between threads.

   auto future = std::async(std::launch::async,
       [&connector] {
           std::vector<nx::vms::rules::EventPtr> events;
           events.emplace_back(new TestEvent());
           connector->sendEventBatch(events);

           connector->sendEvent(EventPtr(new TestEvent()));
       });

   future.wait();
}

} // namespace nx::vms::rules::test
