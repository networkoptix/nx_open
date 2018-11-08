#include "timed_command.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace core {
namespace ptz {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TimedCommand, (eq), PtzTimedCommand_fields);

} // namespace ptz
} // namespace core
} // namespace nx
