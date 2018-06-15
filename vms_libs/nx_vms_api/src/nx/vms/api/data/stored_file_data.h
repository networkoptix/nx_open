#pragma once

#include "data.h"

#include <QtCore/QString>
#include <QtCore/QByteArray>

namespace nx {
namespace vms {
namespace api {

struct StoredFileData: Data
{
    QString path;
    QByteArray data;
};
#define StoredFileData_Fields (path)(data)

struct StoredFilePath: Data
{
    StoredFilePath() = default;
    StoredFilePath(const QString& path): path(path) {}

    bool operator<(const StoredFilePath& other) const
    {
        return path < other.path;
    }

    QString path;
};
#define StoredFilePath_Fields (path)

} // namespace api
} // namespace vms
} // namespace nx
