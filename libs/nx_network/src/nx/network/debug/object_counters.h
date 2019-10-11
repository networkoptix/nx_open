#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <typeinfo>

#include <nx/utils/log/to_string.h>

namespace nx::network::debug {

class NX_NETWORK_API ObjectCounters
{
public:
    std::atomic<int> tcpSocketCount{0};
    std::atomic<int> udpSocketCount{0};
    std::atomic<int> sslSocketCount{0};
    std::atomic<int> cloudSocketCount{0};
    std::atomic<int> stunClientConnectionCount{0};
    std::atomic<int> stunOverHttpClientConnectionCount{0};
    std::atomic<int> stunServerConnectionCount{0};
    std::atomic<int> httpClientConnectionCount{0};
    std::atomic<int> httpServerConnectionCount{0};
    std::atomic<int> websocketConnectionCount{0};

    template<typename T> void recordObjectCreation()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_objectTypeNameToCount[demangleTypeName(typeid(T).name()).toStdString()];
    }
    
    template<typename T> void recordObjectDestruction()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_objectTypeNameToCount.find(demangleTypeName(typeid(T).name()).toStdString());
        if (it == m_objectTypeNameToCount.end())
            return;

        --it->second;
        if (it->second == 0)
            m_objectTypeNameToCount.erase(it);
    }

    std::string toJson() const;

private:
    mutable std::mutex m_mutex;
    std::map<std::string, int /*counter*/> m_objectTypeNameToCount;
};

} // namespace nx::network::debug
