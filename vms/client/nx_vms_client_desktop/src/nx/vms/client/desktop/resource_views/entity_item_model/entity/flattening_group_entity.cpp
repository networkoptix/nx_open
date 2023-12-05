// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "flattening_group_entity.h"

#include <client/client_globals.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/base_notification_observer.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_notification_guard.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_model_mapping.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>

namespace {

using BaseNotificatonObserver =
    nx::vms::client::desktop::entity_item_model::BaseNotificatonObserver;

class NestedEntityNotificationObserver: public BaseNotificatonObserver
{
public:
    using NotificationHandler = std::function<void()>;
    NestedEntityNotificationObserver(
        const NotificationHandler& endInsertRowsHandler,
        const NotificationHandler& endRemoveRowsHandler)
        :
        m_endInsertRowsHandler(endInsertRowsHandler),
        m_endRemoveRowsHandler(endRemoveRowsHandler)
    {
    }

    virtual void beginInsertRows(int first, int) override
    {
        m_skipCheckOnEndInsert = first > 1;
    }

    virtual void endInsertRows() override
    {
        if (!m_skipCheckOnEndInsert)
            m_endInsertRowsHandler();
    }

    virtual void beginRemoveRows(int first, int) override
    {
        m_skipCheckOnEndRemove = first > 1;
    }

    virtual void endRemoveRows() override
    {
        if (!m_skipCheckOnEndRemove)
            m_endRemoveRowsHandler();
    }

private:
    bool m_skipCheckOnEndInsert = false;
    NotificationHandler m_endInsertRowsHandler;

    bool m_skipCheckOnEndRemove = false;
    NotificationHandler m_endRemoveRowsHandler;
};

} // namespace

namespace nx::vms::client::desktop {
namespace entity_item_model {

FlatteningGroupEntity::FlatteningGroupEntity(
    AbstractItemPtr headItem,
    AbstractEntityPtr nestedEntity)
    :
    base_type(),
    m_headItem(std::move(headItem)),
    m_nestedEntity(std::move(nestedEntity))
{
    NX_ASSERT(m_headItem, "Group head item should be defined");
    if (!m_headItem)
        m_headItem = GenericItemBuilder();

    setupHeadItemNotifications();
    setNestedEntityModelMapping();
    setupNestedEntityObserver();
    checkFlatteningCondition();
}

FlatteningGroupEntity::~FlatteningGroupEntity()
{
    auto guard = removeRowsGuard(modelMapping(), 0, rowCount());
}

bool FlatteningGroupEntity::isFlattened() const
{
    return m_isFlattened;
}

void FlatteningGroupEntity::setFlattened(bool flattened)
{
    if (isFlattened() == flattened)
        return;

    if (flattened)
        transformToFlat();
    else
        transformFromFlat();
}

FlatteningGroupEntity::AutoFlatteningPolicy FlatteningGroupEntity::autoFlatteningPolicy() const
{
    return m_autoFlatteningPolicy;
}

void FlatteningGroupEntity::setAutoFlatteningPolicy(AutoFlatteningPolicy policy)
{
    if (m_autoFlatteningPolicy == policy)
        return;

    m_autoFlatteningPolicy = policy;
    checkFlatteningCondition();
}

int FlatteningGroupEntity::rowCount() const
{
    if (isFlattened())
        return (m_nestedEntity ? m_nestedEntity->rowCount() : 0) + (m_headIsHidden ? 0 : 1);

    return 1;
}

AbstractEntity* FlatteningGroupEntity::childEntity(int row) const
{
    if (isFlattened())
    {
        if (row == 0 && !m_headIsHidden)
            return nullptr;

        if (!m_nestedEntity)
            return nullptr;

        return m_nestedEntity->childEntity(row - (m_headIsHidden ? 0 : 1));
    }

    if ((row == 0) && m_nestedEntity)
        return m_nestedEntity.get();

    return nullptr;
}

int FlatteningGroupEntity::childEntityRow(const AbstractEntity* entity) const
{
    if (!m_nestedEntity)
        return -1;

    if (isFlattened())
    {
        const int row = m_nestedEntity->childEntityRow(entity);
        const int offset = (m_headIsHidden || row < 0) ? 0 : 1;
        return row + offset;
    }

    if (m_nestedEntity.get() == entity)
        return 0;

    return -1;
}

QVariant FlatteningGroupEntity::data(int row, int role /*= Qt::DisplayRole*/) const
{
    if (isFlattened())
    {
        if (row == 0 && !m_headIsHidden)
            return m_headItem->data(role);

        if (!m_nestedEntity)
            return QVariant();

        return m_nestedEntity->data(row - (m_headIsHidden ? 0 : 1), role);
    }

    return (row == 0) ? m_headItem->data(role) : QVariant();
}

Qt::ItemFlags FlatteningGroupEntity::flags(int row) const
{
    if (isFlattened())
    {
        if (row == 0 && !m_headIsHidden)
            return m_headItem->flags();

        if (!m_nestedEntity)
            return Qt::ItemFlags();

        return m_nestedEntity->flags(row - (m_headIsHidden ? 0 : 1));
    }

    return (row == 0) ? m_headItem->flags() : Qt::ItemFlags();
}

bool FlatteningGroupEntity::isPlanar() const
{
    return false;
}

void FlatteningGroupEntity::setupHeadItemNotifications()
{
    m_headItem->setDataChangedCallback(
        [this](const QVector<int>& roles)
        {
            if (m_autoFlatteningPolicy == AutoFlatteningPolicy::headItemControl)
            {
                // Empty roles list means "all roles".
                if (roles.empty() || roles.contains(Qn::FlattenedRole))
                    checkFlatteningCondition();
            }

            if (!isFlattened())
                dataChanged(modelMapping(), roles, 0);
        });
}

void FlatteningGroupEntity::setupNestedEntityObserver()
{
    if (!m_nestedEntity)
        return;

    m_nestedEntity->setNotificationObserver(std::make_unique<NestedEntityNotificationObserver>(
        [this]{ if (isFlattened()) checkFlatteningCondition(); }, //< End insert rows handler.
        [this]{ if (!isFlattened()) checkFlatteningCondition(); })); //< End remove rows handler.
}

void FlatteningGroupEntity::setNestedEntityModelMapping()
{
    if (!m_nestedEntity)
        return;

    if (isFlattened())
    {
        assignAsSubEntity(m_nestedEntity.get(),
           [this](const AbstractEntity*) { return m_headIsHidden ? 0 : 1; });
    }
    else
    {
        assignAsChildEntity(m_nestedEntity.get());
    }
}

void FlatteningGroupEntity::checkFlatteningCondition()
{
    const auto nestedEntityRowCount = m_nestedEntity ? m_nestedEntity->rowCount() : 0;
    switch (m_autoFlatteningPolicy)
    {
        case AutoFlatteningPolicy::noChildrenPolicy:
        {
            setFlattened(nestedEntityRowCount == 0);
            break;
        }

        case AutoFlatteningPolicy::singleChildPolicy:
        {
            setFlattened(nestedEntityRowCount < 2);
            break;
        }

        case AutoFlatteningPolicy::headItemControl:
        {
            const auto flattenedData = m_headItem->data(Qn::FlattenedRole);
            if (flattenedData.isValid())
                setFlattened(flattenedData.toBool());
            break;
        }

        case AutoFlatteningPolicy::noPolicy:
        {
            break;
        }
    }
}

void FlatteningGroupEntity::transformToFlat()
{
    if (!m_nestedEntity)
    {
        m_isFlattened = true;
    }
    else
    {
        const int insertBeforeRow = 1;
        auto guard =
            moveRowsGuard(modelMapping(), m_nestedEntity.get(), 0, m_nestedEntity->rowCount(),
            this, insertBeforeRow);

        m_isFlattened = true;
        assignAsSubEntity(m_nestedEntity.get(),
            [this](const AbstractEntity*) { return m_headIsHidden ? 0 : 1; });
    }

    auto guard = removeRowsGuard(modelMapping(), 0);
    m_headIsHidden = true;
}

void FlatteningGroupEntity::transformFromFlat()
{
    {
        auto guard = insertRowsGuard(modelMapping(), 0);
        m_headIsHidden = false;
    }

    if (!m_nestedEntity)
    {
        m_isFlattened = false;
        return;
    }

    const auto insertBeforeRow = 0;
    auto guard = moveRowsGuard(modelMapping(), m_nestedEntity.get(), 0, m_nestedEntity->rowCount(),
        this, insertBeforeRow);

    m_isFlattened = false;
    assignAsChildEntity(m_nestedEntity.get());
}

FlatteningGroupEntityPtr makeFlatteningGroup(
    AbstractItemPtr headItem,
    AbstractEntityPtr nestedEntity,
    FlatteningGroupEntity::AutoFlatteningPolicy flatteningPolicy)
{
    auto result = std::make_unique<FlatteningGroupEntity>(
        std::move(headItem), std::move(nestedEntity));

    result->setAutoFlatteningPolicy(flatteningPolicy);
    return result;
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
