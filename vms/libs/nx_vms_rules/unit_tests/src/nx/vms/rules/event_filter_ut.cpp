// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <thread>

#include <gtest/gtest.h>

#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/keywords_field.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/event_filter_fields/text_field.h>
#include <nx/vms/rules/events/generic_event.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/field.h>

#include "test_event.h"
#include "test_router.h"

namespace nx::vms::rules::test {

namespace {

void addStateField(State state, EventFilter* filter)
{
    auto stateField = std::make_unique<StateField>();
    stateField->setValue(state);
    filter->addField(utils::kStateFieldName, std::move(stateField));
}

void addTextField(const QString& text, EventFilter* filter)
{
    auto field = std::make_unique<EventTextField>();
    field->setValue(text);
    filter->addField("text", std::move(field));
}

} // namespace

class EventFilterTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        engine = std::make_unique<Engine>(std::make_unique<TestRouter>());
        mockRule = std::make_unique<Rule>(QnUuid(), engine.get());

        filter = std::make_unique<EventFilter>(QnUuid::createUuid(), "nx.events.generic");
        filter->addField("source", std::make_unique<KeywordsField>());
        filter->addField("caption", std::make_unique<KeywordsField>());
        filter->addField("description", std::make_unique<KeywordsField>());
        filter->setRule(mockRule.get());

        auto fields = filter->fields();
        sourceField = dynamic_cast<KeywordsField*>(fields.value("source"));
        captionField = dynamic_cast<KeywordsField*>(fields.value("caption"));
        descriptionField = dynamic_cast<KeywordsField*>(fields.value("description"));
    }

    virtual void TearDown() override
    {
        filter.reset();
    }

    EventPtr makeEvent(QString source, QString caption, QString description)
    {
        return QSharedPointer<GenericEvent>::create(
            std::chrono::milliseconds::zero(),
            State::instant,
            source,
            caption,
            description,
            QnUuid::createUuid(),
            QnUuidList({QnUuid::createUuid()}));
    }

    std::unique_ptr <Engine> engine;
    std::unique_ptr<Rule> mockRule;
    std::unique_ptr<EventFilter> filter;

    KeywordsField* sourceField{};
    KeywordsField* captionField{};
    KeywordsField* descriptionField{};
};

// TODO: Does the default filter match to the default event?
// TEST_F(EventFilterTest, defaultGenericEventFilterIsMatchToDefaultGenericEvent)
// {
//     EventPtr genericEvent(new GenericEvent);

//     ASSERT_TRUE(filter->match(genericEvent));
// }

TEST_F(EventFilterTest, allEventFieldsMatchToAllFilterFieldsTest)
{
    auto genericEvent = makeEvent("Source string", "Caption string", "Description string");

    sourceField->setString("Source string");
    captionField->setString("Caption string");
    descriptionField->setString("Description string");

    ASSERT_TRUE(filter->match(genericEvent));
}

TEST_F(EventFilterTest, allEventFieldsMismatchToFilterFieldsTest)
{
    auto genericEvent = makeEvent("Source string", "Caption string", "Description string");

    sourceField->setString("Foo");
    captionField->setString("Bar");
    descriptionField->setString("Baz");

    ASSERT_FALSE(filter->match(genericEvent));
}

TEST_F(EventFilterTest, oneEventFieldMismatchToFilterFieldTest)
{
    auto genericEvent = makeEvent("Source string", "Caption string", "Description string");

    sourceField->setString("Foo");
    captionField->setString("Caption");
    descriptionField->setString("Description");

    ASSERT_FALSE(filter->match(genericEvent));
}

TEST_F(EventFilterTest, handlesStartedProlongedEventProperly)
{
    auto filter = std::make_unique<EventFilter>(QnUuid::createUuid(), "nx.events.test");
    filter->setRule(mockRule.get());
    addStateField(State::started, filter.get());

    EventPtr event(new TestEvent{{}, State::started});

    // Only first started event match.
    ASSERT_TRUE(filter->match(event));
    ASSERT_FALSE(filter->match(event));

    // Stopped event doesn't match, but filter stores that event stopped.
    event->setState(State::stopped);
    ASSERT_FALSE(filter->match(event));

    // New started event match.
    event->setState(State::started);
    ASSERT_TRUE(filter->match(event));
}

TEST_F(EventFilterTest, handlesStoppedProlongedEventProperly)
{
    auto filter = std::make_unique<EventFilter>(QnUuid::createUuid(), "nx.events.test");
    filter->setRule(mockRule.get());
    addStateField(State::stopped, filter.get());

    EventPtr event(new TestEvent{{}, State::stopped});

    // Event not match since there no started event before.
    ASSERT_FALSE(filter->match(event));

    // Started event doesn't match, but filter stores that event started.
    event->setState(State::started);
    ASSERT_FALSE(filter->match(event));

    event->setState(State::stopped);
    ASSERT_TRUE(filter->match(event));
    ASSERT_FALSE(filter->match(event));
}

TEST_F(EventFilterTest, filteredOutEventIsNotRunning)
{
    auto filter = std::make_unique<EventFilter>(QnUuid::createUuid(), "nx.events.test");
    filter->setRule(mockRule.get());
    addStateField(State::started, filter.get());
    addTextField("a", filter.get());

    auto eventA = TestEventPtr::create(std::chrono::microseconds::zero(), State::started);
    eventA->m_text = "a";

    auto eventB = TestEventPtr::create(std::chrono::microseconds::zero(), State::started);
    eventB->m_text = "b";

    ASSERT_EQ(eventA->resourceKey(), eventB->resourceKey());

    // Event doesn't match, and resource key is not stored.
    ASSERT_FALSE(filter->match(eventB));

    // Matched event is stored and double start is prevented.
    ASSERT_TRUE(filter->match(eventA));
    ASSERT_FALSE(filter->match(eventA));
}

TEST_F(EventFilterTest, cacheKey)
{
    auto filter = std::make_unique<EventFilter>(QnUuid::createUuid(), "nx.events.test");
    filter->setRule(mockRule.get());
    addTextField({}, filter.get());

    auto event = TestEventPtr::create();
    event->m_text = "text";

    // Empty cache key events are not cached.
    EXPECT_TRUE(filter->match(event));
    EXPECT_TRUE(filter->match(event));

    // Same cache events are ignored.
    event->setCacheKey("a");
    EXPECT_TRUE(filter->match(event));
    EXPECT_FALSE(filter->match(event));

    // Different cache events are matched.
    event->setCacheKey("b");
    EXPECT_TRUE(filter->match(event));
    EXPECT_FALSE(filter->match(event));
}

TEST_F(EventFilterTest, cacheTimeout)
{
    using namespace std::chrono;

    auto filter = std::make_unique<EventFilter>(QnUuid::createUuid(), "nx.events.test");
    filter->setRule(mockRule.get());
    addTextField({}, filter.get());

    auto event = TestEventPtr::create();
    event->m_text = "text";

    // Same cache events are ignored.
    event->setCacheKey("a");
    EXPECT_TRUE(filter->match(event));
    EXPECT_FALSE(filter->match(event));

    // Same cache events are matched after timeout.
    std::this_thread::sleep_for(5ms);
    engine->eventCache()->cleanupOldEventsFromCache(1ms, 1ms);
    EXPECT_TRUE(filter->match(event));
    EXPECT_FALSE(filter->match(event));
}

} // nx::vms::rules::test
