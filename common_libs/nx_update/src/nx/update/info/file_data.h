#pragma once

#include <QtCore>

namespace nx {
namespace update {
namespace info {

struct FileData
{
    QString file;
    QString url;
    qint64 size;
    QByteArray md5;
};

} // namespace info
} // namespace update
} // namespace nx
