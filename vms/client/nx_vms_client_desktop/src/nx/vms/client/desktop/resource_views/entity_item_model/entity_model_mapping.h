// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QModelIndex>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_notification_interface.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

class EntityItemModel;
class BaseNotificatonObserver;

/**
 * While AbstractEntity derivatives are responsible of actual data and have only parent to
 * child relations, i.e entity owner is not exposed to the entity itself, this class
 * encapsulates reverse data flow, consists of notifications and manages all translations
 * to the QAbstractItemModel interface.
 */
class NX_VMS_CLIENT_DESKTOP_API EntityModelMapping: public IEntityNotification
{
    friend class AbstractEntity;

public:
    virtual ~EntityModelMapping() override;

    /**
     * Maps owner entity to the root level of the model.
     */
    bool setEntityModel(EntityItemModel* model);

    /**
     * @returns Pointer to the attached model.
     */
    EntityItemModel* entityItemModel() const;

    QModelIndex parentModelIndex() const;

    QModelIndex entityModelIndex(int row) const;

private:
    EntityModelMapping(AbstractEntity* entity);

    /**
     * @returns Pointer to the owner entity.
     */
    AbstractEntity* ownerEntity() const;

    /**
     * @returns Pointer to the parent mapping object. It doesn't mean that parent is on the
     *     higher level, it might be a planar composite as well.
     */
    EntityModelMapping* parentMapping() const;

    /**
     * If owning entity is claimed as sub entity, that's mean that it's part of same level
     * entity composition, moreover, parent mapping may be claimed as sub entity mapping as well.
     * Widest planar mapping is the nearest mapping upstream which not represent some sub entity.
     * Basically, head mapping and child mappings are always widest planar mappings, while mappings
     * of sub entities are always not.
     * @returns Pointer to the same level non-sub entity mapping. It may be pointer to this mapping
     *     as well.
     */
    const EntityModelMapping* widestPlanarMapping() const;

    /**
     * @returns Row index in widest planar mapping owner entity coordinate system which corresponds
     *     to the zero row index of this mapping owner entity. Makes sense only for entities
     *     claimed as sub entities. For other types it will be always 0.
     */
    int entityOffset() const;

    /**
     * @returns True if this mapping have no parent.
     */
    bool isHeadMapping() const;

    /**
     * Implementation of AbstractEntity::assignAsChildEntity().
     */
    void assignAsChildEntity(EntityModelMapping* parent);
    bool isChildEntityMapping() const;

    /**
     * Implementation of AbstractEntity::assignAsSubEntity().
     */
    using OffsetEvaluator = std::function<int(const AbstractEntity*)>;
    void assignAsSubEntity(EntityModelMapping* parent, OffsetEvaluator offsetEvaluator);
    bool isSubEntityMapping() const;
    OffsetEvaluator offsetEvaluator() const;

    /**
     * Implementation of AbstractEntity::unassignEntity().
     */
    void unassignEntity();

    /**
     * Implementation of AbstractEntity::setNotificationObserver().
     */
    void setNotificationObserver(std::unique_ptr<BaseNotificatonObserver> notificationObserver);

    /**
     * Internal notification observer getter.
     */
    BaseNotificatonObserver* notificationObserver();

public:
    /**
     * IEntityNotification interface implementation which should receive consistent notifications
     * regarding changes in this entity. If there is entity model attached, it will receive this
     * notifications retranslated to the model coordinate system.
     */

    /**
     * dataChanged() should be called whenever the data in existing items changed.
     * @param first Index of the first changed item in chain.
     * @param last Index of the first changed item in chain.
     * @param roles Set of data roles that announced as changed. If no roles specified, that
     *     will mean that all data provided by items are invalidated.
     */
    virtual void dataChanged(int first, int last, const QVector<int>& roles) override;

    /**
     * beginInsertRows() should be called right before the actual item insertion done.
     * @param first Resulting index of the first item to be inserted.
     * @param last Resulting index of the last item to be inserted.
     */
    virtual void beginInsertRows(int first, int last) override;

    /**
     * Should follow beginInserRows call right after all changes will be done.
     */
    virtual void endInsertRows() override;

    /**
     * beginRemoveRows() should be called right before the actual item removal done.
     * @param first Index of the first item to be removed.
     * @param last Index of the last item to be removed.
     */
    virtual void beginRemoveRows(int first, int last) override;

    /**
     * Should follow beginRemoveRows() call right after all changes are done.
     */
    virtual void endRemoveRows() override;

    /**
     * beginMoveRows() should be called right before the actual moving is done.
     * @param source Valid pointer to the source entity.
     * @param start First index of item chain that will be moved in the source own coordinate
     *     system.
     * @param end Last index of item chain that will be moved in the source own coordinate
     *     system.
     * @param desination Valid pointer to the destination entity, may be same as source as well.
     * @param row Index before which items will be inserted in the destination own coordinate
     *     system.
     */
    virtual void beginMoveRows(
        const AbstractEntity* source, int start, int end,
        const AbstractEntity* desination, int row) override;

    /**
     * Should follow beginMoveRows() call right after all changes are done.
     */
    virtual void endMoveRows() override;

    /**
     * layoutAboutToBeChanged() should be called before items actually rearranged.
     * @param indexPermutation array sized exactly as notifying entity which contains all
     *     indexes in order they will appear after rearrange. That means that item N at
     *     index i will be at index indexPermutation[i] after rearrange.
     */
    virtual void layoutAboutToBeChanged(const std::vector<int>& indexPermutation) override;

    /**
     * Should follow layoutAboutToBeChanged() right after item rearrange is done.
     */
    virtual void layoutChanged() override;

private:
    AbstractEntity* m_entity;
    EntityModelMapping* m_parentMapping = nullptr;
    EntityItemModel* m_entityModel = nullptr;

    bool m_isChildEntityMapping = false;
    OffsetEvaluator m_offsetEvaluator;

    std::unique_ptr<BaseNotificatonObserver> m_notificationObserver;
};

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
