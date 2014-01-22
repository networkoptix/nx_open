#ifndef __RESOURCE_TRANSACTION_DATA_H__
#define __RESOURCE_TRANSACTION_DATA_H__

#include "api_data.h"
#include "../../serialization_helper.h"

#include <QTCore/qglobal.h>
#include <QString>
#include <vector>


namespace ec2
{


enum Status {
    Offline      = 0,
    Unauthorized = 1,
    Online       = 2,
    Recording    = 3
};

struct ApiResourceData: public ApiData {
    qint32        id;
    QString       guid;
    qint32        typeId;
    qint32        parentId;
    QString       name;
    QString       url;
    Status        status;
    bool          disabled;

    QN_DECLARE_STRUCT_SERIALIZATORS();
};

}

QN_DEFINE_STRUCT_SERIALIZATORS (ec2::ApiResourceData, (id) (guid) (typeId) (parentId) (name) (url) (status) (disabled) )

#endif // __RESOURCE_TRANSACTION_DATA_H__
