// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_item.h"

namespace {

auto kDefaultFlagsProvider = []() constexpr -> Qt::ItemFlags
    { return {Qt::ItemIsSelectable, Qt::ItemIsEnabled}; };

} // namespace

namespace nx::vms::client::desktop {
namespace entity_item_model {

void Invalidator::invalidate()
{
    for (const auto& callback: m_callbacks)
        callback();
}

nx::utils::ScopedConnections* Invalidator::connections()
{
    return &m_connectionsGuard;
}

void Invalidator::addCallback(const std::function<void()>& callback)
{
    m_callbacks.push_back(callback);
}

GenericItem::GenericItem():
    base_type(),
    m_flagsProvider(kDefaultFlagsProvider)
{
}

QVariant GenericItem::data(int role) const
{
    auto itr = std::lower_bound(std::cbegin(m_roleDataProviders), std::cend(m_roleDataProviders),
        RoleData(role, {}));

    if (itr == std::cend(m_roleDataProviders) || itr->m_role != role)
        return {};

    if (itr->m_dataProvider && itr->m_data.isNull())
        itr->m_data = itr->m_dataProvider();

    return itr->m_data;
}

Qt::ItemFlags GenericItem::flags() const
{
    return m_flagsProvider();
}

void GenericItem::invalidateRole(int role)
{
    auto itr = std::lower_bound(std::cbegin(m_roleDataProviders), std::cend(m_roleDataProviders),
        RoleData(role, {}));

    if (itr == std::cend(m_roleDataProviders) || itr->m_role != role)
        return;

    if (!itr->m_data.isNull())
        itr->m_data.clear();

    notifyDataChanged({role});
}

GenericItem::RoleData::RoleData(int role, QVariant value):
    m_role(role),
    m_data(value)
{
}

GenericItem::RoleData::RoleData(
    int role,
    const DataProvider& dataProvider,
    const InvalidatorPtr& invalidator)
    :
    m_role(role),
    m_dataProvider(dataProvider),
    m_invalidator(invalidator)
{
};

bool GenericItem::RoleData::operator<(const RoleData& other) const
{
    return m_role < other.m_role;
}

bool GenericItem::RoleData::operator==(const RoleData& other) const
{
    return m_role == other.m_role;
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
