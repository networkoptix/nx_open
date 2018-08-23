#pragma once

#include <nx/core/ptz/sequence_maker.h>
#include <nx/core/ptz/relative/relative_continuous_move_mapping.h>

namespace nx {
namespace core {
namespace ptz {

class RelativeContinuousMoveSequenceMaker: public AbstractSequenceMaker
{
public:
    RelativeContinuousMoveSequenceMaker(const RelativeContinuousMoveMapping& mapping);

    virtual CommandSequence makeSequence(
        const Vector& relativeMoveVector,
        const Options& options) const override;

private:
    RelativeContinuousMoveMapping m_mapping;
};

} // namespace ptz
} // namespace core
} // namespace nx
