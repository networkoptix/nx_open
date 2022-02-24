// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <typeinfo>
#include <type_traits>

#include <nx/utils/log/to_string.h>

namespace nx::network::debug {

class NX_NETWORK_API ObjectCounters
{
public:
    std::atomic<int> tcpSocketCount{0};
    std::atomic<int> udpSocketCount{0};
    std::atomic<int> sslSocketCount{0};
    std::atomic<int> stunClientConnectionCount{0};
    std::atomic<int> stunOverHttpClientConnectionCount{0};
    std::atomic<int> stunServerConnectionCount{0};
    std::atomic<int> httpClientConnectionCount{0};
    std::atomic<int> httpServerConnectionCount{0};
    std::atomic<int> websocketConnectionCount{0};

    template<typename T>
    void recordObjectCreation(T* /*ptr*/)
    {
        recordObjectCreation<std::decay_t<T>>();
    }

    template<typename T>
    void recordObjectCreation()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_objectTypeNameToCount[demangleTypeName(typeid(T).name()).toStdString()];
    }

    template<typename T>
    void recordObjectDestruction(T* /*ptr*/)
    {
        recordObjectDestruction<std::decay_t<T>>();
    }

    template<typename T>
    void recordObjectDestruction()
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

    std::map<std::string /*type name*/, int /*counter*/> aliveObjects() const;

private:
    mutable std::mutex m_mutex;
    mutable std::mutex m_httpClientMutex;
    std::map<std::string, int /*counter*/> m_objectTypeNameToCount;
};

} // namespace nx::network::debug
