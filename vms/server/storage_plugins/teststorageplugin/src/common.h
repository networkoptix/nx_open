#pragma once

#include <atomic>

template <typename P>
class PluginRefCounter
{
public:
    PluginRefCounter()
        : m_count(1)
    {}

    int pAddRef() { return ++m_count; }

    int pReleaseRef()
    {
        int new_count = --m_count;
        if (new_count <= 0) 
            delete static_cast<P*>(this);

        return new_count;
    }
private:
    std::atomic<int> m_count;
};
