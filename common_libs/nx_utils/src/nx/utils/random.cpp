#include "random.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <random>

namespace nx {
namespace utils {
namespace random {

QByteArray generate(std::size_t count)
{
    auto& device = QtDevice::instance();
    QByteArray data(static_cast<int>(count), Qt::Uninitialized);
    for (int i = 0; i != data.size(); ++i)
    {
        // NOTE: Direct device access with simple cast works significantly faster than
        //     uniform_int_distribution on a number of platforms (stl implemenations).
        //     Found on iOS by profiler.
        const auto n = device();
        data[i] = reinterpret_cast<const char&>(n);
    }

    return data;
}

QByteArray generateName(int length)
{
    return generateName(QtDevice::instance(), length);
}

} // namespace random
} // namespace utils
} // namespace nx
