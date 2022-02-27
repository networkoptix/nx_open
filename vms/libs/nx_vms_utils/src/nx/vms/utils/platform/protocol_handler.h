// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/utils/software_version.h>

namespace nx {
namespace vms {
namespace utils {

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX) || defined(Q_OS_MAC)

struct RegisterSystemUriProtocolHandlerResult
{
    bool success = false;
    nx::utils::SoftwareVersion previousVersion;
};

    /**
     * Try to register system uri protocol handler.
     * @returns true if the handler registered successfully (or was already registered), false otherwise.
     */
NX_VMS_UTILS_API RegisterSystemUriProtocolHandlerResult registerSystemUriProtocolHandler(
    const QString& protocol,
    const QString& applicationBinaryPath,
    const QString& applicationName,
    const QString& description,
    const QString& customization,
    const nx::utils::SoftwareVersion& version);
#endif

#if defined(Q_OS_WIN)
NX_VMS_UTILS_API bool runAsAdministratorOnWindows(const QString& applicationBinaryPath,
    const QStringList& parameters);
#endif


} // namespace utils
} // namespace vms
} // namespace nx
