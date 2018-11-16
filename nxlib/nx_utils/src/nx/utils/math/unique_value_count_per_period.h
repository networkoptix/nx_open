#pragma once

#include <chrono>
#include <map>

#include <nx/utils/time.h>

namespace nx::utils::math {

template<typename Key>
class UniqueValueCountPerPeriod
{
public:
    UniqueValueCountPerPeriod(std::chrono::milliseconds period):
        m_period(period)
    {
    }

    void add(const Key& key)
    {
        removeExpiredKeys();

        m_keyToExpirationTime[key] = nx::utils::monotonicTime() + m_period;
    }

    int count() const
    {
        const_cast<UniqueValueCountPerPeriod*>(this)->removeExpiredKeys();
        return static_cast<int>(m_keyToExpirationTime.size());
    }

private:
    const std::chrono::milliseconds m_period;
    std::map<Key, std::chrono::steady_clock::time_point> m_keyToExpirationTime;

    void removeExpiredKeys()
    {
        const auto currentTime = nx::utils::monotonicTime();

        for (auto it = m_keyToExpirationTime.begin();
            it != m_keyToExpirationTime.end();
            )
        {
            if (it->second < currentTime)
                it = m_keyToExpirationTime.erase(it);
            else
                ++it;
        }
    }
};

} // namespace nx::utils::math
