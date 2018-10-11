#pragma once

#include <nx/core/ptz/timed_command.h>

namespace nx {
namespace core {
namespace ptz {

class AbstractSequenceMaker
{
public:
    virtual ~AbstractSequenceMaker() = default;
    virtual CommandSequence makeSequence(
        const Vector& relativeMove,
        const Options& options) const = 0;
};

} // namespace ptz
} // namespace core
} // namespace nx
