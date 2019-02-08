#pragma once

#include <QtCore/QScopedValueRollback>

namespace nx::vms::client::desktop {

template<class Owner, class State>
struct PrivateReduxStore
{
    Owner* const q;
    bool actionInProgress = false;
    State state;

    explicit PrivateReduxStore(Owner* owner):
        q(owner)
    {
    }

    void executeAction(std::function<State(State&&)> reduce)
    {
        // Chained actions are forbidden.
        if (actionInProgress)
            return;

        QScopedValueRollback<bool> guard(actionInProgress, true);
        state = reduce(std::move(state));
        emit q->stateChanged(state);
    }

    void executeAction(std::function<std::pair<bool, State>(State&&)> reduce)
    {
        // Chained actions are forbidden.
        if (actionInProgress)
            return;

        QScopedValueRollback<bool> guard(actionInProgress, true);

        bool changed = false;
        std::tie(changed, state) = reduce(std::move(state));

        if (changed)
            emit q->stateChanged(state);
    }
};

} // namespace nx::vms::client::desktop
