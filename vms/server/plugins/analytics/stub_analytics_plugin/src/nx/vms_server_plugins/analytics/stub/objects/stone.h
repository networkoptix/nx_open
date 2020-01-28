// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_object.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

const std::string kStoneObjectType = "nx.stub.stone";

class Stone: public AbstractObject
{
    using base_type = AbstractObject;
public:
    Stone();

    virtual void update() override {};

};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
