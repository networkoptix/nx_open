// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "progress_state.h"

namespace nx::vms::client::desktop {

ProgressState::ProgressState(State state):
    m_state(state)
{
}

ProgressState::ProgressState(qreal value):
    m_state(value)
{
}

bool ProgressState::isCompleted() const
{
    if (auto state = std::get_if<State>(&m_state))
        return *state == completed;

    return false;
}

bool ProgressState::isFailed() const
{
    if (auto state = std::get_if<State>(&m_state))
        return *state == failed;

    return false;
}

bool ProgressState::isIndefinite() const
{
    if (auto state = std::get_if<State>(&m_state))
        return *state == indefinite;

    return false;
}

std::optional<qreal> ProgressState::value() const
{
    if (auto value = std::get_if<qreal>(&m_state))
        return *value;

    return {};
}

} // namespace nx::vms::client::desktop
