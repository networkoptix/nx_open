// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_entity.h"

#include <QtCore/QAbstractItemModel>

#include <nx/utils/log/assert.h>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/base_notification_observer.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_model_mapping.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

AbstractEntity::AbstractEntity():
    m_modelMapping(new EntityModelMapping(this))
{
}

AbstractEntity::~AbstractEntity()
{
}

int AbstractEntity::childRowCount(int row) const
{
    if (auto entity = childEntity(row))
        return entity->rowCount();
    return 0;
}

EntityModelMapping* AbstractEntity::modelMapping() const
{
    return m_modelMapping.get();
}

AbstractEntity* AbstractEntity::parentEntity() const
{
    if (!m_modelMapping->parentMapping())
        return nullptr;

    return m_modelMapping->parentMapping()->ownerEntity();
}

void AbstractEntity::resetNotificationObserver()
{
    m_modelMapping->setNotificationObserver({});
}

void AbstractEntity::hide()
{
    NX_ASSERT("If entity is used as nested one, this method should be implemented");
}

void AbstractEntity::show()
{
    NX_ASSERT("If entity is used as nested one, this method should be implemented");
}

void AbstractEntity::setNotificationObserver(
    std::unique_ptr<BaseNotificatonObserver> notificationObserver)
{
    m_modelMapping->setNotificationObserver(std::move(notificationObserver));
}

void AbstractEntity::assignAsSubEntity(AbstractEntity* entity, OffsetEvaluator offsetEvaluator)
{
    entity->modelMapping()->assignAsSubEntity(modelMapping(), offsetEvaluator);
}

void AbstractEntity::assignAsChildEntity(AbstractEntity* entity)
{
    entity->modelMapping()->assignAsChildEntity(modelMapping());
}

void AbstractEntity::unassignEntity(const AbstractEntity* entity)
{
    entity->modelMapping()->unassignEntity();
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
