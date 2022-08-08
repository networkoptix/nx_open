// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVariant>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

/**
 * Item for displaying loader or actual status of the Cloud System in terms of Cross-System access.
 * Works as flattening entity root, so visible only when cameras list is loading or when some error
 * occurred.
 */
class CloudSystemStatusItem: public entity_item_model::AbstractItem
{
    using base_type = entity_item_model::AbstractItem;

public:
    explicit CloudSystemStatusItem(const QString& systemId);
    virtual ~CloudSystemStatusItem() override;

    virtual QVariant data(int role) const override;
    virtual Qt::ItemFlags flags() const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
