// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_object.h"

#include "random_pauser.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

const std::string kHumanFaceObjectType = "nx.stub.humanFace";

class HumanFace: public AbstractObject
{
    using base_type = AbstractObject;
public:
    HumanFace();
    virtual void update() override;

private:
    RandomPauser m_pauser;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx