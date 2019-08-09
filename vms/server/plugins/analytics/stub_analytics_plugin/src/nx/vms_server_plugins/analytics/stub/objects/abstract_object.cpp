// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_object.h"

#include <functional>
#include <map>
#include <string>

#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "random_state_changer.h"
#include "random_pauser.h"
#include "random.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

AbstractObject::AbstractObject(const std::string& typeId, Attributes attributes):
    m_typeId(typeId),
    m_attributes(std::move(attributes)),
    m_id(nx::sdk::UuidHelper::randomUuid()),
    m_position(Vector2D(randomFloat(0, 1), randomFloat(0, 1))),
    m_size(Size(randomFloat(0.1F, 0.4F), randomFloat(0.1F, 0.4F))),
    m_speed(randomFloat(0.005F, 0.03F)),
    m_trajectory(randomTrajectory())
{
}

Vector2D AbstractObject::position() const
{
    return m_position;
}

Size AbstractObject::size() const
{
    return m_size;
}

Attributes AbstractObject::attributes() const
{
    return m_attributes;
}

nx::sdk::Uuid AbstractObject::id() const
{
    return m_id;
}

bool AbstractObject::inBounds() const
{
    return m_position.x >= 0
        && m_position.y >= 0
        && m_position.x + m_size.width <= 1
        && m_position.y + m_size.height <= 1;
}

std::string AbstractObject::typeId() const
{
    return m_typeId;
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx