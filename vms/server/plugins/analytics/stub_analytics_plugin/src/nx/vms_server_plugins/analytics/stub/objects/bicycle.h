#pragma once

#include "abstract_object.h"

#include "random_state_changer.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

const std::string kBicycleObjectType = "nx.stub.bicycle";

class Bicycle: public AbstractObject
{
    using base_type = AbstractObject;
public:
    Bicycle();
    virtual void update() override;

private:
    RandomStateChanger m_stateChanger;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx