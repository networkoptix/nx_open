#include "interruption_flag.h"

namespace nx::network::aio {

void InterruptionFlag::handleAioThreadChange()
{
    constexpr auto kAioThreadChanged =
        (base_type::ControlledObjectState) (base_type::ControlledObjectState::customState + 1);

    recordCustomState(kAioThreadChanged);
}

} // namespace nx::network::aio
