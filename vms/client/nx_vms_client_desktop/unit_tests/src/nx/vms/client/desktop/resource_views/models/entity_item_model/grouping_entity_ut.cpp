// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "entity_item_model_test_fixture.h"

#include <cmath>

#include "entity_logging_notification_listener.h"
#include "test_item_pool.h"

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/grouping_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>

namespace {

using namespace nx::vms::client::desktop::entity_item_model;
using namespace nx::vms::client::desktop::entity_resource_tree;
using namespace nx::vms::client::desktop::entity_item_model::test;

static constexpr int kKeyRole = Qt::UserRole;

static constexpr auto kFirstGroupKeyRole = Qt::UserRole + 0xFF;

using GroupIdGetter = std::function<QString(const int&, int)>;
using ItemCreator = std::function<AbstractItemPtr(int)>;
using GroupItemCreator = std::function<AbstractItemPtr(const QString&)>;

GroupIdGetter groupIdGetter(TestItemPool* testItemPool, int groupKeyRole)
{
    return
        [testItemPool, groupKeyRole](const int& key, int /*order*/) -> QString
        {
            return testItemPool->itemData(key, groupKeyRole).toString();
        };
}

GroupIdGetter compositeGroupIdGetter(TestItemPool* testItemPool, int groupKeyRole)
{
    return
        [testItemPool, groupKeyRole](const int& key, int order) -> QString
        {
            QStringList subKeys;
            for (int i = 0; i < order; ++i)
            {
                const auto subKey = testItemPool->itemData(key, groupKeyRole).toString();
                if (!subKey.isEmpty())
                    subKeys.append(subKey);
            }
            return subKeys.join(resource_grouping::kSubIdDelimiter);
        };
}

GroupItemCreator groupItemCreator(int groupRole)
{
    return
        [groupRole](const QString& groupKey) -> AbstractItemPtr
        {
            return GenericItemBuilder()
                .withRole(Qt::DisplayRole, QVariant(groupKey))
                .withRole(groupRole, QVariant(groupKey));
        };
}

GroupItemCreator multiDimensionalGroupItemCreator(int groupRole)
{
    using namespace resource_grouping;
    return
        [groupRole](const QString& groupKey) -> AbstractItemPtr
        {
            const auto groupKeyDimension = compositeIdDimension(groupKey);
            return GenericItemBuilder()
                .withRole(Qt::DisplayRole, extractSubId(groupKey, groupKeyDimension - 1))
                .withRole(groupRole, QVariant(groupKey));
        };
}

int groupKeyRole(int groupingRuleIndex)
{
    return kFirstGroupKeyRole + groupingRuleIndex;
}

QVariant dataByRowIndexChain(
    AbstractEntity* entity,
    std::vector<int> rowIndexChain,
    int role)
{
    NX_ASSERT(rowIndexChain.size() > 0, "Invalid input parameter.");

    if (rowIndexChain.size() == 1)
        return entity->data(rowIndexChain.front(), role);

    return dataByRowIndexChain(
        entity->childEntity(rowIndexChain.front()),
        std::vector<int>(std::next(std::cbegin(rowIndexChain)), std::cend(rowIndexChain)),
        role);
}

QString displayDataByRowIndexChain(
    AbstractEntity* entity,
    std::vector<int> rowIndexChain)
{
    return dataByRowIndexChain(entity, rowIndexChain, Qt::DisplayRole).toString();
}

GroupingEntity<QString, int>::GroupingRuleStack createTestGroupingRuleStack(
    TestItemPool* testItemPool,
    int ruleCount)
{
    GroupingEntity<QString, int>::GroupingRuleStack result;
    for (int i = 0; i < ruleCount; ++i)
    {
        result.push_back(
            {groupIdGetter(testItemPool, groupKeyRole(i)),
            groupItemCreator(groupKeyRole(i)),
            groupKeyRole(i),
            1,
            numericOrder()});
    }
    return result;
}

GroupingEntity<QString, int>::GroupingRuleStack createCompositeTestGroupingRuleStack(
    TestItemPool* testItemPool,
    int ruleDimension)
{
    GroupingEntity<QString, int>::GroupingRuleStack result;
    result.push_back(
        {groupIdGetter(testItemPool, groupKeyRole(0)),
        multiDimensionalGroupItemCreator(groupKeyRole(0)),
        groupKeyRole(0),
        ruleDimension,
        numericOrder()});
    return result;
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_item_model {
namespace test {

using namespace entity_resource_tree;

TEST_F(EntityItemModelTest, groupingEntity_noRules_notifications)
{
    TestItemPool itemPool;
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item"));

    auto groupingEntity = std::make_unique<GroupingEntity<QString, int>>(
        itemPool.itemCreator(),
        kKeyRole,
        numericOrder(),
        createTestGroupingRuleStack(&itemPool, 0));

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    groupingEntity->setNotificationObserver(std::move(notificationRecorder));

    groupingEntity->setItems({0, 1, 2});

    using Record = NotificationRecord;
    queueChecker
        .verifyNotification(Record::beginInsertRows(0, 0)).withClosing()
        .verifyNotification(Record::beginInsertRows(1, 1)).withClosing()
        .verifyNotification(Record::beginInsertRows(2, 2)).withClosing()
        .verifyIsEmpty();
}

TEST_F(EntityItemModelTest, groupingEntity_singleRule_initialGroupingData_notifications)
{
    TestItemPool itemPool;
    itemPool.setUsedRoles({Qt::DisplayRole, kKeyRole, groupKeyRole(0)});
    itemPool.setKeyDataRole(kKeyRole);
    itemPool.setItemNameGenerator(sequentialNameGenerator("Item"));

    // Item '1': Non-null group ID data at the moment of item creation.
    itemPool.setItemData(1, groupKeyRole(0), "Group");

    auto groupingEntity = std::make_unique<GroupingEntity<QString, int>>(
        itemPool.itemCreator(),
        kKeyRole,
        numericOrder(),
        createTestGroupingRuleStack(&itemPool, 1));

    auto notificationRecorder = std::make_unique<EntityLoggingNotificationListener>();
    auto queueChecker = notificationRecorder->queueChecker();
    groupingEntity->setNotificationObserver(std::move(notificationRecorder));

    groupingEntity->setItems({0, 1, 2});

    using Record = NotificationRecord;
    queueChecker
        .verifyNotification(Record::beginInsertRows(0, 0)).withClosing()
        .verifyNotification(Record::beginInsertRows(0, 0)).withClosing()
        .verifyNotification(Record::beginInsertRows(2, 2)).withClosing()
        .verifyIsEmpty();
}

/**
 * Number of test cases is pending. Current approach considered ineffective, role of notification
 * observers are being revisited etc. Set of very few cases have been removed temporarily.
 */

}
} // namespace entity_item_model
} // namespace nx::vms::client::desktop
