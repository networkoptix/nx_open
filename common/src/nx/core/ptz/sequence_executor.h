#pragma once

#include <core/ptz/abstract_ptz_controller.h>
#include <nx/core/ptz/timed_command.h>

namespace nx {
namespace core {
namespace ptz {

class SequenceExecutor
{
public:
    SequenceExecutor(QnAbstractPtzController* controller);

    bool executeSequence(const CommandSequence& sequence);

private:
    QnAbstractPtzController* m_controller;
};

} // namespace ptz
} // namespace core
} // namespace nx
