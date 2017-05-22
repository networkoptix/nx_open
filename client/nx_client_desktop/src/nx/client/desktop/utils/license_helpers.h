#pragma once

#include <functional>

#include <QtCore/QString>

#include <licensing/license_fwd.h>

namespace nx {
namespace client {
namespace desktop {
namespace helpers {
namespace license {

enum class DeactivationResult
{
    Success,
    UnspecifiedError,
    TransportProblem,
    ServerError,
    DeactivationError
};
using DeactivationHandler = std::function<void(DeactivationResult)>;

void deactivateAsync(
    const QnLicenseList& licenses,
    const DeactivationHandler& completionHandler = DeactivationHandler());

} // license namespace
} // helpers namespace
} // desktop namespace
} // client namespace
} // nx namespace
