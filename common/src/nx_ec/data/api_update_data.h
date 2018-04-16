#pragma once

#include "api_data.h"

namespace ec2 {

struct ApiUpdateUploadData: nx::vms::api::Data
{
    QString updateId;
    QByteArray data;
    qint64 offset = 0;
};
#define ApiUpdateUploadData_Fields (updateId)(data)(offset)

struct ApiUpdateUploadResponceData: nx::vms::api::IdData
{
public:
    QString updateId;
    int chunks;
};
#define ApiUpdateUploadResponceData_Fields IdData_Fields (updateId)(chunks)

struct ApiUpdateInstallData: nx::vms::api::Data
{
    ApiUpdateInstallData() = default;
    ApiUpdateInstallData(const QString &updateId): updateId(updateId) {}

    QString updateId;
};
#define ApiUpdateInstallData_Fields (updateId)

} // namespace ec2
