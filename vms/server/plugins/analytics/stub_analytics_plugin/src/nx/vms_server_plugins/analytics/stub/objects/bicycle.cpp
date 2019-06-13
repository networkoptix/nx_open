#include "abstract_object.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;

namespace {

Attributes makeAttributes()
{
    static const std::vector<const char*> kBikeTypes = {
        "Mountain Bike", "Road Bike", "Fixie", "BMX"
    };

    static const std::vector<const char*> kWheelSizes = {
        "20", "24", "26", "27.5", "29"
    };

    return {
        makePtr<Attribute>(
            IAttribute::Type::string,
            "Type",
            kBikeTypes[rand() % kBikeTypes.size()]),
        makePtr<Attribute>(
            IAttribute::Type::string,
            "Wheel Size",
            kWheelSizes[rand() % kWheelSizes.size()])
    };
}

static std::unique_ptr<AbstractObject> newObject()
{
    return std::make_unique<Bicycle>();
}

} // namespace

Bicycle::Bicycle():
    base_type(kBicycleObjectType, makeAttributes())
{
}

void Bicycle::update()
{
    m_stateChanger.update();
    if (m_stateChanger.stateChanged())
        m_trajectory = randomTrajectory();

    m_position += m_trajectory * m_speed;
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx