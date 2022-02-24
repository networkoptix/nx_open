// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_entity.h"
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

/**
 * Entity which wraps single item. Item-based lists designed to be fast, but they are not
 * really convenient for small applications with heterogeneous data. This class may be used
 * as adapter which allows to add single item to composition entity. Common applications:
 * - Composition with few single item entities as static header.
 * - Be placeholder nested entity showing "Loading..." for group with deferred contents loading.
 */
class NX_VMS_CLIENT_DESKTOP_API SingleItemEntity: public AbstractEntity
{
    using base_type = AbstractEntity;

public:
    /**
     * Constructor
     * @param  item Unique pointer to an item. Entity takes ownership of the item, please
     *     take in count: item will be moved and become null after constructor call. Passing
     *     null item as argument is safe, but result will be futile.
     */
    SingleItemEntity(AbstractItemPtr item);

    /**
     * Destructor. Generates remove rows notification if it's already has been mapped to
     * some model. So, you just erase entity to remove its representation from model.
     */
    virtual ~SingleItemEntity() override;

    /**
     * Implements AbstractEntity::rowCount().
     * @returns 1. Even if holding null item.
     */
    virtual int rowCount() const override;

    /**
     * Implements AbstractEntity::childEntity(). No checks applied, result is predefined.
     * @returns nullptr.
     */
    virtual AbstractEntity* childEntity(int row) const override;

    /**
     * Implements AbstractEntity::childEntityRow(). No checks applied, result is predefined.
     * @returns -1.
     */
    virtual int childEntityRow(const AbstractEntity* entity) const override;

    /**
     * Implements AbstractEntity()::data().
     * @param row Expected to be zero, other value will be considered as invalid input.
     * @param role Data role.
     * @returns Data provided by stored item by given role or default constructed QVariant if
     *     stored item is null or row isn't 0;
     */
    virtual QVariant data(int row, int role = Qt::DisplayRole) const override;

    /**
     * Implements AbstractEntity()::flags().
     * @param row Expected to be zero, other value will be considered as invalid input.
     * @returns Flags provided by stored item or default constructed Qt::ItemFlags if stored
     *     item is null or row parameter isn't 0.
     */
    virtual Qt::ItemFlags flags(int row) const override;

    /**
     * Implements AbstractEntity()::isPlanar().
     * @returns True.
     */
    virtual bool isPlanar() const override;

private:
    AbstractItemPtr m_item;
};

using SingleItemEntityPtr = std::unique_ptr<SingleItemEntity>;

/**
 * Convenient non-member factory function.
 */
NX_VMS_CLIENT_DESKTOP_API SingleItemEntityPtr makeSingleItemEntity(AbstractItemPtr item);

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
