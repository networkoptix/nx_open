// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHashFunctions>
#include <QtCore/QString>

#include <core/resource/shared_resource_pointer.h>
#include <nx/utils/string/comparator.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/abstract_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

template <class Key>
using KeyToItemTransform = std::function<AbstractItemPtr(const Key&)>;

template <class Key>
using KeyToEntityTransform = std::function<AbstractEntityPtr(const Key&)>;

template <class Key>
struct Hasher
{
    std::size_t operator()(const Key& key) const
    {
        return ::qHash(key);
    }
};

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
