// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/Qt>
#include <QtCore/QVariant>

#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

/**
 * Helper class which translate some signal to only signal invalidate().
 */
class NX_VMS_CLIENT_DESKTOP_API Invalidator
{
    friend class GenericItemBuilder;

public:
    void invalidate();
    nx::utils::ScopedConnections* connections();

private:
    void addCallback(const std::function<void()>& callback);

    nx::utils::ScopedConnections m_connectionsGuard;
    std::vector<std::function<void()>> m_callbacks;
};

using InvalidatorPtr = std::shared_ptr<Invalidator>;

/**
 * General purpose Item, which just may hold static data for any role, or constructed with
 * data accessor function wrapper.
 */
class GenericItem: public AbstractItem
{
    using base_type = AbstractItem;
    friend class GenericItemBuilder;

public:
    using DataProvider = std::function<QVariant()>;
    using FlagsProvider = std::function<Qt::ItemFlags()>;

    GenericItem();

    virtual QVariant data(int role) const override;
    virtual Qt::ItemFlags flags() const override;

    void invalidateRole(int role);

private:
    struct RoleData
    {
        RoleData(int role, QVariant data);
        RoleData(int role, const DataProvider& dataProvider, const InvalidatorPtr& invalidator);

        int m_role = 0;
        mutable QVariant m_data;
        DataProvider m_dataProvider;
        InvalidatorPtr m_invalidator;

        bool operator< (const RoleData& other) const;
        bool operator== (const RoleData& other) const;
    };
    std::vector<RoleData> m_roleDataProviders;

    FlagsProvider m_flagsProvider;
};

using GenericItemPtr = std::unique_ptr<GenericItem>;

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
