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

#include "test_router.h"

namespace nx::vms::rules::test {

namespace {

const FieldDescriptor kDummyDescriptor;

} // namespace

class EventFilterTest: public EngineBasedTest
{
protected:
    virtual void SetUp() override
    {
        mockRule = std::make_unique<Rule>(nx::Uuid(), engine.get());

        filter = std::make_unique<EventFilter>(nx::Uuid::createUuid(), "nx.events.generic");
        filter->addField("source", std::make_unique<KeywordsField>(&kDummyDescriptor));
        filter->addField("caption", std::make_unique<KeywordsField>(&kDummyDescriptor));
        filter->addField("description", std::make_unique<KeywordsField>(&kDummyDescriptor));
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
            nx::Uuid::createUuid(),
            UuidList({nx::Uuid::createUuid()}));
    }

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

} // namespace nx::vms::rules::test
