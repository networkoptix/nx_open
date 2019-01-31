#pragma once

#include <functional>

#include <nx/core/ptz/vector.h>
#include <nx/core/ptz/options.h>

namespace nx {
namespace core {
namespace ptz {

using RelativeMoveDoneCallback = std::function<void()>;

class RelativeMoveEngine
{
public:
    virtual ~RelativeMoveEngine() = default;

    virtual bool relativeMove(
        const nx::core::ptz::Vector& direction,
        const nx::core::ptz::Options& options,
        RelativeMoveDoneCallback doneCallback) = 0;

    virtual bool relativeFocus(
        qreal direction,
        const nx::core::ptz::Options& options,
        RelativeMoveDoneCallback doneCallback) = 0;
};

} // namespace ptz
} // namespaace core
} // namespace nx
