#pragma once

#include <nx/core/ptz/timed_command.h>

namespace nx {
namespace core {
namespace ptz {

class SequenceExecutor
{
public:
    virtual bool executeSequence(const CommandSequence& sequence) = 0;
};

} // namespace ptz
} // namespace core
} // namespace nx
