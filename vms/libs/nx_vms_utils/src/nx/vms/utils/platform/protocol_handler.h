#pragma once

#include <QtCore/QString>

namespace nx {
namespace utils {

class SoftwareVersion;

} // namespace utils
} // namespace nx

namespace nx {
namespace vms {
namespace utils {

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX) || defined(Q_OS_MAC)

    /**
     * Try to register system uri protocol handler.
     * @returns true if the handler registered successfully (or was already registered), false otherwise.
     */
NX_VMS_UTILS_API bool registerSystemUriProtocolHandler(
    const QString& protocol,
    const QString& applicationBinaryPath,
    const QString& applicationName,
    const QString& description,
    const QString& customization,
    const nx::utils::SoftwareVersion& version);
#endif

#if defined(Q_OS_WIN)
NX_VMS_UTILS_API bool runAsAdministratorWithUAC(const QString& applicationBinaryPath, const QStringList& parameters);
#endif


} // namespace utils
} // namespace vms
} // namespace nx
