// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

// TODO: Used to link the unit test with the plugin. Rewrite.
#if defined(_WIN32)
    #pragma warning(disable: 4251) //< Disable warnings "...  needs to have dll-interface...".
    #pragma warning(disable: 4275) //< Disable warnings "...  non dll-interface class...".
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
