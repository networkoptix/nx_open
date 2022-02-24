// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_item_pool.h"

namespace nx::vms::client::desktop {
namespace entity_item_model {
namespace test {

TestItemPool::TestItemPool():
    m_usedRoles({Qt::DisplayRole}),
    m_nameGenerator([]{ return "Item"; })
{
}

TestItemPool::~TestItemPool()
{
}

std::unordered_set<int> TestItemPool::usedRoles() const
{
    return m_usedRoles;
}

void TestItemPool::setUsedRoles(const std::unordered_set<int>& roles)
{
    m_usedRoles = roles;
}

void TestItemPool::setKeyDataRole(int role)
{
    m_keyDataRole = role;
}

void TestItemPool::setItemNameGenerator(std::function<QString()> nameGenerator)
{
    m_nameGenerator = nameGenerator;
}

void TestItemPool::setItemName(int itemKey, const QString& name)
{
    setItemData(itemKey, Qt::DisplayRole, QVariant(name));
}

QVariant TestItemPool::itemData(int itemKey, int role)
{
    auto itemItr = m_itemDataByKey.find(itemKey);
    if (itemItr == std::end(m_itemDataByKey))
        return QVariant();

    auto& roleDataMap = itemItr->second.roleDataByRole;
    auto roleItr = roleDataMap.find(role);
    if (roleItr == std::end(roleDataMap))
        return QVariant();

    return roleItr->second.value;
}

void TestItemPool::setItemData(int itemKey, int role, const QVariant& data)
{
    auto& itemData = m_itemDataByKey[itemKey];
    auto& roleDataMap = itemData.roleDataByRole;
    auto& roleData = roleDataMap[role];

    if (roleData.value == data)
        return;

    roleData.value = data;
    if (roleData.invalidator)
        roleData.invalidator->invalidate();
}

std::function<AbstractItemPtr(int)> TestItemPool::itemCreator()
{
    return [this](int itemKey) -> AbstractItemPtr
        {
            auto& itemDataStorage = m_itemDataByKey[itemKey];
            for (const auto& role: m_usedRoles)
            {
                if (m_keyDataRole && *m_keyDataRole == role)
                    continue;

                auto& roleData = itemDataStorage.roleDataByRole[role];
                if (roleData.value.isNull() && role == Qt::DisplayRole && m_nameGenerator)
                    roleData.value = m_nameGenerator();

                roleData.dataProvider = [this, itemKey, role] { return itemData(itemKey, role); };
                roleData.invalidator = std::make_shared<Invalidator>();
            }

            auto itemBuilder = GenericItemBuilder();
            auto& roleDataMap = itemDataStorage.roleDataByRole;
            for (auto roleItr = roleDataMap.cbegin(); roleItr != roleDataMap.cend(); ++roleItr)
            {
                itemBuilder.withRole(
                    roleItr->first,
                    roleItr->second.dataProvider,
                    roleItr->second.invalidator);
            }

            if (m_keyDataRole)
                itemBuilder.withRole(*m_keyDataRole, itemKey);

            itemBuilder.withFlags(Qt::ItemIsEnabled);

            return itemBuilder;
        };
}

} // namespace test
} // namespace entity_item_model
} // namespace nx::vms::client::desktop
