#include "random.h"

#include <vector>
#include <functional>

#include "bicycle.h"
#include "vehicle.h"
#include "pedestrian.h"
#include "human_face.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

float randomFloat(float min, float max)
{
    if (max <= min)
        return NAN;
    return  min + (float) rand() / ((float) RAND_MAX / (max - min));
}

Vector2D randomTrajectory()
{
    return Vector2D(randomFloat(-1, 1), randomFloat(-1, 1)).normalized();
}

std::unique_ptr<AbstractObject> randomObject()
{
    static const std::vector<std::function<std::unique_ptr<AbstractObject>()>> kObjectFuncs = {
        [](){ return std::make_unique<Bicycle>(); },
        [](){ return std::make_unique<Car>(); },
        [](){ return std::make_unique<Truck>(); },
        [](){ return std::make_unique<Pedestrian>(); },
        [](){ return std::make_unique<HumanFace>(); }
    };

    return kObjectFuncs[rand() % kObjectFuncs.size()]();
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx