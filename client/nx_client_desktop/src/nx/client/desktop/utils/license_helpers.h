#pragma once

#include <functional>

#include <QtCore/QString>

namespace nx {
namespace client {
namespace desktop {
namespace helpers {
namespace license {

enum class DeactivationResult
{
    Success,
    UnspecifiedError,
    InvalidParameters,
    TransportProblem,
    ServerError,
    DeactivationError
};
using DeactivationHandler = std::function<void(DeactivationResult)>;

void deactivateAsync(
    const QString& hwid,
    const DeactivationHandler& completionHandler = DeactivationHandler());

} // license namespace
} // helpers namespace
} // desktop namespace
} // client namespace
} // nx namespace
