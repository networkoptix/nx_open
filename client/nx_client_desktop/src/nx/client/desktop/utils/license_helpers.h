#pragma once

#include <functional>

#include <QtCore/QHash>
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

    ServerError,
    DeactivationError
};

enum class DeactivationError
{
    NoError,
    UnknownError,

    LicenseDeactivatedAlready
};

using LicenseKeyErrorHash = QHash<QByteArray, DeactivationError>;
using DeactivationHandler = std::function<void(DeactivationResult, const LicenseKeyErrorHash&)>;

void deactivateAsync(
    const QnLicenseList& licenses,
    const DeactivationHandler& completionHandler = DeactivationHandler());

} // license namespace
} // helpers namespace
} // desktop namespace
} // client namespace
} // nx namespace
