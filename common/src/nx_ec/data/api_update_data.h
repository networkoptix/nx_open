#ifndef EC2_UPDATE_DATA_H
#define EC2_UPDATE_DATA_H

#include "api_data.h"

namespace ec2 {

struct ApiUpdateUploadData : public ApiData {
    QString updateId;
    QByteArray data;
};
#define ApiUpdateUploadData_Fields ApiData_Fields (updateId)(data)

struct ApiUpdateUploadResponceData : public ApiIdData {
public:
    QString updateId;
};
#define ApiUpdateUploadResponceData_Fields ApiIdData_Fields (updateId)

}

#endif // EC2_UPDATE_DATA_H
