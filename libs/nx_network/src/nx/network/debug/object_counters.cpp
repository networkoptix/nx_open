#include "object_counters.h"

namespace nx::network::debug {

std::string ObjectCounters::toJson() const
{
    std::map<std::string, int> values;
    values.emplace("tcpSocketCount", tcpSocketCount);
    values.emplace("udpSocketCount", udpSocketCount);
    values.emplace("sslSocketCount", sslSocketCount);
    values.emplace("cloudSocketCount", cloudSocketCount);
    values.emplace("stunClientConnectionCount", stunClientConnectionCount);
    values.emplace("stunOverHttpClientConnectionCount", stunOverHttpClientConnectionCount);
    values.emplace("stunServerConnectionCount", stunServerConnectionCount);
    values.emplace("httpClientConnectionCount", httpClientConnectionCount);
    values.emplace("httpServerConnectionCount", httpServerConnectionCount);
    values.emplace("websocketConnectionCount", websocketConnectionCount);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        values.insert(m_objectTypeNameToCount.begin(), m_objectTypeNameToCount.end());
    }

    std::string json = "{";
    for (const auto& val: values)
        json += "\"" + val.first + "\":" + std::to_string(val.second) + ",";
    json.pop_back();
    json += "}";

    return json;
}

} // namespace nx::network::debug
