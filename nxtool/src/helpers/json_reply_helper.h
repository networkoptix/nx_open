#pragma once

#include <QtCore/QByteArray>

namespace rtu
{
    namespace helpers
    {
        int getJsonReplyError(const QByteArray &jsonData, QString *errorString = nullptr);
    }
}
