#include "protocol_detector.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace network {

std::string toString(DetectionResult detectionResult)
{
    switch (detectionResult)
    {
        case DetectionResult::detected:
            return "detected";
        case DetectionResult::needMoreData:
            return "needMoreData";
        case DetectionResult::unknownProtocol:
            return "unknownProtocol";
    }

    NX_ASSERT(false);
    return "unsupported value";
}

} // namespace network
} // namespace nx
