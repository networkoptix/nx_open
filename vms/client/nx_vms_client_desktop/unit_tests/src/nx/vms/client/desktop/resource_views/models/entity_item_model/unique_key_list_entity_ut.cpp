// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "entity_item_model_test_fixture.h"
#include "entity_logging_notification_listener.h"
#include "test_item_pool.h"

#include <QtCore/QCollator>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>
#include <iomanip>

namespace {

using namespace nx::vms::client::desktop::entity_item_model;

QStringList getItemNames(const AbstractEntity* entity)
{
    QStringList items;
    for (int i = 0; i < entity->rowCount(); ++i)
        items.push_back(entity->data(i, Qt::DisplayRole).toString());
    return items;
}

QString listToString(const AbstractEntity* entity)
{
    return QString("[%1]").arg(getItemNames(entity).join(", "));
}

const auto uuidItemCreator =
    [](const nx::Uuid& id) -> AbstractItemPtr
    {
        return GenericItemBuilder()
            .withRole(Qt::DisplayRole, QVariant::fromValue(id.toString()))
            .withFlags({Qt::ItemIsEnabled, Qt::ItemIsSelectable});
    };

const auto uuidNullItemCreator =
    [](const nx::Uuid&) -> AbstractItemPtr
    {
        return AbstractItemPtr();
    };

const auto intItemCreator =
    [](int id) -> AbstractItemPtr
    {
        return GenericItemBuilder()
            .withRole(Qt::DisplayRole, QVariant::fromValue(QString::number(id)))
            .withFlags({Qt::ItemIsEnabled, Qt::ItemIsSelectable});
    };

const auto resourcePtrItemCreator =
    [](const QnResourcePtr& resource) -> AbstractItemPtr
    {
        return GenericItemBuilder()
            .withRole(Qt::DisplayRole,
                QVariant::fromValue(resource ? resource->getName() : "Null Resource"))
            .withFlags({Qt::ItemIsEnabled, Qt::ItemIsSelectable});
    };

} // namespace

namespace nx::vms::client::desktop {
namespace entity_item_model {
namespace test {

TEST_F(EntityItemModelTest, nullUuidIsntValidKey)
{
    const auto nullUuid = nx::Uuid();
    EXPECT_TRUE(nullUuid.isNull());

    auto listWithUuudKey = makeKeyList<nx::Uuid>(uuidItemCreator, noOrder());

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    listWithUuudKey->setNotificationObserver(std::move(notificationRecorder));

    ASSERT_FALSE(listWithUuudKey->addItem(nullUuid));
    ASSERT_EQ(listWithUuudKey->rowCount(), 0);
    ASSERT_FALSE(listWithUuudKey->removeItem(nullUuid));
    queueChecker.verifyIsEmpty();
}

TEST_F(EntityItemModelTest, nullUuidsAreFilteredBySetKeysMethod)
{
    QVector<nx::Uuid> keys;
    keys.append(nx::Uuid());
    keys.append(nx::Uuid::createUuid());
    keys.append(nx::Uuid());
    keys.append(nx::Uuid::createUuid());

    auto listWithUuudKey = makeKeyList<nx::Uuid>(uuidItemCreator, noOrder());

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    listWithUuudKey->setNotificationObserver(std::move(notificationRecorder));

    listWithUuudKey->setItems(keys);

    using Record = NotificationRecord;
    queueChecker
        .verifyNotification(Record::beginInsertRows(0, 1)).withClosing()
        .verifyIsEmpty();
}

TEST_F(EntityItemModelTest, nothingAddedIfNullItemCreated)
{
    const auto uuid = nx::Uuid::createUuid();
    EXPECT_TRUE(!uuid.isNull());

    auto listWithUuudKey = makeKeyList<nx::Uuid>(uuidNullItemCreator, noOrder());

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    listWithUuudKey->setNotificationObserver(std::move(notificationRecorder));

    ASSERT_FALSE(listWithUuudKey->addItem(uuid));
    ASSERT_EQ(listWithUuudKey->rowCount(), 0);
    ASSERT_FALSE(listWithUuudKey->removeItem(uuid));
    queueChecker.verifyIsEmpty();
}

TEST_F(EntityItemModelTest, nullSmartPointerIsntValidKey)
{
    const auto nullPtr = QnResourcePtr();
    EXPECT_TRUE(nullPtr.isNull());

    auto listWithSmartPointerKey = makeKeyList<QnResourcePtr>(resourcePtrItemCreator, noOrder());

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    listWithSmartPointerKey->setNotificationObserver(std::move(notificationRecorder));

    ASSERT_FALSE(listWithSmartPointerKey->addItem(nullPtr));
    ASSERT_EQ(listWithSmartPointerKey->rowCount(), 0);
    ASSERT_FALSE(listWithSmartPointerKey->removeItem(nullPtr));
    queueChecker.verifyIsEmpty();
}

TEST_F(EntityItemModelTest, zeroIntegralIsValidKey)
{
    auto listWithIntKey = makeKeyList<int>(intItemCreator, noOrder());

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    listWithIntKey->setNotificationObserver(std::move(notificationRecorder));

    ASSERT_TRUE(listWithIntKey->addItem(0));
    ASSERT_EQ(listWithIntKey->rowCount(), 1);
    ASSERT_TRUE(listWithIntKey->removeItem(0));
    ASSERT_EQ(listWithIntKey->rowCount(), 0);

    using Record = NotificationRecord;
    queueChecker
        .verifyNotification(Record::beginInsertRows(0, 0)).withClosing()
        .verifyNotification(Record::beginRemoveRows(0, 0)).withClosing()
        .verifyIsEmpty();
}

TEST_F(EntityItemModelTest, secondAtemptToInsertItemWithSameKeyIsUnsuccessfull)
{
    const auto uuid = nx::Uuid::createUuid();
    EXPECT_FALSE(uuid.isNull());

    auto listWithUuudKey = makeKeyList<nx::Uuid>(uuidItemCreator, noOrder());

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    listWithUuudKey->setNotificationObserver(std::move(notificationRecorder));

    ASSERT_TRUE(listWithUuudKey->addItem(uuid));
    ASSERT_FALSE(listWithUuudKey->addItem(uuid));
    ASSERT_EQ(listWithUuudKey->rowCount(), 1);

    using Record = NotificationRecord;
    queueChecker
        .verifyNotification(Record::beginInsertRows(0, 0)).withClosing()
        .verifyIsEmpty();
}

TEST_F(EntityItemModelTest, secondAtemptToRemoveItemWithSameKeyIsUnsuccessfull)
{
    const auto uuid = nx::Uuid::createUuid();
    EXPECT_FALSE(uuid.isNull());

    auto listWithUuudKey = makeKeyList<nx::Uuid>(uuidItemCreator, noOrder());

    listWithUuudKey->addItem(uuid);

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    listWithUuudKey->setNotificationObserver(std::move(notificationRecorder));

    ASSERT_TRUE(listWithUuudKey->removeItem(uuid));
    ASSERT_FALSE(listWithUuudKey->removeItem(uuid));
    ASSERT_EQ(listWithUuudKey->rowCount(), 0);

    using Record = NotificationRecord;
    queueChecker
        .verifyNotification(Record::beginRemoveRows(0, 0))
        .verifyNotification(Record::endRemoveRows())
        .verifyIsEmpty();
}

TEST_F(EntityItemModelTest, reusingKeyIsValid)
{
    const auto uuid = nx::Uuid::createUuid();
    EXPECT_FALSE(uuid.isNull());

    auto listWithUuudKey = makeKeyList<nx::Uuid>(uuidItemCreator, noOrder());

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    listWithUuudKey->setNotificationObserver(std::move(notificationRecorder));

    ASSERT_TRUE(listWithUuudKey->addItem(uuid));
    ASSERT_TRUE(listWithUuudKey->removeItem(uuid));
    ASSERT_TRUE(listWithUuudKey->addItem(uuid));
    ASSERT_EQ(listWithUuudKey->rowCount(), 1);

    using Record = NotificationRecord;
    queueChecker
        .verifyNotification(Record::beginInsertRows(0, 0)).withClosing()
        .verifyNotification(Record::beginRemoveRows(0, 0)).withClosing()
        .verifyNotification(Record::beginInsertRows(0, 0)).withClosing()
        .verifyIsEmpty();
}

TEST_F(EntityItemModelTest, listRemainSorted)
{
    static constexpr int kListSize = 50;
    static constexpr int kModificationRounds = 150;

    TestItemPool itemPool;
    itemPool.setItemNameGenerator(randomNameGenerator("Item", 0, 0));

    // Generate unique keys.
    QVector<int> keys;
    std::generate_n(std::back_inserter(keys), kListSize, sequentalIntegerGenerator());

    // Fill list with 50 items with same name.
    auto sortedList = std::make_unique<UniqueKeyListEntity<int>>(
        itemPool.itemCreator(), numericOrder());

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

TEST_F(EntityItemModelTest, frontItemGoesToBackOfSortedList)
{
    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item"));

    auto sortedList = std::make_unique<UniqueKeyListEntity<int>>(
        itemPool.itemCreator(), numericOrder());

    sortedList->setItems({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    sortedList->setNotificationObserver(std::move(notificationRecorder));

    itemPool.setItemName(0, "Item_99");

    using Record = NotificationRecord;
    queueChecker
        .withAnnotation(toString(sortedList))
        .verifyNotification(Record::dataChanged(0, 0, {Qt::DisplayRole}))
        .verifyNotification(Record::beginMoveRows(sortedList.get(), 0, 0, sortedList.get(), 10))
        .verifyNotification(Record::endMoveRows())
        .verifyIsEmpty();
}

TEST_F(EntityItemModelTest, backItemGoesToFrontOfSortedList)
{
    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item", 10));

    auto sortedList = std::make_unique<UniqueKeyListEntity<int>>(
        itemPool.itemCreator(), numericOrder());

    sortedList->setItems({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    sortedList->setNotificationObserver(std::move(notificationRecorder));

    itemPool.setItemName(9, "Item_0");

    using Record = NotificationRecord;
    queueChecker
        .withAnnotation(listToString(sortedList.get()))
        .verifyNotification(Record::dataChanged(9, 9, {Qt::DisplayRole}))
        .verifyNotification(Record::beginMoveRows(sortedList.get(), 9, 9, sortedList.get(), 0))
        .verifyNotification(Record::endMoveRows())
        .verifyIsEmpty();
}

TEST_F(EntityItemModelTest, singleItemListDoentGenerateMoveNotifications)
{
    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item"));

    auto sortedList = std::make_unique<UniqueKeyListEntity<int>>(
        itemPool.itemCreator(), numericOrder());

    sortedList->setItems({0});

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    sortedList->setNotificationObserver(std::move(notificationRecorder));

    itemPool.setItemName(0, "Name");
    itemPool.setItemName(0, "It");
    itemPool.setItemName(0, "Whatever");
    itemPool.setItemName(0, "You");
    itemPool.setItemName(0, "Want");

    using Record = NotificationRecord;
    queueChecker
        .withAnnotation(listToString(sortedList.get()))
        .verifyNotification(Record::dataChanged(0, 0, {Qt::DisplayRole}))
        .verifyNotification(Record::dataChanged(0, 0, {Qt::DisplayRole}))
        .verifyNotification(Record::dataChanged(0, 0, {Qt::DisplayRole}))
        .verifyNotification(Record::dataChanged(0, 0, {Qt::DisplayRole}))
        .verifyNotification(Record::dataChanged(0, 0, {Qt::DisplayRole}))
        .verifyIsEmpty();
}

TEST_F(EntityItemModelTest, itemRenamedButStayedInPlace)
{
    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item"));

    auto sortedList = std::make_unique<UniqueKeyListEntity<int>>(itemPool.itemCreator(),
        numericOrder());

    sortedList->setItems({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    sortedList->setNotificationObserver(std::move(notificationRecorder));

    itemPool.setItemName(4, "Item_4.2"); //< Item position shouldn't change due alphanumeric sorting nature.

    using Record = NotificationRecord;
    queueChecker
        .withAnnotation(listToString(sortedList.get()))
        .verifyNotification(Record::dataChanged(4, 4, {Qt::DisplayRole}))
        .verifyIsEmpty();
}

TEST_F(EntityItemModelTest, permutationsAreSymmetric)
{
    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item", 10));

    auto sortedList = std::make_unique<UniqueKeyListEntity<int>>(
        itemPool.itemCreator(), numericOrder());

    sortedList->setItems({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    sortedList->setNotificationObserver(std::move(notificationRecorder));

    sortedList->setItemOrder(noOrder());
    sortedList->setItemOrder(numericOrder());
    sortedList->setItemOrder(numericOrder(Qt::DescendingOrder));
    sortedList->setItemOrder(noOrder());
}

TEST_F(EntityItemModelTest, correctMoveNotificationsForListOfTwoItems)
{
    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item", 10));

    auto sortedList = std::make_unique<UniqueKeyListEntity<int>>(
        itemPool.itemCreator(), numericOrder());

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
TEST_F(EntityItemModelTest, DISABLED_modelWithNoOrderAddRenameRemove20000TimesEach)
{
    static const auto kItemsSize = 20000;

    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item"));

    auto unsortedList = std::make_unique<UniqueKeyListEntity<int>>(
        itemPool.itemCreator(), noOrder());

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

// Disabled since it doesn't test something particular, more like poor man's benchmark.
TEST_F(EntityItemModelTest, DISABLED_modelWithNumericOrderAddRenameRemove20000TimesEach)
{
    static const auto kItemsSize = 20000;

    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item"));

    auto sortedList = std::make_unique<UniqueKeyListEntity<int>>(
        itemPool.itemCreator(), numericOrder());

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
TEST_F(EntityItemModelTest, DISABLED_modelAddRemove20000TimesEachToggleSorting20Times)
{
    static const auto kItemsSize = 20000;
    static const auto kToggleSortingBackAndForth = 10;

    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item"));

    auto unsortedList = std::make_unique<UniqueKeyListEntity<int>>(
        itemPool.itemCreator(), noOrder());

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
