#include "abstract_object.h"

#include <iostream>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;

namespace {

static constexpr char kObjectTypeId[] = "nx.stub.pedestrian";
static constexpr char kTypeName[] = "Pedestrian";

static Attributes makeAttributes()
{
    static std::vector<const char*> kDirections = { "Towards the camera", "Away from the camera" };
    return {
        nx::sdk::makePtr<Attribute>(
            IAttribute::Type::string,
            "Direction",
            kDirections[rand() % kDirections.size()]),
    };
}

} // namespace

Pedestrian::Pedestrian():
    base_type(kPedestrianObjectType, makeAttributes())
{
}

void Pedestrian::update()
{
    m_pauser.update();
    if (m_pauser.paused())
        return;

    bool updateTrajectory = m_pauser.resuming();
    if (!updateTrajectory)
    {
        m_stateChanger.update();
        updateTrajectory = m_stateChanger.stateChanged();
    }

    if (updateTrajectory)
        m_trajectory = randomTrajectory();

    m_position += m_trajectory * m_speed;
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx