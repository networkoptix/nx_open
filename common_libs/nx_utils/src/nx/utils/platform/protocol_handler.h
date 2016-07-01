#pragma once

#include <QtCore/QString>

namespace nx {
namespace utils {

class SoftwareVersion;

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)

    /**
     * Try to register system uri protocol handler.
     * @returns true if the handler registered successfully (or was already registered), false otherwise.
     */
NX_UTILS_API bool registerSystemUriProtocolHandler(
    const QString& protocol,
    const QString& applicationBinaryPath,
    const QString& applicationName,
    const QString& description,
    const QString& customization,
    const SoftwareVersion& version);
#endif

#ifdef Q_OS_WIN
NX_UTILS_API bool runAsAdministratorWithUAC(const QString& applicationBinaryPath, const QStringList& parameters);
#endif

} // namespace utils
} // namespace nx
