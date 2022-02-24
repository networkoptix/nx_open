// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::client::desktop {

/**
 * Base class for flux states. It's main concept is making the class move-only.
 */
struct AbstractFluxState
{
    AbstractFluxState() = default;
    AbstractFluxState(const AbstractFluxState& other) = delete;
    AbstractFluxState(AbstractFluxState&& other) = default;
    AbstractFluxState& operator=(const AbstractFluxState&) = delete;
    AbstractFluxState& operator=(AbstractFluxState&&) = default;
    ~AbstractFluxState() = default;
};

} // namespace nx::vms::client::desktop
