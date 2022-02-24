// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <random>

namespace nx {
namespace utils {
namespace random {

/**
 * Exception free, qrand based random device.
 * NOTE: Should be used on platforms where std::random_device does not work as expected.
 */
class NX_UTILS_API QtDevice
{
public:
    typedef unsigned int result_type;

    QtDevice();

    result_type operator()();
    double entropy() const;

    static constexpr result_type min() { return std::random_device::min(); }
    static constexpr result_type max() { return std::random_device::max(); }

    /** Thread local QtDevice. */
    static QtDevice& instance();
};

} // namespace random
} // namespace utils
} // namespace nx
