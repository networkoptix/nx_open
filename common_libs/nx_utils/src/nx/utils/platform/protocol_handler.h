#pragma once

#include <QtCore/QString>

namespace nx
{
    namespace utils
    {
#ifdef Q_OS_WIN

        /**
         * Try to register system uri protocol handler.
         * @returns true if the handler registered successfully (or was already registered), false otherwise.
         */
        NX_UTILS_API bool registerSystemUriProtocolHandler(const QString& protocol, const QString& applicationBinaryPath, const QString& description);
        NX_UTILS_API bool runAsAdministratorWithUAC(const QString &applicationBinaryPath, const QStringList &parameters);
#endif
    }
}
