// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pedestrian.h"

#include "random.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;

namespace {

static Attributes makeAttributes()
{
    static const std::vector<const char*> kDirections = {
        "Towards the camera", "Away from the camera"};
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