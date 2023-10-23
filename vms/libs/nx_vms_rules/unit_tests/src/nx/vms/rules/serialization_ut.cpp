// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/qobject.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/source_user_field.h>
#include <nx/vms/rules/utils/api.h>
#include <nx/vms/rules/utils/serialization.h>

#include "test_field.h"
#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

TEST(VmsRulesSerialization, EventField)
{
    auto engine = std::make_unique<nx::vms::rules::Engine>(std::make_unique<TestRouter>());
    auto plugin = std::make_unique<nx::vms::rules::test::TestPlugin>(engine.get());

    static const auto kEventType = "nx.vms_rules_serialization.event_field.test";
    static const auto kFieldName = "test";
    engine->registerEventField(fieldMetatype<TestEventField>(), []{ return new TestEventField; });
    engine->registerEvent(
        ItemDescriptor{
            .id = kEventType,
            .displayName = "testField",
            .fields = {
                makeFieldDescriptor<TestEventField>(kFieldName, "Test")
            }
        },
        [](){ return new TestEvent; });

    auto field = std::make_unique<TestEventField>();
    field->id = QnUuid::createUuid();
    field->idSet << QnUuid::createUuid() << QnUuid::createUuid();
    field->string = "test string";
    field->strings << "string 1" << "string 2";
    field->flag = true;
    field->number = 42;
    field->state = State::started;
    field->levels = nx::vms::api::EventLevel::info;

    nx::vms::rules::EventFilter filter(QnUuid::createUuid(), kEventType);
    filter.addField(kFieldName, std::move(field));

    auto sourceField = dynamic_cast<TestEventField*>(filter.fields()[kFieldName]);
    ASSERT_TRUE(sourceField);

    auto filterApi = serialize(&filter);
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
    EXPECT_EQ(sourceField->idSet, resultField->idSet);
    EXPECT_EQ(sourceField->string, resultField->string);
    EXPECT_EQ(sourceField->strings, resultField->strings);
    EXPECT_EQ(sourceField->flag, resultField->flag);
    EXPECT_EQ(sourceField->number, resultField->number);
    EXPECT_EQ(sourceField->state, resultField->state);
    EXPECT_EQ(sourceField->levels, resultField->levels);
}

TEST(VmsRulesSerialization, Event)
{
    const auto sourceEvent = QSharedPointer<TestEvent>::create(
        std::chrono::seconds(5),
        State::started);
    sourceEvent->attributes = nx::common::metadata::Attributes{{"key", "value"}};
    sourceEvent->level = nx::vms::api::EventLevel::error;
    sourceEvent->reason = nx::vms::api::EventReason::networkBadCameraTime;
    sourceEvent->conflicts.camerasByServer["test"] = QStringList{"cam1", "cam2"};

    const auto propData =
        serializeProperties(sourceEvent.get(), nx::utils::propertyNames(sourceEvent.get()));

    auto propJson = QJson::serialized(propData);
    SCOPED_TRACE(propJson.toStdString());

    const auto resultEvent = QSharedPointer<TestEvent>::create();
    deserializeProperties(propData, resultEvent.get());

    EXPECT_EQ(sourceEvent->timestamp(), resultEvent->timestamp());
    EXPECT_EQ(sourceEvent->state(), resultEvent->state());
    EXPECT_EQ(sourceEvent->attributes, resultEvent->attributes);
    EXPECT_EQ(sourceEvent->level, resultEvent->level);
    EXPECT_EQ(sourceEvent->reason, resultEvent->reason);
    EXPECT_EQ(sourceEvent->conflicts, resultEvent->conflicts);
}

TEST(VmsRulesSerialization, VariantConversion)
{
    {
        QString s = "s1";
        QVariant v = s;

        // String may be extracted as string list with single element.
        ASSERT_TRUE(v.canConvert<QStringList>());
        ASSERT_EQ(v.toStringList().size(), 1);
        EXPECT_EQ(v.toStringList()[0], s);
    }
    {
        // Empty variant is null, but not valid.
        QVariant v;
        EXPECT_FALSE(v.isValid());
        EXPECT_TRUE(v.isNull());
    }
    {
        // Empty variant can not convert to string, but toString works.
        QVariant v;
        EXPECT_FALSE(v.canConvert<QString>());
        EXPECT_EQ(v.toString(), QString());
        EXPECT_TRUE(v.toString().isEmpty());
        EXPECT_TRUE(v.toString().isNull());
    }
    {
        // Variant constructed from not null string is not null.
        QVariant v = QString("");
        EXPECT_FALSE(v.isNull());
        EXPECT_TRUE(v.isValid());
    }
}

} // namespace nx::vms::rules::test
