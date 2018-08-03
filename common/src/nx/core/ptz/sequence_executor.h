#pragma once

#include <functional>

#include <nx/core/ptz/timed_command.h>

namespace nx {
namespace core {
namespace ptz {

using SequenceExecutedCallback = std::function<void()>;

class AbstractSequenceExecutor
{
public:
    virtual bool executeSequence(
        const CommandSequence& sequence,
        SequenceExecutedCallback sequenceExecutedCallback) = 0;
    virtual ~AbstractSequenceExecutor() {}
};

} // namespace ptz
} // namespace core
} // namespace nx
