#pragma once

#include <QtCore/QString>

namespace nx
{
    namespace utils
    {
#ifdef Q_OS_WIN
        NX_UTILS_API bool registerSystemUriProtocolHandler(const QString& protocol, const QString& applicationBinaryPath);
#endif
    }
}
