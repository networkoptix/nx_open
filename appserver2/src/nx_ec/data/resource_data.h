#ifndef __RESOURCE_TRANSACTION_DATA_H__
#define __RESOURCE_TRANSACTION_DATA_H__

#include "api_data.h"
#include "../../serialization_helper.h"

#include <QTCore/qglobal.h>
#include <QString>
#include <vector>

#define QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(TYPE, BASE_TYPE, FIELD_SEQ, ... /* PREFIX */) \
    QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ); \
    template <class T> \
    void TYPE::serialize(BinaryStream<T>& stream) \
    { \
        BASE_TYPE::serialize(stream); \
        QnBinary::serialize(*this, &stream); \
    } \
    \
    template <class T> \
    void TYPE::deserialize(BinaryStream<T>& stream) \
    { \
        BASE_TYPE::deserialize(stream); \
        QnBinary::deserialize(*this, &stream); \
    }

#define QN_DEFINE_STRUCT_SERIALIZATORS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ); \
    template <class T> \
    void TYPE::serialize(BinaryStream<T>& stream) \
    { \
        QnBinary::serialize(*this, &stream); \
    } \
    \
    template <class T> \
    void TYPE::deserialize(BinaryStream<T>& stream) \
    { \
        QnBinary::deserialize(*this, &stream); \
    }


#define QN_DECLARE_STRUCT_SERIALIZATORS() \
    template <class T> void serialize(BinaryStream<T>& stream); \
    template <class T> void deserialize(BinaryStream<T>& stream); \


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
