#pragma once

#include <utility>

namespace nx {
namespace gstreamer {

template<typename GuardFunc>
class ScopeGuard
{
public:
    explicit ScopeGuard(GuardFunc guardFunc):
        m_guardFunc(std::move(guardFunc))
    {
    }

    ScopeGuard(ScopeGuard&& other):
        m_guardFunc(std::move(other.m_guardFunc)),
        m_fired(other.m_fired)
    {
        other.m_fired = true;
    }

    ScopeGuard(const ScopeGuard&) = delete;
    void operator=(const ScopeGuard&) = delete;

    ~ScopeGuard()
    {
        m_guardFunc();
    }

    void release()
    {
        m_fired = true;
    }

private:
    GuardFunc m_guardFunc;
    bool m_fired = false;
};

template<typename GuardFunc>
ScopeGuard<GuardFunc> makeScopeGuard(GuardFunc&& guardFunc)
{
    return ScopeGuard<GuardFunc>(guardFunc);
}

} // namespace gstreamer
} // namespace nx
