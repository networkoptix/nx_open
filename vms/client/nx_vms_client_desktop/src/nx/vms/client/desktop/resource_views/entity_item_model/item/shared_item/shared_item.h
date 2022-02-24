// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/shared_item/shared_item_origin.h>
#include <set>

namespace nx::vms::client::desktop {
namespace entity_item_model {

class SharedItem: public AbstractItem
{
    using base_type = AbstractItem;
    friend SharedItemOrigin;

public:
    SharedItem(const std::shared_ptr<SharedItemOrigin>& sharedDataHolder);
    SharedItem(const SharedItem&) = delete;
    SharedItem& operator=(const SharedItem&) = delete;
    virtual ~SharedItem() override;

    virtual QVariant data(int role) const override;
    virtual Qt::ItemFlags flags() const override;

private:
    void notifyDataChanged(const QVector<int>& roles) const;

private:
    std::weak_ptr<SharedItemOrigin> m_sharedDataHolder;
};

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
