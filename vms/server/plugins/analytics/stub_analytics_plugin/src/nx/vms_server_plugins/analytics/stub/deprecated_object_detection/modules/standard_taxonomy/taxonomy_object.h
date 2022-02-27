// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms_server_plugins/analytics/stub/deprecated_object_detection/objects/abstract_object.h>
#include <nx/vms_server_plugins/analytics/stub/deprecated_object_detection/objects/random_pauser.h>
#include <nx/vms_server_plugins/analytics/stub/deprecated_object_detection/objects/random_state_changer.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

class TaxonomyObject: public AbstractObject
{
    using base_type = AbstractObject;

public:
    TaxonomyObject(std::string objectTypeId, std::map<std::string, std::string> attributes);

    virtual void update() override;

private:
    RandomPauser m_pauser;
    RandomStateChanger m_stateChanger;
};

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx