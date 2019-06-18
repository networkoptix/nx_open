#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/helpers/ptr.h>

#include "vector2d.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using Attributes = std::vector<nx::sdk::Ptr<nx::sdk::Attribute>>;

class AbstractObject
{
public:
    AbstractObject(const std::string& typeId, Attributes attributes);
    ~AbstractObject() = default;

    Vector2D position() const;
    Size size() const;
    Attributes attributes() const;
    nx::sdk::Uuid id() const;
    std::string typeId() const;

    bool inBounds() const;

    virtual void update() = 0;

protected:
    std::string m_typeId;
    Attributes m_attributes;
    nx::sdk::Uuid m_id;
    Vector2D m_position;
    Size m_size;
    float m_speed = 0;
    Vector2D m_trajectory;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx