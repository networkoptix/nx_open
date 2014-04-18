#ifndef EC2_UPDATE_DATA_H
#define EC2_UPDATE_DATA_H

#include "api_data.h"

#include <nx_ec/binary_serialization_helper.h>

namespace ec2 {

class ApiUpdateUploadData : public ApiData {
public:
    QString updateId;
    QByteArray data;
};
QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(ApiUpdateUploadData, (updateId) (data))

class ApiUpdateUploadResponceData : public ApiIdData {
public:
    QString updateId;
};
QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(ApiUpdateUploadResponceData, (updateId) (id))

typedef QString ApiUpdateInstallData;

}

#endif // EC2_UPDATE_DATA_H
