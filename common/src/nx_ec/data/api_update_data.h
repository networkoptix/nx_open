#ifndef EC2_UPDATE_DATA_H
#define EC2_UPDATE_DATA_H

#include "api_data.h"

namespace ec2 {

struct ApiUpdateUploadData : public ApiData {
    QString updateId;
    QByteArray data;
    qint64 offset;
};
#define ApiUpdateUploadData_Fields (updateId)(data)(offset)

struct ApiUpdateUploadResponceData : public ApiIdData {
public:
    QString updateId;
    int chunks;
};
#define ApiUpdateUploadResponceData_Fields ApiIdData_Fields (updateId)(chunks)

struct ApiUpdateInstallData: public ApiData {

    ApiUpdateInstallData() {}
    ApiUpdateInstallData(const QString &updateId): updateId(updateId) {}

    QString updateId;
};
#define ApiUpdateInstallData_Fields (updateId)

}

#endif // EC2_UPDATE_DATA_H
