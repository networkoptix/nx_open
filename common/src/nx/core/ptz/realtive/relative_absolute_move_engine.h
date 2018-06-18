#pragma once

#include <nx/core/ptz/realtive/relative_move_engine.h>

namespace nx {
namespace core {
namespace ptz {

class RelativeAbsoluteMoveEngine: public RelativeMoveEngine
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
