// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "random_qt_device.h"

#if defined(Q_OS_MAC)
    #include <pthread.h>
#endif

#include <chrono>

#include <QtCore/QRandomGenerator>

namespace nx {
namespace utils {
namespace random {

QtDevice::QtDevice()
{
    const auto time = std::chrono::steady_clock::now() - std::chrono::steady_clock::time_point();
    QRandomGenerator::global()->seed((uint)std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
}

QtDevice::result_type QtDevice::operator()()
{
    return QRandomGenerator::global()->generate64() % (max() - min()) + min();
}

double QtDevice::entropy() const
{
    return 0;
}

QtDevice& QtDevice::instance()
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

} // namespace random
} // namespace utils
} // namespace nx
