// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <random>

namespace nx {
namespace utils {
namespace random {

struct CryptographicDeviceImpl;

class NX_UTILS_API CryptographicDevice
{
public:
    using result_type = unsigned int;

    CryptographicDevice();
    ~CryptographicDevice();

    result_type operator()();
    double entropy() const;

    static constexpr result_type min() { return std::random_device::min(); }
    static constexpr result_type max() { return std::random_device::max(); }

    static CryptographicDevice& instance();
};

} // namespace random
} // namespace utils
} // namespace nx
