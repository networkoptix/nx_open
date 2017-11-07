#pragma once

#include <memory>
#include <random>

namespace nx {
namespace utils {
namespace random {

struct CryptographicRandomDeviceImpl;

class NX_UTILS_API CryptographicRandomDevice
{
public:
    using result_type = unsigned int;

    CryptographicRandomDevice();
    ~CryptographicRandomDevice();

    result_type operator()();
    double entropy() const;

    static constexpr result_type min() { return std::random_device::min(); }
    static constexpr result_type max() { return std::random_device::max(); }

    static CryptographicRandomDevice& instance();

private:
    std::unique_ptr<CryptographicRandomDeviceImpl> m_impl;
};

} // namespace random
} // namespace utils
} // namespace nx
