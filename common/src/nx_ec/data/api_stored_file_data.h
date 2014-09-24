#ifndef EC2_STORED_FILE_DATA_H
#define EC2_STORED_FILE_DATA_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiStoredFileData: ApiData
    {
        QString path;
        QByteArray data;
    };
#define ApiStoredFileData_Fields (path)(data)

    struct ApiStoredFilePath: ApiData
    {
        ApiStoredFilePath() {}
        ApiStoredFilePath(const QString &path): path(path) {}

        QString path;
    };
#define ApiStoredFilePath_Fields (path)

}

#endif //EC2_STORED_FILE_DATA_H
