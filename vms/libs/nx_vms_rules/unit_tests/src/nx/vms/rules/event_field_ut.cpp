// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <analytics/common/object_metadata.h>
#include <nx/utils/log/format.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>

namespace nx::vms::rules::test {

namespace {

template<class Field>
void testSimpleTypeField(
    const std::vector<typename Field::value_type> values,
    bool matchAny = false)
{
    using ValueType = typename Field::value_type;

    for (const auto& value: values)
    {
        Field field;
        SCOPED_TRACE(nx::format(
            "Field type: %1, value: %2",
            field.metatype(),
            value).toStdString());

        EXPECT_EQ(field.value(), ValueType());

        if (matchAny)
        {
            EXPECT_TRUE(field.match(QVariant()));
            EXPECT_TRUE(field.match(QVariant::fromValue(ValueType())));
            EXPECT_TRUE(field.match(QVariant::fromValue(value)));
        }

        field.setValue(value);
        EXPECT_EQ(field.value(), value);

        EXPECT_TRUE(field.match(QVariant::fromValue<ValueType>(value)));
    }
}

} // namespace

class EventFieldContextTest: public nx::vms::common::test::ContextBasedTest
{
};

TEST_F(EventFieldContextTest, SourceUserField)
{
    const auto permission = nx::vms::api::GlobalPermission::userInput;
    const auto user = addUser(NoGroup, kTestUserName, api::UserType::local, permission);
    const auto userIdValue = QVariant::fromValue(user->getId());
    const auto group = createUserGroup(permission);

    user->setGroupIds({group.id});

    auto field = SourceUserField(systemContext());

    // Default field do not match anything.
    EXPECT_FALSE(field.match(userIdValue));

    // Accept all match any user.
    field.setAcceptAll(true);
    EXPECT_TRUE(field.match(userIdValue));
    EXPECT_TRUE(field.match({}));
    field.setAcceptAll(false);

    // User id is matched.
    field.setIds({user->getId()});
    EXPECT_TRUE(field.match(userIdValue));

    // User role is matched.
    field.setIds({group.id});
    EXPECT_TRUE(field.match(userIdValue));
}

class LookupFieldContextTest: public nx::vms::common::test::ContextBasedTest
{
protected:
    using Attributes = nx::common::metadata::Attributes;

    QnUuid makeAndRegisterList(
        std::vector<QString> attributes = {},
        std::vector<std::map<QString, QString>> entries = {})
    {
        const auto id = QnUuid::createUuid();

        api::LookupListData lookupList;
        lookupList.id = id;
        lookupList.attributeNames = std::move(attributes);
        lookupList.entries = std::move(entries);

        systemContext()->lookupListManager()->initialize({std::move(lookupList)});

        return id;
    }
};

TEST_F(LookupFieldContextTest, emptyLookupList)
{
    const auto listId = makeAndRegisterList();

    LookupField field{systemContext()};
    field.setSource(LookupSource::lookupList);
    field.setValue(listId.toString());

    // An empty List will always return false for 'in' comparison.
    field.setCheckType(LookupCheckType::in);
    ASSERT_FALSE(field.match(QVariant{"1"}));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "bar"}})));
    // And always return true for 'out' comparison.
    field.setCheckType(LookupCheckType::out);
    ASSERT_TRUE(field.match(QVariant{"1"}));
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "bar"}})));
}

TEST_F(LookupFieldContextTest, emptyKeywords)
{
    LookupField field{systemContext()};
    field.setSource(LookupSource::keywords);
    field.setValue(""); //< Transforms to lookup list {'keyword': ''}.

    // An empty keywords always return true for 'in' comparison.
    field.setCheckType(LookupCheckType::in);
    ASSERT_TRUE(field.match(QVariant{"1"}));
    // And always return true for 'out' comparison.
    field.setCheckType(LookupCheckType::out);
    ASSERT_FALSE(field.match(QVariant{"1"}));
}

TEST_F(LookupFieldContextTest, emptyListAttributeMatchAnyInputValue)
{
    const auto listId = makeAndRegisterList({"foo"},
        {
            {{"foo", ""}} //< The entry accepts any value for the 'foo' attribute.
        });

    LookupField field{systemContext()};
    field.setSource(LookupSource::lookupList);
    field.setValue(listId.toString());

    field.setCheckType(LookupCheckType::in);
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}})));

    field.setCheckType(LookupCheckType::out);
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}})));
}

TEST_F(LookupFieldContextTest, keywordsSource)
{
    LookupField field{systemContext()};
    field.setSource(LookupSource::keywords);

    field.setValue("foo bar"); //< Transforms to lookup list {'keyword': 'foo', 'keyword': 'bar'}.

    field.setCheckType(LookupCheckType::in);
    // Check values list contains.
    ASSERT_TRUE(field.match(QVariant{"foo"}));
    ASSERT_TRUE(field.match(QVariant{"bar"}));
    ASSERT_TRUE(field.match(QVariant{"foo_bar"}));
    // Check value list does not contains.
    ASSERT_FALSE(field.match(QVariant{"baz"}));

    field.setCheckType(LookupCheckType::out);
    ASSERT_FALSE(field.match(QVariant{"foo"}));
    ASSERT_FALSE(field.match(QVariant{"bar"}));
    ASSERT_FALSE(field.match(QVariant{"foo_bar"}));
    ASSERT_TRUE(field.match(QVariant{"baz"}));
}

TEST_F(LookupFieldContextTest, lookupListSource)
{
    const auto listId = makeAndRegisterList({"foo", "bar"},
        {
            {{"foo", "1"}, {"bar", "one"}},
            {{"foo", "2"}, {"bar", "two"}}
        });

    LookupField field{systemContext()};
    field.setSource(LookupSource::lookupList);
    field.setValue(listId.toString());

    field.setCheckType(LookupCheckType::in);
    // Check attributes lookup list contains.
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"bar", "one"}})));
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}, {"bar", "two"}})));
    // Check attribute lookup list does not contains.
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "3"}, {"bar", "three"}})));
    // Check attribute lookup list partially not contains.
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"baz", "three"}})));

    field.setCheckType(LookupCheckType::out);
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"bar", "one"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}, {"bar", "two"}})));
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "3"}, {"bar", "three"}})));
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"baz", "three"}})));
}

TEST_F(LookupFieldContextTest, lookupListSelectedAttributes)
{
    const auto listId = makeAndRegisterList({"foo", "bar", "baz"},
        {
            {{"foo", "1"}, {"bar", "one"}, {"baz", "0x1"}},
            {{"foo", "2"}, {"bar", "two"}, {"baz", "0x2"}},
            {{"foo", "3"}, {"bar", ""}, {"baz", "0x3"}}, //< The entry accepts any value for the 'bar' attribute.
        });

    LookupField field{systemContext()};
    field.setSource(LookupSource::lookupList);
    field.setValue(listId.toString());
    // Checks only few attributes from the lookup list.
    field.setAttributes({"foo", "bar"});

    field.setCheckType(LookupCheckType::in);
    // Check attributes lookup list contains.
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"bar", "one"}, {"baz", "0x1"}})));
    // Check attributes where not selected attribute is not fit any record.
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}, {"bar", "two"}, {"baz", "invalid"}})));
    // Check the case when the first attribute is in the lookup list, the second might be any value
    // and the third attribute is absent(must not be a problem as not selected as attribute to check).
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "3"}, {"bar", "any"}})));
    // Check attributes lookup list not contains.
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "4"}, {"bar", "four"}, {"baz", "0x4"}})));

    field.setCheckType(LookupCheckType::out);
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"bar", "one"}, {"baz", "0x1"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}, {"bar", "two"}, {"baz", "invalid"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "3"}, {"bar", "any"}})));
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "4"}, {"bar", "four"}, {"baz", "0x4"}})));
}

TEST(EventFieldTest, SimpleTypes)
{
    testSimpleTypeField<EventFlagField>({true, false});
    testSimpleTypeField<ExpectedUuidField>({QnUuid(), QnUuid::createUuid()});
    testSimpleTypeField<IntField>({-1, 0, 60, 300, 86400});
    testSimpleTypeField<EventTextField>({ "", "Hello", "\\/!@#$%^&*()_+" }, true);
}

TEST(EventFieldTest, KeywordsField)
{
    static const struct {
        QString input;
        QString keywords;
        bool match = false;
    } testData[] = {
        // Empty keywords match any input.
        {"", "", true},
        {"baz", "", true},
        // Empty input do not match valid keywords.
        {"", "baz", false},
        // Valid keywords match input.
        {"baz", "baz", true},
        {"fizz baz bar", "baz", true},
        // Any keyword match is enough.
        {"fizz baz bar", "baz fizz", true},
        {"fizz bar", "baz fizz", true},
        {"fizz", "baz", false},
        // Keywords are trimmed and unquoted before match, quoted keywords may contain space.
        {"fizz", "baz   \"fizz\"", true},
        {"fizz baz bar", "\"baz bar\"", true},
        {"fizz baz  bar", "\"baz bar\"", false},
        {"fizz bar baz", "\"baz bar\"", false},
        // Keywords are first trimmed and then unqouted, not vice versa.
        {"fizz", "baz \" fizz\"", false},
        {"fizz", "baz \"fizz \"", false},
    };

    for(const auto& data: testData)
    {
        KeywordsField field;
        field.setString(data.keywords);

        SCOPED_TRACE(nx::format("Input: %1, keywords: %2", data.input, data.keywords).toStdString());
        EXPECT_EQ(field.match(data.input), data.match);
    }
}

} // namespace nx::vms::rules::test
