// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_fields/keywords_field.h>
#include <nx/vms/rules/event_fields/state_field.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/events/generic_event.h>
#include <nx/vms/rules/utils/field.h>

#include "test_event.h"

namespace nx::vms::rules::test {

class EventFilterTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        filter = std::make_unique<EventFilter>(QnUuid::createUuid(), "nx.events.generic");
        filter->addField("source", std::make_unique<KeywordsField>());
        filter->addField("caption", std::make_unique<KeywordsField>());
        filter->addField("description", std::make_unique<KeywordsField>());

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
            description);
    }

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

TEST(EventFilterProlongedEventTest, eventFilterHandlesStartedProlongedEventProperly)
{
    auto filter = std::make_unique<EventFilter>(QnUuid::createUuid(), "nx.events.test");
    auto stateField = std::make_unique<StateField>();
    stateField->setValue(State::started);
    filter->addField(utils::kStateFieldName, std::move(stateField));

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

TEST(EventFilterProlongedEventTest, eventFilterHandlesStoppedProlongedEventProperly)
{
    auto filter = std::make_unique<EventFilter>(QnUuid::createUuid(), "nx.events.test");
    auto stateField = std::make_unique<StateField>();
    stateField->setValue(State::stopped);
    filter->addField(utils::kStateFieldName, std::move(stateField));

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

} // nx::vms::rules::test
