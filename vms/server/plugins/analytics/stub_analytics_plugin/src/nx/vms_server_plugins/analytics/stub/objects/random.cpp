#include "random.h"

#include <limits>
#include <vector>
#include <functional>

#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

float randomFloat(float min, float max)
{
    if (!NX_KIT_ASSERT(min < max))
        return std::numeric_limits<float>::quiet_NaN();
    return  min + (float) rand() / ((float) RAND_MAX / (max - min));
}

Vector2D randomTrajectory()
{
    return Vector2D(randomFloat(-1, 1), randomFloat(-1, 1)).normalized();
}

std::unique_ptr<AbstractObject> RandomObjectGenerator::generate() const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_registry.empty())
        return nullptr;

    return m_registry[rand() % m_registry.size()].factory();
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
