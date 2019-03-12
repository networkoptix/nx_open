#pragma once

#include <atomic>

// TODO: Used to link the unit test with the plugin. Rewrite.
#if defined(_WIN32)
    #define NX_TEST_STORAGE_PLUGIN_API __declspec(dllexport)
#else
    #define NX_TEST_STORAGE_PLUGIN_API /*empty*/
#endif

template <typename P>
class PluginRefCounter
{
public:
    PluginRefCounter()
        : m_count(1)
    {}

    int pAddRef() const { return ++m_count; }

    int pReleaseRef() const
    {
        int new_count = --m_count;
        if (new_count <= 0)
            delete static_cast<const P*>(this);

        return new_count;
    }
private:
    mutable std::atomic<int> m_count;
};
