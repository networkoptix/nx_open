#include "protocol_handler.h"

#include <QtCore/QDebug>

namespace nx
{
    namespace utils
    {
        bool registerSystemUriProtocolHandler(const QString& protocol, const QString& applicationBinaryPath)
        {
            qDebug() << "Register protocol" << protocol << applicationBinaryPath;
            return true;
        }
    }
}
