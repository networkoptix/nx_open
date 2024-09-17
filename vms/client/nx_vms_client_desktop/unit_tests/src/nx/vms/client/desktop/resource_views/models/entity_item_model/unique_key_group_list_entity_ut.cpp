// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QCollator>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_group_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>

#include "entity_item_model_test_fixture.h"
#include "entity_logging_notification_listener.h"
#include "test_item_pool.h"

namespace {

using namespace nx::vms::client::desktop::entity_item_model;

QStringList getItemNames(const AbstractEntity* entity)
{
    QStringList items;
    for (int i = 0; i < entity->rowCount(); ++i)
        items.push_back(entity->data(i, Qt::DisplayRole).toString());
    return items;
}

std::function<AbstractEntityPtr(int)> nullNestedEntityCreator()
{
    return [](int) { return AbstractEntityPtr(); };
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_item_model {
namespace test {

TEST_F(EntityItemModelTest, groupListRemainSorted)
{
    static constexpr int kListSize = 50;
    static constexpr int kModificationRounds = 150;

    TestItemPool itemPool;
    itemPool.setItemNameGenerator(randomNameGenerator("Item", 0, 0));

    // Generate unique keys.
    QVector<int> keys;
    std::generate_n(std::back_inserter(keys), kListSize, sequentalIntegerGenerator());

    // Fill list with 50 items with same name.
    auto sortedList = std::make_unique<UniqueKeyGroupListEntity<int>>(
        itemPool.itemCreator(), nullNestedEntityCreator(), numericOrder());

    sortedList->setItems(keys);

    auto itemNames = getItemNames(sortedList.get());
    itemNames.removeDuplicates();
    ASSERT_TRUE(itemNames.size() == 1);

    // Rename items randomly, check if list remains sorted.
    auto nameGenerator = randomNameGenerator("Item", -500, 500);
    auto keyGenerator = randomIntegerGenerator(0, kListSize - 1);
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    auto pred =
        [&collator](const auto& lhs, const auto& rhs) { return collator.compare(lhs, rhs) < 0; };

    for (int i = 0; i < kModificationRounds; ++i)
    {
        auto itemNames = getItemNames(sortedList.get());
        itemPool.setItemName(keyGenerator(), nameGenerator());
        ASSERT_TRUE(std::is_sorted(std::begin(itemNames), std::end(itemNames), pred));
    }

    // Check if list actually contains different named items.
    itemNames = getItemNames(sortedList.get());
    itemNames.removeDuplicates();
    auto uniqueNamesCount = itemNames.size();
    EXPECT_GT(uniqueNamesCount, kListSize / 5);
}

// Disabled since it doesn't test something particular, more like poor man's benchmark.
TEST_F(EntityItemModelTest, DISABLED_modelGroupWithNoOrderAddRenameRemove20000TimesEach)
{
    static const auto kItemsSize = 20000;

    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item"));

    auto unsortedList = std::make_unique<UniqueKeyGroupListEntity<int>>(
        itemPool.itemCreator(), nullNestedEntityCreator(), noOrder());

    auto entityModel = std::make_unique<EntityItemModel>();
    entityModel->setRootEntity(unsortedList.get());

    QVector<int> keys;
    std::generate_n(std::back_inserter(keys), kItemsSize, sequentalIntegerGenerator(0));

    for (int i = 0; i < kItemsSize; ++i)
        unsortedList->addItem(keys.at(i));

    auto renamedNameGenerator = randomNameGenerator("RenamedItem", 0, 1000000);
    for (int i = 0; i < kItemsSize; ++i)
        itemPool.setItemName(i, renamedNameGenerator());

    for (int i = 0; i < kItemsSize; ++i)
        unsortedList->removeItem(keys.at(i));
}

TEST_F(EntityItemModelTest, groupListcorrectMoveNotificationsForListOfTwoItems)
{
    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item", 10));

    auto sortedList = std::make_unique<UniqueKeyGroupListEntity<int>>(
        itemPool.itemCreator(), nullNestedEntityCreator(), numericOrder());

    sortedList->setItems({0, 1});

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    sortedList->setNotificationObserver(std::move(notificationRecorder));

    // 1. First goes to back.
    itemPool.setItemName(0, "Item_99");

    // 2. First reverted.
    itemPool.setItemName(0, "Item_10");

    // 3. Second goes to front.
    itemPool.setItemName(1, "Item_0");

    // 4. Second reverted.
    itemPool.setItemName(1, "Item_11");

    SCOPED_TRACE("");

    using Record = NotificationRecord;

    queueChecker.withAnnotation("1. First goes to back")
        .verifyNotification(Record::dataChanged(0, 0, {Qt::DisplayRole}))
        .verifyNotification(Record::beginMoveRows(sortedList.get(), 0, 0, sortedList.get(), 2))
        .withClosing();

    queueChecker.withAnnotation("2. First reverted")
        .verifyNotification(Record::dataChanged(1, 1, {Qt::DisplayRole}))
        .verifyNotification(Record::beginMoveRows(sortedList.get(), 1, 1, sortedList.get(), 0))
        .withClosing();

    queueChecker.withAnnotation("3. Second goes to front")
        .verifyNotification(Record::dataChanged(1, 1, {Qt::DisplayRole}))
        .verifyNotification(Record::beginMoveRows(sortedList.get(), 1, 1, sortedList.get(), 0))
        .withClosing();

    queueChecker.withAnnotation("4. Second reverted")
        .verifyNotification(Record::dataChanged(0, 0, {Qt::DisplayRole}))
        .verifyNotification(Record::beginMoveRows(sortedList.get(), 0, 0, sortedList.get(), 2))
        .withClosing()
        .verifyIsEmpty();
}

// Disabled since it doesn't test something particular, more like poor man's benchmark.
TEST_F(EntityItemModelTest, DISABLED_modelGroupWithNumericOrderAddRenameRemove20000TimesEach)
{
    static const auto kItemsSize = 20000;

    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item"));

    auto sortedList = std::make_unique<UniqueKeyGroupListEntity<int>>(
        itemPool.itemCreator(), nullNestedEntityCreator(), numericOrder());

    auto entityModel = std::make_unique<EntityItemModel>();
    entityModel->setRootEntity(sortedList.get());
    QVector<int> keys;
    std::generate_n(std::back_inserter(keys), kItemsSize, sequentalIntegerGenerator(0));

    for (int i = 0; i < kItemsSize; ++i)
        sortedList->addItem(keys.at(i));

    auto renamedNameGenerator = randomNameGenerator("RenamedItem", 0, 1000000);
    for (int i = 0; i < kItemsSize; ++i)
        itemPool.setItemName(i, renamedNameGenerator());

    for (int i = 0; i < kItemsSize; ++i)
        sortedList->removeItem(keys.at(i));
}

// Disabled since it doesn't test something particular, more like poor man's benchmark.
TEST_F(EntityItemModelTest, DISABLED_modelGroupAddRemove20000TimesEachToggleSorting20Times)
{
    static const auto kItemsSize = 20000;
    static const auto kToggleSortingBackAndForth = 10;

    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item"));

    auto unsortedList = std::make_unique<UniqueKeyGroupListEntity<int>>(
        itemPool.itemCreator(), nullNestedEntityCreator(), noOrder());

    auto entityModel = std::make_unique<EntityItemModel>();
    entityModel->setRootEntity(unsortedList.get());

    QVector<int> keys;
    std::generate_n(std::back_inserter(keys), kItemsSize, sequentalIntegerGenerator(0));

    for (int i = 0; i < kItemsSize; ++i)
        unsortedList->addItem(keys.at(i));

    for (int i = 0; i < kToggleSortingBackAndForth; ++i)
    {
        unsortedList->setItemOrder(numericOrder());
        unsortedList->setItemOrder(noOrder());
    }
    for (int i = 0; i < kItemsSize; ++i)
        unsortedList->removeItem(keys.at(i));
}

} // namespace test
} // namespace entity_item_model
} // namespace nx::vms::client::desktop
