#include "random.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <random>

#if defined(Q_OS_MAC)
    #include <pthread.h>
#endif

namespace nx {
namespace utils {
namespace random {

QtDevice::QtDevice()
{
    const auto time = std::chrono::steady_clock::now() - std::chrono::steady_clock::time_point();
    ::qsrand((uint) std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
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
    static result_type qrandMask = ((result_type) 1 << qrandBits) - 1;
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

QtDevice& qtDevice()
{
    // There is a bug in OSX's clang and gcc, so thread_local is not supported :(
    #if !defined(Q_OS_MAC)
        thread_local QtDevice rd;
        return rd;
    #else
        static pthread_key_t key;
        static auto init = pthread_key_create(
            &key, [](void* p) { if (p) delete static_cast<QtDevice*>(p); });

        static_cast<void>(init);
        if (auto rdp = static_cast<QtDevice*>(pthread_getspecific(key)))
            return *rdp;

        const auto rdp = new QtDevice();
        pthread_setspecific(key, rdp);
        return *rdp;
    #endif
}

QByteArray generate(std::size_t count)
{
    auto& device = qtDevice();
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

} // namespace random
} // namespace utils
} // namespace nx
