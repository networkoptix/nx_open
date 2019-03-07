#pragma once

#include <nx/utils/object_destruction_flag.h>

namespace nx::network::aio {

/**
 * Watches for object destruction and change of AIO thread.
 * TODO: #ak This class appears to be redundant.
 */
class NX_NETWORK_API InterruptionFlag:
    public nx::utils::ObjectDestructionFlag
{
    using base_type = nx::utils::ObjectDestructionFlag;

public:
    void handleAioThreadChange();
    void interrupt();
};

} // namespace nx::network::aio
