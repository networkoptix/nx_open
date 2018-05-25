#include "protocol_detector.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace network {

std::string toString(ProtocolMatchResult detectionResult)
{
    switch (detectionResult)
    {
        case ProtocolMatchResult::detected:
            return "detected";
        case ProtocolMatchResult::needMoreData:
            return "needMoreData";
        case ProtocolMatchResult::unknownProtocol:
            return "unknownProtocol";
    }

    NX_ASSERT(false);
    return "unsupported value";
}

} // namespace network
} // namespace nx
