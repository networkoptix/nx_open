// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <analytics/common/object_metadata.h>
#include <nx/utils/log/format.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/manifest.h>

namespace nx::vms::rules::test {

namespace {

const FieldDescriptor kDummyDescriptor;

template<class Field>
void testSimpleTypeField(
    const std::vector<typename Field::value_type> values,
    bool matchAny = false)
{
    using ValueType = typename Field::value_type;

    for (const auto& value: values)
    {
        Field field{&kDummyDescriptor};
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
    const auto group = createUserGroup(
        "User input group", NoGroup, {{api::kAllDevicesGroupId, api::AccessRight::userInput}});
    const auto user = addUser({group.id});
    const auto userIdValue = QVariant::fromValue(user->getId());

    auto field = SourceUserField(systemContext(), &kDummyDescriptor);

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

class LookupFieldTestBase: public nx::vms::common::test::ContextBasedTest
{
protected:
    using Attributes = nx::common::metadata::Attributes;

    nx::Uuid makeAndRegisterList(
        std::vector<QString> attributes = {},
        std::vector<std::map<QString, QString>> entries = {})
    {
        const auto id = nx::Uuid::createUuid();

        api::LookupListData lookupList;
        lookupList.id = id;
        lookupList.attributeNames = std::move(attributes);
        lookupList.entries = std::move(entries);

        systemContext()->lookupListManager()->initialize({std::move(lookupList)});

        return id;
    }
};

class TextLookupFieldTest: public LookupFieldTestBase
{
};

TEST_F(TextLookupFieldTest, emptyLookupList)
{
    const auto listId = makeAndRegisterList();

    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toString());

    field.setCheckType(TextLookupCheckType::inList);
    // An empty List will always return false for 'inList' comparison.
    ASSERT_FALSE(field.match(QString{"foo"}));

    // And always return true for 'notInList' comparison.
    field.setCheckType(TextLookupCheckType::notInList);
    ASSERT_TRUE(field.match(QString{"foo"}));
}

TEST_F(TextLookupFieldTest, emptyKeywords)
{
    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue("");

    // An empty keywords always return true for 'containsKeywords' comparison.
    field.setCheckType(TextLookupCheckType::containsKeywords);
    ASSERT_TRUE(field.match(QString{"foo"}));
    // And always return true for 'doesNotContainKeywords' comparison.
    field.setCheckType(TextLookupCheckType::doesNotContainKeywords);
    ASSERT_FALSE(field.match(QString{"foo"}));
}

TEST_F(TextLookupFieldTest, lookupInKeywordsWorksProperly)
{
    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue("foo bar");

    field.setCheckType(TextLookupCheckType::containsKeywords);
    // Check values list contains.
    ASSERT_TRUE(field.match(QString{"foo"}));
    ASSERT_TRUE(field.match(QString{"bar"}));
    ASSERT_TRUE(field.match(QString{"foo_bar"}));
    // Check value list does not contains.
    ASSERT_FALSE(field.match(QString{"baz"}));

    field.setCheckType(TextLookupCheckType::doesNotContainKeywords);
    ASSERT_FALSE(field.match(QString{"foo"}));
    ASSERT_FALSE(field.match(QString{"bar"}));
    ASSERT_FALSE(field.match(QString{"foo_bar"}));
    ASSERT_TRUE(field.match(QString{"baz"}));
}

TEST_F(TextLookupFieldTest, lookupInListWorksProperly)
{
    const auto listId = makeAndRegisterList({"foo"},
        {
            {{"foo", "1"}},
            {{"foo", "2"}},
            {{"foo", "three"}}
        });

    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toString());

    field.setCheckType(TextLookupCheckType::inList);
    // Check attributes lookup list contains.
    ASSERT_TRUE(field.match(QString{"1"}));
    ASSERT_TRUE(field.match(QString{"2"}));
    ASSERT_TRUE(field.match(QString{"three"}));
    // Check attribute lookup list does not contains.
    ASSERT_FALSE(field.match(QString{"four"}));

    field.setCheckType(TextLookupCheckType::notInList);
    ASSERT_FALSE(field.match(QString{"1"}));
    ASSERT_FALSE(field.match(QString{"2"}));
    ASSERT_FALSE(field.match(QString{"three"}));
    ASSERT_TRUE(field.match(QString{"four"}));
}

class ObjectLookupFieldTest: public LookupFieldTestBase
{
};

TEST_F(ObjectLookupFieldTest, emptyLookupList)
{
    const auto listId = makeAndRegisterList();

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toString());

    field.setCheckType(ObjectLookupCheckType::inList);
    // An empty List will always return false for 'inList' comparison.
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "bar"}})));

    // And always return true for 'notInList' comparison.
    field.setCheckType(ObjectLookupCheckType::notInList);
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "bar"}})));
}

TEST_F(ObjectLookupFieldTest, emptyAttributes)
{
    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue("");

    field.setCheckType(ObjectLookupCheckType::hasAttributes);
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "bar"}})));
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"baz", "quux"}})));
}

TEST_F(ObjectLookupFieldTest, emptyListAttributeMatchAnyInputValue)
{
    const auto listId = makeAndRegisterList({"foo"},
        {
            {{"foo", ""}} //< The entry accepts any value for the 'foo' attribute.
        });

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toString());

    field.setCheckType(ObjectLookupCheckType::inList);
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}})));

    field.setCheckType(ObjectLookupCheckType::notInList);
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}})));
}

TEST_F(ObjectLookupFieldTest, lookupWithAttributesWorksProperly)
{
    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setCheckType(ObjectLookupCheckType::hasAttributes);

    field.setValue("foo=1");
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"baz", "3"}})));

    field.setValue("foo=1 bar=two");
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"bar", "two"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"bar", "2"}})));

    field.setValue("foo=1 bar!=two");
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"bar", "three"}})));
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"bar", "two"}})));
}

TEST_F(ObjectLookupFieldTest, lookupInListWorksProperly)
{
    const auto listId = makeAndRegisterList({"foo", "bar.baz", "bar.quux"},
        {
            {{"foo", "1"}, {"bar.baz", "one"}, {"bar.quux", "0x1"}},
            {{"foo", "2"}, {"bar.baz", "two"}, {"bar.quux", "0x2"}}
        });

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toString());

    field.setCheckType(ObjectLookupCheckType::inList);
    // Check attributes lookup list contains.
    ASSERT_TRUE(field.match(
        QVariant::fromValue(Attributes{{"foo", "1"}, {"bar.baz", "one"}, {"bar.quux", "0x1"}})));
    ASSERT_TRUE(field.match(
        QVariant::fromValue(Attributes{{"foo", "2"}, {"bar.baz", "two"}, {"bar.quux", "0x2"}})));
    // Check attribute lookup list does not contains.
    ASSERT_FALSE(field.match(
        QVariant::fromValue(Attributes{{"foo", "3"}, {"bar.baz", "three"}, {"bar.quux", "0x3"}})));
    // Check attribute lookup list partially not contains.
    ASSERT_FALSE(field.match(
        QVariant::fromValue(Attributes{{"foo", "1"}, {"baz.baz", "one"}, {"bar.quux", "0x3"}})));
    // Check attribute name list does not contain.
    ASSERT_FALSE(field.match(
        QVariant::fromValue(Attributes{{"unexpected", "1"}, {"baz.baz", "one"}, {"bar.quux", "0x3"}})));

    field.setCheckType(ObjectLookupCheckType::notInList);
    ASSERT_FALSE(field.match(
        QVariant::fromValue(Attributes{{"foo", "1"}, {"bar.baz", "one"}, {"bar.quux", "0x1"}})));
    ASSERT_FALSE(field.match(
        QVariant::fromValue(Attributes{{"foo", "2"}, {"bar.baz", "two"}, {"bar.quux", "0x2"}})));
    ASSERT_TRUE(field.match(
        QVariant::fromValue(Attributes{{"foo", "3"}, {"bar.baz", "three"}, {"bar.quux", "0x3"}})));
    ASSERT_TRUE(field.match(
        QVariant::fromValue(Attributes{{"foo", "1"}, {"baz.baz", "one"}, {"bar.quux", "0x3"}})));
    ASSERT_TRUE(field.match(
        QVariant::fromValue(Attributes{{"unexpected", "1"}, {"baz.baz", "one"}, {"bar.quux", "0x3"}})));
}

TEST(EventFieldTest, SimpleTypes)
{
    testSimpleTypeField<EventFlagField>({true, false});
    testSimpleTypeField<ExpectedUuidField>({nx::Uuid(), nx::Uuid::createUuid()});
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
        KeywordsField field{&kDummyDescriptor};
        field.setString(data.keywords);

        SCOPED_TRACE(nx::format("Input: %1, keywords: %2", data.input, data.keywords).toStdString());
        EXPECT_EQ(field.match(data.input), data.match);
    }
}

} // namespace nx::vms::rules::test
