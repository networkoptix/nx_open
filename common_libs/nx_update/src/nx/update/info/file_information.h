#pragma once

#include <QtCore>

namespace nx {
namespace update {
namespace info {

struct FileInformation
{
    QString name;
    qint64 size;
    QByteArray md5;
};

} // namespace info
} // namespace update
} // namespace nx
