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
    qint64 offset;
};
#define ApiUpdateUploadResponceData_Fields ApiIdData_Fields (updateId)(offset)

}

#endif // EC2_UPDATE_DATA_H
