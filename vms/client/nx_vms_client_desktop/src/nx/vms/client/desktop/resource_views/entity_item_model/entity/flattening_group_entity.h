// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/abstract_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

/**
 * FlatteningGroupEntity is entity wrapper, which along with single item defines entity with
 * one row and wrapped entity as its child. In a simple words: it will be mapped to a model
 * as single collapsible node with any user-defined contents. It have special trait: group
 * may be 'flattened', in the other words: became identity nested entity wrapper (head item
 * will be hidden). This process is reversible and automatically generates notifications if
 * being mapped to a model. Nested entity is moved between levels with all persistent
 * indexes preserved. It's possible to do this conversion manually or make it automatic,
 * triggered by nested entity row count change.
 */
class NX_VMS_CLIENT_DESKTOP_API FlatteningGroupEntity: public AbstractEntity
{
    using base_type = AbstractEntity;

public:
    enum class AutoFlatteningPolicy
    {
        noPolicy,
        noChildrenPolicy,
        singleChildPolicy,
        headItemControl, //< Head item will control state using Qn::FlattenedRole.
    };

    /**
     * Constructor. Composes group entity from the given head item and nested entity.
     * @param headItem Any valid item. Null input is inacceptable and will lead to creation of
     *     degenerate entity with zero rows, in this case ownership of nested entity will be
     *     transferred to the group anyway, but then it will be immediately discarded. Group
     *     takes ownership of the head item.
     * @param nestedEntity Any valid entity, null one is either acceptable. Group takes
     *     ownership of nested entity.
     */
    FlatteningGroupEntity(AbstractItemPtr headItem, AbstractEntityPtr nestedEntity);

    /**
     * Destructor
     */
    virtual ~FlatteningGroupEntity() override;

    /**
     * @returns True if group is in the 'flattened' state, false otherwise.
     */
    bool isFlattened() const;

    /**
     * Used to switch group between normal and 'flattened' state. If auto flattening policy differs
     * from AutoFlatteningPolicy::noPolicy, call to this function will be considered as unexpected.
     * @param True for 'flattened' state false for normal state. By default group is in the normal
     *     state.
     */
    void setFlattened(bool flattened);

    /**
     * @returns Current flattening policy. Default value is AutoFlatteningPolicy::noPolicy,
     *     that means that group state can be changed only by caller.
     */
    AutoFlatteningPolicy autoFlatteningPolicy() const;

    /**
     * Sets automatic flattening policy, accepts following values:
     *
     *  - AutoFlatteningPolicy::noPolicy. No group state conversions based on nested entity
     *    children count.
     *
     *  - AutoFlatteningPolicy::noChildrenPolicy. Group became 'flattened' when nested entity
     *    became  empty, as result group will be invisible since it will have zero rows. Rule
     *    is symmetric: group will back in the normal state as soon as nested entity gets some
     *    contents.
     *
     *  - AutoFlatteningPolicy::singleChildPolicy. Group became 'flattened' when nested entity
     *    child count become one or less, and vice versa.
     */
    void setAutoFlatteningPolicy(AutoFlatteningPolicy policy);

    /**
     * Implements AbstractEntity::rowCount().
     * @returns 1 in the normal group state, nested entity row count in a flattened state, 0 if
     *     group initialized with inappropriate data. Please note, nested entity row count also
     *     may be 0, i.e. zero value isn't a sign that group is improperly initialized).
     */
    virtual int rowCount() const override;

    /**
     * Implements AbstractEntity::childEntity().
     * @param row The row.
     * @returns pointer to the nested entity (may be nullptr) in the normal group state,
     *     equivalent of childEntity() call of the nested entity in a 'flattened' state.
     */
    virtual AbstractEntity* childEntity(int row) const override;

    /**
     * Implements AbstractEntity::childEntityRow().
     * @returns O if nested entity is set and equals to the parameter in the normal state.
     *     Equivalent of childEntityRow() call of the nested entity with the same parameter in a
     *     'flattened' state, -1 in any other case.
     */
    virtual int childEntityRow(const AbstractEntity* entity) const override;

    /**
     * Implements AbstractEntity()::data().
     * @returns Data provided by the head item in the normal state if given row is 0.
     *     Data provided by the nested entity if such exists for given row and role in a
     *     'flattened' state. Default constructed QVariant if parameters are invalid.
     */
    virtual QVariant data(int row, int role = Qt::DisplayRole) const override;

    /**
     * Implements AbstractEntity()::flags().
     * @returns Flags provided by the head item in the normal state if given row is 0.
     *     Flags provided by the nested entity if such exists for given row in a 'flattened' state.
     *     Default constructed Qt::ItemFlags if passed row is invalid.
     */
    virtual Qt::ItemFlags flags(int row) const override;

    /**
     * Implements AbstractEntity()::isPlanar().
     * @returns false.
     */
    virtual bool isPlanar() const override;

private:
    void setupHeadItemNotifications();
    void setupNestedEntityObserver();
    void setNestedEntityModelMapping();
    void checkFlatteningCondition();
    void transformToFlat();
    void transformFromFlat();

private:
    AbstractItemPtr m_headItem;
    AbstractEntityPtr m_nestedEntity;

    AutoFlatteningPolicy m_autoFlatteningPolicy = AutoFlatteningPolicy::noPolicy;
    bool m_isFlattened = false;
    bool m_headIsHidden = false;
};

using FlatteningGroupEntityPtr = std::unique_ptr<FlatteningGroupEntity>;

/**
 * Convenient non-member factory function.
 */
FlatteningGroupEntityPtr makeFlatteningGroup(
    AbstractItemPtr headItem,
    AbstractEntityPtr nestedEntity,
    FlatteningGroupEntity::AutoFlatteningPolicy flatteningPolicy =
        FlatteningGroupEntity::AutoFlatteningPolicy::noPolicy);

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
