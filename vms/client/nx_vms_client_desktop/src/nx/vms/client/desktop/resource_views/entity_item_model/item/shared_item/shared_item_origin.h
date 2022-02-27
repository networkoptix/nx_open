// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <unordered_set>

#include <QtCore/QVector>

#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

class SharedItem;

class SharedItemOrigin: public std::enable_shared_from_this<SharedItemOrigin>
{
    friend class SharedItem;
public:
    SharedItemOrigin(std::unique_ptr<AbstractItem> sourceItemData);
    SharedItemOrigin(const SharedItemOrigin&) = delete;
    SharedItemOrigin& operator=(const SharedItemOrigin&) = delete;
    ~SharedItemOrigin();

    std::unique_ptr<AbstractItem> createSharedInstance();

    using SharedInstanceCountObserver = std::function<void(int)>;
    void setSharedInstanceCountObserver(SharedInstanceCountObserver observer);

    QVariant data(int role) const;
    Qt::ItemFlags flags() const;

private:
    void onSourceDataChanged(const QVector<int>& roles);

private:
    std::unordered_set<SharedItem*> m_sharedInstances;
    std::unique_ptr<AbstractItem> m_sourceItemData;
    SharedInstanceCountObserver m_sharedInstanceCountObserver;
};

using SharedItemOriginPtr = std::shared_ptr<SharedItemOrigin>;

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
