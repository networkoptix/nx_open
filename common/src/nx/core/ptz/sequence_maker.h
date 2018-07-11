#pragma once

#include <nx/core/ptz/timed_command.h>

namespace nx {
namespace core {
namespace ptz {

class SequenceMaker
{
public:
    virtual CommandSequence makeSequence(
        const Vector& relativeMove,
        const Options& options) const = 0;
};

} // namespace ptz
} // namespace core
} // namespace nx
