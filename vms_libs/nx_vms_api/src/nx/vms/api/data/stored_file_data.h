#pragma once

#include "data.h"

#include <QtCore/QString>
#include <QtCore/QByteArray>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API StoredFileData: Data
{
    QString path;
    QByteArray data;
};
#define StoredFileData_Fields (path)(data)

struct NX_VMS_API StoredFilePath: Data
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

Q_DECLARE_METATYPE(nx::vms::api::StoredFileData)
Q_DECLARE_METATYPE(nx::vms::api::StoredFileDataList)
Q_DECLARE_METATYPE(nx::vms::api::StoredFilePath)
Q_DECLARE_METATYPE(nx::vms::api::StoredFilePathList)
