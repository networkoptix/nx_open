#pragma once

#include "abstract_object.h"

#include "random_pauser.h"
#include "random_state_changer.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

const std::string kPedestrianObjectType = "nx.stub.pedestrian";

class Pedestrian: public AbstractObject
{
    using base_type = AbstractObject;
public:
    Pedestrian();

    virtual void update() override;

private:
    RandomPauser m_pauser;
    RandomStateChanger m_stateChanger;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx