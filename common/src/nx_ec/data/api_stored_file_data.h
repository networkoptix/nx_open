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

}

#endif //EC2_STORED_FILE_DATA_H
