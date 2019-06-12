#include "abstract_object.h"

#include <functional>
#include <map>
#include <string>

#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "random_state_changer.h"
#include "random_pauser.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

AbstractObject::AbstractObject(const std::string& typeId, Attributes attributes):
    m_typeId(typeId),
    m_attributes(std::move(attributes)),
    m_id(nx::sdk::UuidHelper::randomUuid()),
    m_position(Vector2(randomDecimal(0, 1), randomDecimal(0, 1))),
    m_size(Size{randomDecimal(0.1F, 0.4F), randomDecimal(0.1F, 0.4F)}),
    m_speed(randomDecimal(0.005F, 0.03F)),
    m_trajectory(randomTrajectory())
{
}

Vector2 AbstractObject::position() const
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

float randomDecimal(float min, float max)
{
    if (max <= min)
        return NAN;
    return  min + (float)rand() / ((float)RAND_MAX / (max - min));
}

Vector2 randomTrajectory()
{
    return Vector2(randomDecimal(-1, 1), randomDecimal(-1, 1)).normalized();
}

std::unique_ptr<AbstractObject> randomObject()
{
    static const std::vector <
        std::function<std::unique_ptr<AbstractObject>()>> kObjectFuncs = {
        []() { return std::make_unique<Bicycle>(); },
        []() { return std::make_unique<Car>(); },
        []() { return std::make_unique<Truck>(); },
        []() { return std::make_unique<Pedestrian>(); },
        []() { return std::make_unique<HumanFace>(); }
    };

    return kObjectFuncs[rand() % kObjectFuncs.size()]();
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx