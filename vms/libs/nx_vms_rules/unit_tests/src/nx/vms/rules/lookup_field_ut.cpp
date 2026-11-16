// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <atomic>
#include <map>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <analytics/common/object_metadata.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/manifest.h>

namespace nx::vms::rules::test {

namespace {

struct TestData
{
    std::vector<QString> attributes;
    std::vector<std::map<QString, QString>> entries;
    bool isGeneric = true;
};

const FieldDescriptor kDummyDescriptor;

} // namespace

class LookupFieldTestBase:
    public nx::vms::common::test::ContextBasedTest,
    public testing::WithParamInterface<TestData>
{
protected:
    using Attributes = nx::common::metadata::Attributes;

    nx::Uuid makeAndRegisterList(std::vector<QString> attributes = {},
        std::vector<std::map<QString, QString>> entries = {},
        bool isGeneric = true)
    {
        const auto id = nx::Uuid::createUuid();

        api::LookupListData lookupList;
        lookupList.id = id;
        lookupList.attributeNames = std::move(attributes);
        lookupList.entries = std::move(entries);
        lookupList.objectTypeId = isGeneric ? std::string() : "nx.test.type";

        systemContext()->lookupListManager()->initialize({std::move(lookupList)});

        return id;
    }
};

class TextLookupFieldTest: public LookupFieldTestBase
{
};

TEST_F(TextLookupFieldTest, disabledLookupList)
{
    const auto listId = makeAndRegisterList();

    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toSimpleString());

    // If field is disabled it will always return true
    field.setCheckType(TextLookupCheckType::inList);
    ASSERT_TRUE(field.match(QString{}));
    ASSERT_TRUE(field.match(QString{"foo"}));

    field.setCheckType(TextLookupCheckType::notInList);
    ASSERT_TRUE(field.match(QString{}));
    ASSERT_TRUE(field.match(QString{"foo"}));

    field.setCheckType(TextLookupCheckType::containsKeywords);
    ASSERT_TRUE(field.match(QString{}));
    ASSERT_TRUE(field.match(QString{"foo"}));

    field.setCheckType(TextLookupCheckType::doesNotContainKeywords);
    ASSERT_TRUE(field.match(QString{}));
    ASSERT_TRUE(field.match(QString{"foo"}));
}

TEST_F(TextLookupFieldTest, emptyLookupList)
{
    const auto listId = makeAndRegisterList();

    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setEnabled(true);
    field.setValue(listId.toSimpleString());

    field.setCheckType(TextLookupCheckType::inList);
    // An empty List will always return false for 'inList' comparison.
    ASSERT_FALSE(field.match(QString{}));
    ASSERT_FALSE(field.match(QString{"foo"}));

    // And always return true for 'notInList' comparison.
    field.setCheckType(TextLookupCheckType::notInList);
    ASSERT_TRUE(field.match(QString{}));
    ASSERT_TRUE(field.match(QString{"foo"}));
}

TEST_F(TextLookupFieldTest, emptyKeywords)
{
    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue({});

    // An empty keywords always return true for 'containsKeywords' comparison.
    field.setCheckType(TextLookupCheckType::containsKeywords);
    ASSERT_TRUE(field.match(QString{}));
    ASSERT_TRUE(field.match(QString{"foo"}));

    // And always return true for 'doesNotContainKeywords' comparison.
    field.setCheckType(TextLookupCheckType::doesNotContainKeywords);
    ASSERT_TRUE(field.match(QString{}));
    ASSERT_TRUE(field.match(QString{"foo"}));
}

TEST_F(TextLookupFieldTest, lookupInKeywordsWorksProperly)
{
    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setEnabled(true);
    field.setValue("foo bar");

    field.setCheckType(TextLookupCheckType::containsKeywords);
    // Check values list contains.
    ASSERT_TRUE(field.match(QString{"foo"}));
    ASSERT_TRUE(field.match(QString{"bar"}));
    ASSERT_TRUE(field.match(QString{"foo_bar"}));
    // Check value list does not contains.
    ASSERT_FALSE(field.match(QString{}));
    ASSERT_FALSE(field.match(QString{"baz"}));

    field.setCheckType(TextLookupCheckType::doesNotContainKeywords);
    // Value contains keywords - no match.
    ASSERT_FALSE(field.match(QString{"foo"}));
    ASSERT_FALSE(field.match(QString{"bar"}));
    ASSERT_FALSE(field.match(QString{"foo_bar"}));
    // Value does not contain keywords - match.
    ASSERT_TRUE(field.match(QString{}));
    ASSERT_TRUE(field.match(QString{"baz"}));
}

TEST_F(TextLookupFieldTest, lookupInListWorksProperly)
{
    const auto listId =
        makeAndRegisterList({"foo"}, {{{"foo", "1"}}, {{"foo", "2"}}, {{"foo", "three"}}});

    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setEnabled(true);
    field.setValue(listId.toSimpleString());

    field.setCheckType(TextLookupCheckType::inList);
    // Check attributes lookup list contains.
    ASSERT_TRUE(field.match(QString{"1"}));
    ASSERT_TRUE(field.match(QString{"2"}));
    ASSERT_TRUE(field.match(QString{"three"}));
    // Check value contains lookup list entry
    ASSERT_TRUE(field.match(QString{"three123"}));
    // Check attribute lookup list does not contains.
    ASSERT_FALSE(field.match(QString{"four"}));
    ASSERT_FALSE(field.match(QString{}));

    field.setCheckType(TextLookupCheckType::notInList);
    // Value in list - no match.
    ASSERT_FALSE(field.match(QString{"1"}));
    ASSERT_FALSE(field.match(QString{"2"}));
    ASSERT_FALSE(field.match(QString{"three123"}));
    ASSERT_FALSE(field.match(QString{"three"}));
    // Value not in list - match.
    ASSERT_TRUE(field.match(QString{}));
    ASSERT_TRUE(field.match(QString{"four"}));
}

TEST_F(TextLookupFieldTest, lookupListChangesAreApplied)
{
    const auto listId = makeAndRegisterList({"foo"}, {{{"foo", "1"}}});
    const auto manager = systemContext()->lookupListManager();

    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setEnabled(true);
    field.setValue(listId.toSimpleString());
    field.setCheckType(TextLookupCheckType::inList);

    EXPECT_TRUE(field.match(QString{"1"}));
    EXPECT_FALSE(field.match(QString{"2"}));

    auto list = manager->lookupList(listId);
    list.entries.push_back({{"foo", "2"}});
    manager->addOrUpdate(list);
    EXPECT_TRUE(field.match(QString{"2"}));

    manager->remove(listId);
    EXPECT_FALSE(field.match(QString{"1"}));
}

TEST_F(TextLookupFieldTest, lookupListFollowsManagerInitialization)
{
    api::LookupListData list;
    list.id = nx::Uuid::createUuid();
    list.attributeNames = {"foo"};
    list.entries = {{{"foo", "1"}}};

    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setEnabled(true);
    field.setValue(list.id.toSimpleString());
    field.setCheckType(TextLookupCheckType::inList);

    // The lists aren't loaded yet.
    EXPECT_FALSE(field.match(QString{"1"}));

    const auto manager = systemContext()->lookupListManager();
    manager->initialize({list});
    EXPECT_TRUE(field.match(QString{"1"}));

    manager->deinitialize();
    EXPECT_FALSE(field.match(QString{"1"}));
}

TEST_F(TextLookupFieldTest, lookupListAddedAfterMatch)
{
    makeAndRegisterList({"foo"}, {{{"foo", "1"}}});

    api::LookupListData list;
    list.id = nx::Uuid::createUuid();
    list.attributeNames = {"foo"};
    list.entries = {{{"foo", "1"}}};

    TextLookupField field{systemContext(), &kDummyDescriptor};
    field.setEnabled(true);
    field.setValue(list.id.toSimpleString());
    field.setCheckType(TextLookupCheckType::notInList);

    // The list doesn't exist yet.
    EXPECT_FALSE(field.match(QString{"2"}));

    systemContext()->lookupListManager()->addOrUpdate(list);
    EXPECT_TRUE(field.match(QString{"2"}));
    EXPECT_FALSE(field.match(QString{"1"}));
}

//-------------------------------------------------------------------------------------------------

class ObjectLookupFieldTest: public LookupFieldTestBase
{
};

TEST_F(ObjectLookupFieldTest, emptyLookupList)
{
    const auto listId = makeAndRegisterList({}, {}, /* isGeneric */ false);

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toSimpleString());

    field.setCheckType(ObjectLookupCheckType::inList);
    // An empty List will always return false for 'inList' comparison.
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "bar"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(Attributes{})));

    // And always return true for 'notInList' comparison.
    field.setCheckType(ObjectLookupCheckType::notInList);
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "bar"}})));
}

TEST_F(ObjectLookupFieldTest, emptyAttributeFilter)
{
    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue("");

    field.setCheckType(ObjectLookupCheckType::hasAttributes);
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "bar"}})));
    ASSERT_TRUE(field.match(QVariant::fromValue(Attributes{{"baz", "quux"}})));
}

TEST_F(ObjectLookupFieldTest, emptyList)
{
    const auto listId = makeAndRegisterList({"foo, bar"}, {}, /* isGeneric */ false);

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toSimpleString());

    field.setCheckType(ObjectLookupCheckType::inList);
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{})));

    field.setCheckType(ObjectLookupCheckType::notInList);
    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{})));
}

// The entries accept any value for the 'foo' attribute.
const auto kEmptyEntryData = std::vector<TestData>{
    {{"foo, bar"}, {{{"foo", ""}, {"bar", "1"}}}, /* isGeneric */ false},
    {{"foo, bar"}, {{{"bar", "1"}}}, /* isGeneric */ false},
};

TEST_P(ObjectLookupFieldTest, emptyListAttributeMatchAnyInputValue)
{
    const auto& data = GetParam();

    const auto listId = makeAndRegisterList(data.attributes, data.entries, data.isGeneric);

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toSimpleString());

    field.setCheckType(ObjectLookupCheckType::inList);
    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"bar", "1"}})));
    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}, {"bar", "1"}})));
    // Empty attributes doesn't match lookup list.
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{})));

    field.setCheckType(ObjectLookupCheckType::notInList);
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"bar", "1"}})));
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}, {"bar", "1"}})));
}

INSTANTIATE_TEST_SUITE_P(EmptyEntry, ObjectLookupFieldTest, testing::ValuesIn(kEmptyEntryData));

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

TEST_F(ObjectLookupFieldTest, lookupInGenericListWorksProperly)
{
    const auto listId = makeAndRegisterList({"Value"},
        {
            {{"Value", "1"}},
            {{"Value", "one"}},
            {{"Value", "0x1"}},
        });
    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toSimpleString());

    field.setCheckType(ObjectLookupCheckType::inList);
    // Check attributes lookup list contains.
    ASSERT_TRUE(field.match(
        QVariant::fromValue(Attributes{{"foo", "1"}, {"bar.baz", "one"}, {"bar.quux", "0x1"}})));

    // Check attribute lookup list does not contain.
    ASSERT_FALSE(field.match(
        QVariant::fromValue(Attributes{{"foo", "3"}, {"bar.baz", "three"}, {"bar.quux", "0x3"}})));

    field.setCheckType(ObjectLookupCheckType::notInList);
    ASSERT_FALSE(field.match(
        QVariant::fromValue(Attributes{{"foo", "1"}, {"bar.baz", "one"}, {"bar.quux", "0x1"}})));
    ASSERT_FALSE(field.match(QVariant::fromValue(
        Attributes{{"foo", "Value"}, {"bar.baz", "two"}, {"bar.quux", "0x1"}})));
    ASSERT_TRUE(field.match(
        QVariant::fromValue(Attributes{{"foo", "3"}, {"bar.baz", "three"}, {"bar.quux", "0x3"}})));
}

TEST_F(ObjectLookupFieldTest, lookupInListWorksProperly)
{
    const auto listId = makeAndRegisterList({"foo", "bar.baz", "bar.quux"},
        {{{"foo", "1"}, {"bar.baz", "one"}, {"bar.quux", "0x1"}},
            {{"foo", "2"}, {"bar.baz", "two"}, {"bar.quux", "0x2"}}},
        /* isGeneric */ false);

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toSimpleString());

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
    ASSERT_FALSE(field.match(QVariant::fromValue(
        Attributes{{"unexpected", "1"}, {"baz.baz", "one"}, {"bar.quux", "0x3"}})));
    // Absent attribute does not match.
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"baz.baz", "one"}})));

    field.setCheckType(ObjectLookupCheckType::notInList);
    ASSERT_FALSE(field.match(
        QVariant::fromValue(Attributes{{"foo", "1"}, {"bar.baz", "one"}, {"bar.quux", "0x1"}})));
    ASSERT_FALSE(field.match(
        QVariant::fromValue(Attributes{{"foo", "2"}, {"bar.baz", "two"}, {"bar.quux", "0x2"}})));
    ASSERT_TRUE(field.match(
        QVariant::fromValue(Attributes{{"foo", "3"}, {"bar.baz", "three"}, {"bar.quux", "0x3"}})));
    ASSERT_TRUE(field.match(
        QVariant::fromValue(Attributes{{"foo", "1"}, {"baz.baz", "one"}, {"bar.quux", "0x3"}})));
    ASSERT_TRUE(field.match(QVariant::fromValue(
        Attributes{{"unexpected", "1"}, {"baz.baz", "one"}, {"bar.quux", "0x3"}})));
    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}, {"baz.baz", "one"}})));
}

TEST_F(ObjectLookupFieldTest, lookupListChangesAreApplied)
{
    const auto listId = makeAndRegisterList({"foo"}, {{{"foo", "1"}}}, /* isGeneric */ false);
    const auto manager = systemContext()->lookupListManager();

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toSimpleString());
    field.setCheckType(ObjectLookupCheckType::inList);

    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}})));

    auto list = manager->lookupList(listId);
    list.entries.push_back({{"foo", "2"}});
    manager->addOrUpdate(list);
    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}})));

    manager->remove(listId);
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
}

TEST_F(ObjectLookupFieldTest, lookupListIsReloadedOnManagerInitialization)
{
    api::LookupListData list;
    list.id = nx::Uuid::createUuid();
    list.attributeNames = {"foo"};
    list.entries = {{{"foo", "1"}}};
    list.objectTypeId = "nx.test.type";

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(list.id.toSimpleString());
    field.setCheckType(ObjectLookupCheckType::inList);

    // The lists aren't loaded yet.
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));

    systemContext()->lookupListManager()->initialize({list});
    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
}

TEST_F(ObjectLookupFieldTest, lookupListIsDroppedOnManagerDeinitialization)
{
    const auto listId = makeAndRegisterList({"foo"}, {{{"foo", "1"}}}, /* isGeneric */ false);

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toSimpleString());
    field.setCheckType(ObjectLookupCheckType::inList);
    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));

    systemContext()->lookupListManager()->deinitialize();
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
}

TEST_F(ObjectLookupFieldTest, notInListLookupListChangesAreApplied)
{
    const auto listId = makeAndRegisterList({"foo"}, {{{"foo", "1"}}}, /* isGeneric */ false);
    const auto manager = systemContext()->lookupListManager();

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toSimpleString());
    field.setCheckType(ObjectLookupCheckType::notInList);
    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}})));

    auto list = manager->lookupList(listId);
    list.entries.push_back({{"foo", "2"}});
    manager->addOrUpdate(list);
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}})));
}

TEST_F(ObjectLookupFieldTest, lookupListAddedAfterMatch)
{
    makeAndRegisterList({"foo"}, {{{"foo", "1"}}}, /* isGeneric */ false);

    api::LookupListData list;
    list.id = nx::Uuid::createUuid();
    list.attributeNames = {"foo"};
    list.entries = {{{"foo", "1"}}};
    list.objectTypeId = "nx.test.type";

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(list.id.toSimpleString());
    field.setCheckType(ObjectLookupCheckType::inList);

    // The list doesn't exist yet.
    EXPECT_FALSE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));

    systemContext()->lookupListManager()->addOrUpdate(list);
    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "1"}})));
}

// The Server matches analytics object events in several threads at once.
TEST_F(ObjectLookupFieldTest, concurrentMatchWhileLookupListChanges)
{
    const auto listId = makeAndRegisterList({"foo"}, {{{"foo", "1"}}}, /* isGeneric */ false);
    const auto manager = systemContext()->lookupListManager();

    ObjectLookupField field{systemContext(), &kDummyDescriptor};
    field.setValue(listId.toSimpleString());
    field.setCheckType(ObjectLookupCheckType::inList);

    constexpr int kThreadCount = 4;
    constexpr int kChangeCount = 500;
    const auto matchingValue = QVariant::fromValue(Attributes{{"foo", "1"}});
    std::atomic<int> matchCount = 0;
    std::atomic<int> failedMatchCount = 0;
    std::atomic<bool> stop = false;

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i)
    {
        threads.emplace_back(
            [&]
            {
                while (!stop)
                {
                    if (!field.match(matchingValue))
                        ++failedMatchCount;
                    ++matchCount;
                }
            });
    }

    // Toggle an entry and let the threads match after every change.
    auto list = manager->lookupList(listId);
    const auto initialEntries = list.entries;
    for (int i = 0; i < kChangeCount; ++i)
    {
        list.entries = initialEntries;
        if (i % 2 == 1)
            list.entries.push_back({{"foo", "2"}});
        manager->addOrUpdate(list);

        const int expectedMatchCount = matchCount + kThreadCount;
        while (matchCount < expectedMatchCount)
            std::this_thread::yield();
    }

    stop = true;
    for (auto& thread: threads)
        thread.join();

    EXPECT_EQ(failedMatchCount.load(), 0);
    EXPECT_TRUE(field.match(QVariant::fromValue(Attributes{{"foo", "2"}})));
}

} // namespace nx::vms::rules::test
