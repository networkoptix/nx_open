#pragma once

#include <chrono>
#include <iostream>
#include <string>

#include <boost/optional.hpp>

#include <nx/network/test_support/socket_test_helper.h>

namespace nx {
namespace cctu {

class DynamicStatistics
{
public:
    DynamicStatistics();

    void addValue(const nx::network::test::ConnectionTestStatistics& value);
    std::string toStdString() const;

private:
    boost::optional<nx::network::test::ConnectionTestStatistics> m_prevValue;
    boost::optional<std::chrono::steady_clock::time_point> m_prevValueTime;
    size_t m_inputBandwidth;
    size_t m_outputBandwidth;

    size_t calcBandwidth(
        size_t prevCalculatedBytesPerSecond,
        size_t bytesTransferred,
        size_t prevBytesTransferred,
        std::chrono::milliseconds timePassed);
};

} // namespace cctu
} // namespace nx
