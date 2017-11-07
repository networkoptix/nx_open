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

QByteArray generateName(int length)
{
    return generateName(qtDevice(), length);
}

} // namespace random
} // namespace utils
} // namespace nx
