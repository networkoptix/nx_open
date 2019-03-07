#include "interruption_flag.h"

namespace nx::network::aio {

void InterruptionFlag::handleAioThreadChange()
{
    constexpr auto kAioThreadChanged =
        (base_type::ControlledObjectState) (base_type::ControlledObjectState::customState + 1);

    recordCustomState(kAioThreadChanged);
}

void InterruptionFlag::interrupt()
{
    constexpr auto kAioInterrupted =
        (base_type::ControlledObjectState) (base_type::ControlledObjectState::customState + 2);

    recordCustomState(kAioInterrupted);
}

} // namespace nx::network::aio
