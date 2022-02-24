// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

/**
 * Builder class which helps to conveniently make unique generic item instance.
 */
class NX_VMS_CLIENT_DESKTOP_API GenericItemBuilder
{
    using DataProvider = GenericItem::DataProvider;
    using FlagsProvider = GenericItem::FlagsProvider;

public:
    explicit GenericItemBuilder();

    /**
     * @param role Data role for item being constructed, supposed to be used for access to
     *     the stored static data.
     * @param value Value which item should provide by given role, null QVariant is
     *     considered as invalid input. If it's needed to to return null QVariant, just do nothing.
     */
    GenericItemBuilder& withRole(int role, const QVariant& value);

    /**
     * @param role Data role for item being constructed, supposed to be used for access to
     *     the cached data value, provided by function wrapper passed as second parameter.
     * @param provider Function wrapper which return actual data value. Since it returns
     *     QVariant, expected type of provided value is responsibility of user.
     * @param invalidator Sole purpose of invalidator is notify that provided data has been
     *     changed, so, cached value will be discarded and DataChangedCallback instance will be
     *     invoked if one has been set.
     */
    GenericItemBuilder& withRole(int role, const DataProvider& provider,
        const InvalidatorPtr& invalidator);

    /**
     * @param flags For the item if it's expected that they will never change during item
     *     lifetime. If neither flags not flags provider will be specified, item will provide
     *     default flags set of {Qt::ItemIsEnabled, Qt::ItemIsSelectable}.
     */
    GenericItemBuilder& withFlags(Qt::ItemFlags flags);

    /**
     * @param  flagsProvider Function wrapper which returns actual flags for the item, will be
     *     invoked for each AbstractItem::flags() call.
     */
    GenericItemBuilder& withFlags(FlagsProvider flagsProvider);

    /**
     * Overloaded operator() which provides implicit conversion to the unique smart pointer
     * which holds constructed item, it's way the builder returns result. Typical usage
     * of GenericItemBuilder is daisy chaining data defining calls and return it as
     * AbstractItemPtr. It's not expected that operator() will be called more than once.
     * @returns Unique instance of item.
     */
    [[nodiscard]] operator AbstractItemPtr();

private:
    GenericItemPtr m_constructedItem;
};

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
