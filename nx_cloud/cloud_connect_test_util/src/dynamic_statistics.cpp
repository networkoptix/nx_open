#include "dynamic_statistics.h"

#include <sstream>

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
    if (!m_prevValue)
        m_prevValue = value;
    if (!m_prevValueTime)
        m_prevValueTime = std::chrono::steady_clock::now();

    const auto timePassed = 
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - *m_prevValueTime);
    if (timePassed <= std::chrono::milliseconds::zero())
        return;

    m_inputBandwidth = calcBandwidth(
        m_inputBandwidth, value.bytesReceived, m_prevValue->bytesReceived, timePassed);

    m_outputBandwidth = calcBandwidth(
        m_outputBandwidth, value.bytesSent, m_prevValue->bytesSent, timePassed);
}

std::string DynamicStatistics::toStdString() const
{
    std::ostringstream statisticsStringStream;
    statisticsStringStream << "rate: ";
    statisticsStringStream << "in: ";
    printBandwidth(statisticsStringStream, m_inputBandwidth);
    statisticsStringStream << ", ";
    statisticsStringStream << "out: ";
    printBandwidth(statisticsStringStream, m_outputBandwidth);
    return statisticsStringStream.str();
}

size_t DynamicStatistics::calcBandwidth(
    size_t prevCalculatedBytesPerSecond,
    size_t bytesTransferred,
    size_t prevBytesTransferred,
    std::chrono::milliseconds timePassed)
{
    const auto bandwidthSincePrevMark =
        ((bytesTransferred - prevBytesTransferred) * 1000) / timePassed.count();
    return (prevCalculatedBytesPerSecond * 0.7) + (bandwidthSincePrevMark * 0.3);
}

void DynamicStatistics::printBandwidth(
    std::ostream& statisticsStringStream, size_t bandwidth)
{
    std::string unitName = "Bps";
    if (bandwidth > 10 * 1024)
    {
        bandwidth /= 1024;
        unitName = "KBps";
    }

    if (bandwidth > 10 * 1024)
    {
        bandwidth /= 1024;
        unitName = "MBps";
    }

    statisticsStringStream << bandwidth << " " << unitName;
}

} // namespace cctu
} // namespace nx
