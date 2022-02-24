// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_set>
#include <unordered_map>
#include <optional>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/abstract_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {
namespace test {

class TestItemPool
{
public:
    TestItemPool();
    ~TestItemPool();

    std::unordered_set<int> usedRoles() const;
    void setUsedRoles(const std::unordered_set<int>& roles);
    void setKeyDataRole(int role);

    void setItemNameGenerator(std::function<QString()> nameGenerator);
    void setItemName(int itemKey, const QString& name);

    QVariant itemData(int itemKey, int role);
    void setItemData(int itemKey, int role, const QVariant& data);

    std::function<AbstractItemPtr(int)> itemCreator();

private:
    struct ItemData
    {
        struct RoleData
        {
            QVariant value;
            GenericItem::DataProvider dataProvider;
            InvalidatorPtr invalidator;
        };
        std::unordered_map<int, RoleData> roleDataByRole;
    };

    std::unordered_set<int> m_usedRoles;
    std::optional<int> m_keyDataRole;
    std::function<QString()> m_nameGenerator;
    std::unordered_map<int, ItemData> m_itemDataByKey;
};

} // namespace test
} // namespace entity_item_model
} // namespace nx::vms::client::desktop
