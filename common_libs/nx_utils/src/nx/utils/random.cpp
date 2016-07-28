#include "random.h"

#include <algorithm>
#include <mutex>
#include <random>

namespace nx {
namespace utils {
namespace random {

std::random_device& device()
{
    static thread_local std::random_device rd;
    return rd;
}

QByteArray generate(std::size_t count, bool* isOk)
{
    QByteArray data(static_cast<int>(count), Qt::Uninitialized);
    try
    {
        std::uniform_int_distribution<int> distribution(
            std::numeric_limits<std::int8_t>::min(),
            std::numeric_limits<std::int8_t>::max());

        std::generate(
            data.data(), data.data() + count,
            [&distribution] { return distribution(device()); });

        if (isOk)
            *isOk = true;
    }
    catch (const std::exception&)
    {
        if (isOk)
            *isOk = false;
    }

    return data;
}

} // namespace random
} // namespace utils
} // namespace nx
