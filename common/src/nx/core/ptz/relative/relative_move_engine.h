#pragma once

#include <nx/core/ptz/vector.h>
#include <nx/core/ptz/options.h>

namespace nx {
namespace core {
namespace ptz {

class RelativeMoveEngine
{
public:
    virtual bool relativeMove(
        const nx::core::ptz::Vector& direction,
        const nx::core::ptz::Options& options) = 0;

    virtual bool relativeFocus(qreal direction, const nx::core::ptz::Options& options) = 0;
};

} // namespace ptz
} // namespaace core
} // namespace nx
