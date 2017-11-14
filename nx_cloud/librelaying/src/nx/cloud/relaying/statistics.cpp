#include "statistics.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cloud {
namespace relaying {

bool Statistics::operator==(const Statistics& right) const
{
    return
        listeningServerCount == right.listeningServerCount &&
        connectionsCount == right.connectionsCount &&
        connectionsAveragePerServerCount == right.connectionsAveragePerServerCount &&
        connectionsAcceptedPerMinute == right.connectionsAcceptedPerMinute;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _relaying_Fields)

} // namespace relaying
} // namespace cloud
} // namespace nx
