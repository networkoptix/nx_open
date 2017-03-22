#include "dynamic_statistics.h"

#include <sstream>

#include <nx/utils/string.h>

namespace nx {
namespace cctu {

DynamicStatistics::DynamicStatistics():
    m_inputBandwidth(0),
    m_outputBandwidth(0)
{
}

void DynamicStatistics::addValue(
    const nx::network::test::ConnectionTestStatistics& value)
{
    const auto currentTime = std::chrono::steady_clock::now();

    if (!m_prevValue)
        m_prevValue = value;
    if (!m_prevValueTime)
        m_prevValueTime = currentTime;

    const auto timePassed = 
        std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - *m_prevValueTime);
    if (timePassed <= std::chrono::milliseconds::zero())
        return;

    m_inputBandwidth = calcBandwidth(
        m_inputBandwidth, value.bytesReceived, m_prevValue->bytesReceived, timePassed);

    m_outputBandwidth = calcBandwidth(
        m_outputBandwidth, value.bytesSent, m_prevValue->bytesSent, timePassed);

    m_prevValue = value;
    m_prevValueTime = currentTime;
}

std::string DynamicStatistics::toStdString() const
{
    std::ostringstream statisticsStringStream;
    statisticsStringStream << "rate: ";

    statisticsStringStream << "in: ";
    statisticsStringStream << nx::utils::bytesToString(m_inputBandwidth).toStdString() << "Bps";
    statisticsStringStream << ", ";

    statisticsStringStream << "out: ";
    statisticsStringStream << nx::utils::bytesToString(m_outputBandwidth).toStdString() << "Bps";
    return statisticsStringStream.str();
}

size_t DynamicStatistics::calcBandwidth(
    size_t prevCalculatedBytesPerSecond,
    size_t bytesTransferred,
    size_t prevBytesTransferred,
    std::chrono::milliseconds timePassed)
{
    if (bytesTransferred < prevBytesTransferred)
        return prevCalculatedBytesPerSecond;

    const auto bandwidthSincePrevMark =
        ((bytesTransferred - prevBytesTransferred) * 1000) / timePassed.count();
    return (prevCalculatedBytesPerSecond * 0.7) + (bandwidthSincePrevMark * 0.3);
}

} // namespace cctu
} // namespace nx
