#pragma once

#include <memory>

#include "vector2d.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

class AbstractObject;

float randomFloat(float min = 0, float max = 1);

Vector2D randomTrajectory();

std::unique_ptr<AbstractObject> randomObject();

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
