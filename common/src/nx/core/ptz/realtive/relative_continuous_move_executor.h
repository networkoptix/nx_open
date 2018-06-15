#pragma once

#include <nx/core/ptz/realtive/relative_move_executor.h>

namespace nx {
namespace core {
namespace ptz {

class RelativeContinuousMoveExecutor: public RelativeMoveExecutor
{
public:
    virtual bool relativeMove(
        const nx::core::ptz::Vector& direction,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeFocus(qreal direction, const nx::core::ptz::Options& options) override;
};

} // namespace ptz
} // namespace core
} // namespace nx
