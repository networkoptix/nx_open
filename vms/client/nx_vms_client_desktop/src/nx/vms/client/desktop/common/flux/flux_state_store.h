// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedValueRollback>

namespace nx::vms::client::desktop {

template<typename S>
class FluxStateStore
{
    bool check(S&& s) { m_state = std::move(s); return true; }
    bool check(std::pair<bool, S>&& p) { m_state = std::move(p.second); return p.first; }

public:
    template <typename ... Args>
    FluxStateStore(Args&& ... args) : m_state(std::forward<Args>(args)...) {}

    typedef std::function<void(const S&)> StateChangedCallback;

    void subscribe(StateChangedCallback stateChangedCallback)
    {
        m_stateChangedCallback = stateChangedCallback;
    }

    const S& state() const { return m_state; }

    template <typename F, typename ... Args>
    void dispatch(F&& f, Args&& ... args)
    {
        // Chained actions are forbidden.
        if (m_actionInProgress)
            return;

        QScopedValueRollback<bool> guard(m_actionInProgress, true);
        if (check(f(std::move(m_state), std::forward<Args>(args)...)) && m_stateChangedCallback)
            m_stateChangedCallback(m_state);
    }

    template <typename R, typename ... Args>
    auto dispatchTo(R(*f)(S s, Args ...))
    {
        return [this, f](Args ... args) {
            dispatch(f, std::forward<Args>(args)...);
        };
    }

private:
    S m_state;
    StateChangedCallback m_stateChangedCallback;
    bool m_actionInProgress = false;
};

} // namespace nx::vms::client::desktop
