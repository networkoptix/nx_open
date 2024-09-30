// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API StoredFileData
{
    QString path;
    QByteArray data;

    bool operator==(const StoredFileData& other) const = default;

    const QString& getId() const { return path; }
};
#define StoredFileData_Fields (path)(data)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(StoredFileData)
NX_REFLECTION_INSTRUMENT(StoredFileData, StoredFileData_Fields)

struct NX_VMS_API StoredFilePath
{
    StoredFilePath() = default;
    StoredFilePath(const QString& path): path(path) {}

    bool operator==(const StoredFilePath& other) const = default;

    bool operator<(const StoredFilePath& other) const
    {
        return path < other.path;
    }

    const QString& getId() const { return path; }

    QString path;
};
#define StoredFilePath_Fields (path)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(StoredFilePath)
NX_REFLECTION_INSTRUMENT(StoredFilePath, StoredFilePath_Fields)

} // namespace api
} // namespace vms
} // namespace nx
