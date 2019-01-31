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

private:
    std::unique_ptr<CryptographicDeviceImpl> m_impl;
};

} // namespace random
} // namespace utils
} // namespace nx
