// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "taxonomy_object.h"

#include <nx/vms_server_plugins/analytics/stub/deprecated_object_detection/objects/random.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

using namespace nx::sdk;

static std::vector<Ptr<Attribute>> makeSdkAttributes(
    std::map<std::string, std::string> attributes)
{
    std::vector<nx::sdk::Ptr<nx::sdk::Attribute>> result;
    for (const auto& nameAndValue: attributes)
    {
        result.push_back(nx::sdk::makePtr<Attribute>(
            Attribute::Type::string, //< Type doesn't matter here.
            nameAndValue.first,
            nameAndValue.second));
    }

    return result;
}

TaxonomyObject::TaxonomyObject(
    std::string objectTypeId,
    std::map<std::string, std::string> attributes)
    :
    base_type(std::move(objectTypeId), makeSdkAttributes(std::move(attributes)))
{
}

void TaxonomyObject::update()
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

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx