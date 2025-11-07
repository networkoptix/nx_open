// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop::entity_resource_tree {

class WebPageDecorator:
    public QObject,
    public core::entity_item_model::AbstractItem
{
public:
    WebPageDecorator(
        core::entity_item_model::AbstractItemPtr sourceItem,
        bool hasPowerUserPermissions = false);

    virtual QVariant data(int role) const override;
    virtual Qt::ItemFlags flags() const override;

private:
    QnWebPageResourcePtr getWebPage() const;

private:
    core::entity_item_model::AbstractItemPtr m_sourceItem;
    bool m_hasPowerUserPermissions = false;
};

} // namespace nx::vms::client::desktop::entity_resource_tree
