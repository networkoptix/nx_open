#include "qt_random_device.h"

#include <chrono>

#include <QtCore/QtGlobal>

namespace nx {
namespace utils {
namespace random {

QtDevice::QtDevice()
{
    const auto time = std::chrono::steady_clock::now() - std::chrono::steady_clock::time_point();
    ::qsrand((uint)std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
}

static uint usedBits(QtDevice::result_type number)
{
    uint result = 0;
    while ((number & 0xFF) == 0xFF)
    {
        number >>= 8;
        result += 8;
    }

    return result;
}

QtDevice::result_type QtDevice::operator()()
{
    static auto qrandBits = usedBits(RAND_MAX);
    static result_type qrandMask = ((result_type)1 << qrandBits) - 1;
    static auto neededBits = usedBits(max() - min());

    result_type result = 0;
    for (uint bits = 0; bits < neededBits; bits += qrandBits)
        result = (result << qrandBits) | (::qrand() & qrandMask);

    return result + min();
}

double QtDevice::entropy() const
{
    return 0;
}

} // namespace random
} // namespace utils
} // namespace nx
