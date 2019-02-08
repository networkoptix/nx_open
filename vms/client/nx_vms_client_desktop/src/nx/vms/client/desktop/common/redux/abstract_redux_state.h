#pragma once

namespace nx::vms::client::desktop {

/**
 * Base class for redux states. It's main concept is making the class move-only.
 */
struct AbstractReduxState
{
    AbstractReduxState() = default;
    AbstractReduxState(const AbstractReduxState& other) = delete;
    AbstractReduxState(AbstractReduxState&& other) = default;
    AbstractReduxState& operator=(const AbstractReduxState&) = delete;
    AbstractReduxState& operator=(AbstractReduxState&&) = default;
    ~AbstractReduxState() = default;
};

} // namespace nx::vms::client::desktop
