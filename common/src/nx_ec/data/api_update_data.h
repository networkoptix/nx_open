#pragma once

#include "api_data.h"

namespace ec2 {

struct ApiUpdateUploadData: ApiData
{
    QString updateId;
    QByteArray data;
    qint64 offset;
};
#define ApiUpdateUploadData_Fields (updateId)(data)(offset)

struct ApiUpdateUploadResponceData: ApiIdData
{
public:
    QString updateId;
    int chunks;
};
#define ApiUpdateUploadResponceData_Fields IdData_Fields (updateId)(chunks)

struct ApiUpdateInstallData: ApiData
{
    ApiUpdateInstallData() {}
    ApiUpdateInstallData(const QString &updateId): updateId(updateId) {}

    QString updateId;
};
#define ApiUpdateInstallData_Fields (updateId)

} // namespace ec2
