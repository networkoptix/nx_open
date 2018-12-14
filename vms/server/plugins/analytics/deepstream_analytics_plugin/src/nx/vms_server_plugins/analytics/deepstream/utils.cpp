#include "utils.h"

#include <algorithm>
#include <random>
#include <cstring>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

namespace {

void trimLeft(std::string* inOutStr)
{
    inOutStr->erase(
        inOutStr->begin(),
        std::find_if(
            inOutStr->begin(),
            inOutStr->end(),
            [](char ch) { return !std::isspace(ch); }));
}

void trimRight(std::string* inOutStr)
{
    inOutStr->erase(
        std::find_if(
            inOutStr->rbegin(),
            inOutStr->rend(),
            [](char ch) { return !std::isspace(ch);}).base(),
        inOutStr->end());
}

} // namespace

std::vector<std::string> split(const std::string& str, const std::string& delimiter)
{
    std::vector<std::string> result;
    auto current = 0;
    auto previous = 0;

    current = str.find(delimiter);
    while (current != std::string::npos)
    {
        result.push_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find(delimiter, previous);
    }

    result.push_back(str.substr(previous, current - previous));
    return result;
}

std::vector<std::string> split(const std::string& str, const char delimiter)
{
    return split(str, std::string(1, delimiter));
}

std::string trimCopy(std::string str)
{
    trimLeft(&str);
    trimRight(&str);
    return str;
}

std::string* trim(std::string* inOutStr)
{
    trimLeft(inOutStr);
    trimRight(inOutStr);
    return inOutStr;
}

nxpl::NX_GUID makeGuid()
{
    // Likely this method is not correct in the sense of randomness of output GUIDs.
    nxpl::NX_GUID guid;
    auto data = (int*) guid.bytes;
    std::mt19937 rng;
    rng.seed(std::random_device()());
    std::uniform_int_distribution<std::mt19937::result_type> dist(
        0, std::numeric_limits<int>::max());

    data[0] = dist(rng);
    data[1] = dist(rng);
    data[2] = dist(rng);
    data[3] = dist(rng);

    return guid;
}

bool isNull(const nxpl::NX_GUID& guid)
{
    return memcmp(guid.bytes, kNullGuid.bytes, sizeof(nxpl::NX_GUID::bytes)) == 0;
}

std::string makeElementName(
    const std::string& pipelineName,
    const std::string& factoryName,
    const std::string& elementName)
{
    return pipelineName + "_" + factoryName + "_" + elementName;
}

std::chrono::milliseconds now()
{
    auto timePointNow = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(timePointNow.time_since_epoch());
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
