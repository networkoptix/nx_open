// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <QtCore/QVector>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/abstract_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

/**
 * Main trait of GroupEntity is constant row count, you may always rely that it's 1. It's
 * necessary for fast offset calculation since it mainly used as sub entity of
 * UniqueKeyGroupList.
 */
class NX_VMS_CLIENT_DESKTOP_API GroupEntity: public AbstractEntity
{
    using base_type = AbstractEntity;

public:
    /**
     * Constructor. Makes group entity with head item only, suitable when nested entity
     * can't be created by the moment of group instantiation and will be installed later.
     * @param headItem Any item, ownership of item will be transferred to the entity. Null input
     *     is inacceptable and will lead to creation of dummy head item.
     */
    GroupEntity(AbstractItemPtr headItem);

    /**
     * Constructor. Composes group entity from the given head item and nested entity.
     * @param headItem Any valid item. Null input is inacceptable and will to creation of
     *     dummy head item. Group takes ownership of the head item.
     * @param nestedEntity Any valid entity, null one is either acceptable. Group takes
     *     ownership of the nested entity.
     */
    GroupEntity(AbstractItemPtr headItem, AbstractEntityPtr nestedEntity);

    /**
     * Constructor.
     * @param headItem Any valid item. Null input is inacceptable and will to creation of
     *     dummy head item. Group takes ownership of the head item.
     * @param nestedEntityCreator Factory function which creates nested entity at the moment of
     *     instantiation, and later, when data change notification for given role will be received
     *     from the head item.
     * @param discardingRoles Data roles which cause nested entity rebuild.
     */
    using EntityCreator = std::function<AbstractEntityPtr()>;
    GroupEntity(AbstractItemPtr headItem,
        const EntityCreator& nestedEntityCreator,
        const QVector<int>& discardingRoles);

    /**
     * Destructor. Generates remove notifications if mapped to model.
     */
    virtual ~GroupEntity() override;

    /**
     * Sets nested entity to the group. If there was another one, it will be discarded.
     * Corresponding notifications will be generated if group has already to some model.
     * @param  entity Valid unique pointer to the the entity. Ownership will be transferred to
     *     the group. Null input is acceptable, it will make group childless.
     */
    void setNestedEntity(AbstractEntityPtr entity);

    /**
     * Implements AbstractEntity::rowCount().
     * @returns 1
     */
    virtual int rowCount() const override;

    /**
     * Implements AbstractEntity::childEntity().
     * @returns pointer to the nested entity, if such is set, nullptr otherwise.
     */
    virtual AbstractEntity* childEntity(int row) const override;

    /**
     * Implements AbstractEntity::childEntityRow().
     * @returns 0 if nested entity is set and equals to the parameter, -1 in any other case.
     */
    virtual int childEntityRow(const AbstractEntity* entity) const override;

    /**
     * Implements AbstractEntity()::data().
     * @returns Data provided by the head item for row 0. QVariant() otherwise.
     *
     */
    virtual QVariant data(int row, int role = Qt::DisplayRole) const override;

    /**
     * Implements AbstractEntity()::flags().
     * @returns Flags provided by the head item for row 0. Qt::ItemFlags() otherwise.
     */
    virtual Qt::ItemFlags flags(int row) const override;

    /**
     * Implements AbstractEntity()::isPlanar().
     * @returns false.
     */
    virtual bool isPlanar() const override;

    const AbstractItem* headItem() const;

    using DataChangedCallback = std::function<void(const GroupEntity*, const QVector<int>&)>;

    void setDataChangedCallback(const DataChangedCallback& callback);

private:
    void setupHeadItemNotifications();

private:
    AbstractItemPtr m_headItem;
    AbstractEntityPtr m_nestedEntity;
    QVector<int> m_discardingRoles;
    EntityCreator m_nestedEntityCreator;
    DataChangedCallback m_callback;
};

using GroupEntityPtr = std::unique_ptr<GroupEntity>;

/**
 * Convenient non-member factory function for group list.
 */
NX_VMS_CLIENT_DESKTOP_API GroupEntityPtr makeGroup(AbstractItemPtr headItem);

/**
 * Convenient non-member factory function.
 */
NX_VMS_CLIENT_DESKTOP_API GroupEntityPtr makeGroup(
    AbstractItemPtr headItem,
    AbstractEntityPtr nestedEntity);

/**
 * Convenient non-member factory function.
 */
NX_VMS_CLIENT_DESKTOP_API GroupEntityPtr makeGroup(
    AbstractItemPtr headItem,
    const GroupEntity::EntityCreator& nestedEntityCreator,
    const QVector<int>& discardingRoles);

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
