// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_fields/source_user_field.h>

#include "test_field.h"
#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

TEST(VmsRulesSerialization, EventField)
{
    auto engine = std::make_unique<nx::vms::rules::Engine>(std::make_unique<TestRouter>());
    auto plugin = std::make_unique<nx::vms::rules::test::TestPlugin>(engine.get());
    plugin->registerFields();

    auto field = std::make_unique<TestEventField>();
    field->id = QnUuid::createUuid();
    field->idsList << QnUuid::createUuid() << QnUuid::createUuid();
    field->idsSet << QnUuid::createUuid() << QnUuid::createUuid();
    field->string = "test string";
    field->strings << "string 1" << "string 2";
    field->flag = true;
    field->number = 42;
    field->state = State::started;

    static const auto kFieldName = "test";
    nx::vms::rules::EventFilter filter(QnUuid::createUuid(), "nx.events.test");
    filter.addField(kFieldName, std::move(field));

    auto sourceField = dynamic_cast<TestEventField*>(filter.fields()[kFieldName]);
    ASSERT_TRUE(sourceField);

    auto filterApi = engine->serialize(&filter);
    auto filterJson = QJson::serialized(filterApi);
    SCOPED_TRACE(filterJson.toStdString());

    auto newFilter = engine->buildEventFilter(filterApi);
    ASSERT_TRUE(newFilter);

    auto newField = newFilter->fields()[kFieldName];
    ASSERT_TRUE(newField);

    ASSERT_EQ(sourceField->metatype(), newField->metatype());
    auto resultField = dynamic_cast<TestEventField*>(newField);
    ASSERT_TRUE(resultField);

    EXPECT_EQ(sourceField->id, resultField->id);
    EXPECT_EQ(sourceField->idsList, resultField->idsList);
    EXPECT_EQ(sourceField->idsSet, resultField->idsSet);
    EXPECT_EQ(sourceField->string, resultField->string);
    EXPECT_EQ(sourceField->strings, resultField->strings);
    EXPECT_EQ(sourceField->flag, resultField->flag);
    EXPECT_EQ(sourceField->number, resultField->number);
    EXPECT_EQ(sourceField->state, resultField->state);
}

} // namespace nx::vms::rules::test
