// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_counters.h"

namespace nx::network::debug {

std::string ObjectCounters::toJson() const
{
    std::map<std::string, int> values;
    values.emplace("tcpSocketCount", tcpSocketCount);
    values.emplace("udpSocketCount", udpSocketCount);
    values.emplace("sslSocketCount", sslSocketCount);
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
    for (const auto& val : values)
        json += "\"" + val.first + "\":" + std::to_string(val.second) + ",";
    json.pop_back();
    json += "}";

    return json;
}

std::map<std::string /*type name*/, int /*counter*/> ObjectCounters::aliveObjects() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_objectTypeNameToCount;
}

} // namespace nx::network::debug
