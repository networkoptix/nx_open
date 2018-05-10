#pragma once

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

struct ApiStoredFileData: ApiData
{
    QString path;
    QByteArray data;
};
#define ApiStoredFileData_Fields (path)(data)

struct ApiStoredFilePath: ApiData
{
    ApiStoredFilePath() {}
    ApiStoredFilePath(const QString& path): path(path) {}

    bool operator<(const ApiStoredFilePath& other) const
    {
        return path < other.path;
    }

    QString path;
};
#define ApiStoredFilePath_Fields (path)

} // namespace ec2
