// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QVariant>

class QAbstractItemModel;

namespace nx::vms::client::desktop {
namespace entity_item_model {

class EntityModelMapping;
class BaseNotificatonObserver;

/**
 * Base interface class for all entities. Serve the concept of aggregate data structure, made
 * by independent bits. Main goal was to achieve that class user did not take into account
 * whole structure. All notifications should be made in current entity coordinates, are mostly
 * planar, also it provides way to split whole model to parts and make separate
 * implementations. Which is testable and modular. It doesn't inherit QObject intentionally, it
 * will guarantee strict contracts and very deterministic data flow which won't be ruined with
 * ad hoc signal-slot connection.
 */
class NX_VMS_CLIENT_DESKTOP_API AbstractEntity
{
public:
    AbstractEntity();
    AbstractEntity(const AbstractEntity&) = delete;
    AbstractEntity& operator=(const AbstractEntity&) = delete;
    virtual ~AbstractEntity();

    /**
     * @returns Number of rows of planar part of this entity.
     */
    virtual int rowCount() const = 0;

    /**
     * @param row Valid row index.
     * @returns Returns reference to the child entity at given row if such exists, nullptr
     *     otherwise. Child entity is not necessarily owned by this entity, but always represents
     *     part of hierarchy which one step deeper.
     */
    virtual AbstractEntity* childEntity(int row) const = 0;

    /**
     * Base implementation of this method performs linear traverse and comparison of given
     * entity with one returned by childEntity() method. Except for entities which are declare
     * themselves as planar, -1 is explicitly returned then. Despite fact that this
     * implementation is sufficient for any application, for descent performance it should be
     * reimplemented in subclasses. O(log N) complexity or less is preferred.
     * @param entity Valid reference to entity.
     * @returns Index at which given entity is held as a child, -1 if there no such.
     */
    virtual int childEntityRow(const AbstractEntity* entity) const = 0;

    /**
     * Convenience method to get count of child items which using childEntity() and rowCount()
     * methods. Please pay attention that returned zero value doesn't mean absence of child
     * entity at given row, it may be just empty which is absolutely valid case.
     * @param row Valid row index.
     * @returns Count of deeper level items available at given row.
     */
    int childRowCount(int row) const;

    /**
     * Returns the data stored under the given role for the item referred to by the row index.
     * As you can see, this method copies one from QAbstractItemModel interface, except it
     * applicable for single level and one entity, which make things much simpler.
     * @param row Valid row index.
     * @param role Data role.
     * @returns Data stored under given role. If you do not have a value to return, return an
     *     invalid QVariant instead of returning 0.
     */
    virtual QVariant data(int row, int role) const = 0;

    /**
     * @param row Valid row index.
     * @returns Returns item flags for the given index.
     */
    virtual Qt::ItemFlags flags(int row) const = 0;

    /**
     * Query if this entity is pure planar, i.e it never have children, not only at the moment
     *     of method call. Implementing this method in subclasses makes sense performance wise.
     *     Planar entities implicitly provide Qt::ItemNeverHasChildren flag and ommited from
     *     any children entities lookups.
     * @returns True if entity cannot have children, or, at least won't have children during
     *     instance lifetime.
     */
    virtual bool isPlanar() const = 0;

    /**
     * Sets notification observer to this entity. It will receive notification, even in case if
     * this entity is subpart of some composition.
     * @param notificationObserver Unique instance of notification observer interface
     *      implementation, entity takes ownership of observer.
     */
    void setNotificationObserver(std::unique_ptr<BaseNotificatonObserver> notificationObserver);

    /**
     * Resets notification observer to the initial state.
     */
    void resetNotificationObserver();

    /**
     * @note This method will be removed very soon.
     */
    virtual void hide();

    /**
     * @note This method will be removed very soon.
     */
    virtual void show();

    /**
     * @todo Move method to the protected section.
     *
     * EntityModelMapping essentially is an isolated part of AbstractEntity implementation,
     * it have same lifetime as owner entity and manages relations between entities in terms
     * of update notifications propagation and translation. While model mapping deals
     * strictly with child to parent data flow, entity itself is not aware of owner, and
     * manages only downstream data. EntityModelMapping implements IEntityNotification
     * interface, direct usage of it available to the entity is use it as entry point for
     * entity own notification, which are limited to the one level of hierarchy and done in
     * local coordinates despite entity may be a part of complex composition.
     * @returns Pointer to the EntityModelMapping instance owned by this entity.
     */
    EntityModelMapping* modelMapping() const;

    /**
     * @todo Remove this method.
     *
     * Method was introduced to implement GroupingEntity::removeEmptyGroups() with minimal amount
     * of work and no significant overhead at the same time. It violates principle of entity
     * isolation, and will be removed after some refactoring.
     * @returns Pointer to the parent entity, null if there is no such.
     */
    AbstractEntity* parentEntity() const;

protected:

    /**
     * Protected member functions which provide interface for determining relations between
     * parts of entity composition. Absolutely nothing here for simple entities which
     * doesn't own any other entities.
     */

    using OffsetEvaluator = std::function<int(const AbstractEntity*)>;

    /**
     * Claim that passed reference to the entity will act as part of planar entity composition.
     * @param entity Valid pointer to the entity.
     * @params setEvaluator Function which translates 0 in the coordinate system of entity to the
     *     value in the coordinate system of upstream entity.
     */
    void assignAsSubEntity(AbstractEntity* entity, OffsetEvaluator offsetEvaluator);

    /**
     * Claim that passed reference to the entity is child of this one at the index 0.
     * @param entity Valid pointer to the entity.
     */
    void assignAsChildEntity(AbstractEntity* entity);

    /**
     * Claim that previously assigned entity is no longer part of the composition. That mean that
     * there are no notifications from it will be translated. Passed entity may be safely released
     * or rearranged by the will of caller.
     * @param Valid pointer to the entity entity assigned earlier as part of composition.
     */
    void unassignEntity(const AbstractEntity* entity);

private:
    std::unique_ptr<EntityModelMapping> m_modelMapping;
};

using AbstractEntityPtr = std::unique_ptr<AbstractEntity>;

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
