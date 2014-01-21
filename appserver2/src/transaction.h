
#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <QString>
#include <vector>
#include "serialization_helper.h"

namespace ec2
{

    struct ApiData;

    enum class ApiCommand
    {
        addCamera,
        removeCamera
    };

    template<class T>
    class QnTransaction
    {
    public:
        struct ID
        {
            GUID serverGUID;
            qint64 number;
        };

        QnTransaction( const QnTransaction&& xvalue );

        ID id;

        bool persistent;
        ApiCommand command;
        T params;
    };


    struct ApiData {
        virtual ~ApiData() {}
    };

    struct ApiResourceData: public ApiData {
        qint32        id;
        QString       guid;
        qint32        typeId;
        qint32        parentId;
        QString       name;
        QString       url;
        int           status; // enum Status
        bool         disabled;
    };

    struct ApiCameraData: public ApiResourceData {
        bool scheduleDisabled;
        int motionType;

        template <class T> void serialize(BinaryStream<T>& stream);
        //QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS ( (scheduleDisabled) (motionType) )
    };

    namespace detail {
        QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS (ec2::ApiResourceData, (id) (guid) (typeId) (parentId) (name) (url) (status) (disabled) )
        QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS (ec2::ApiCameraData, (scheduleDisabled) (motionType) )
    }

    template <class T>
    void ApiCameraData::serialize(BinaryStream<T>& stream)
    {
        //serialize( (ApiResourceData*) this, &stream );
        detail::serialize( *((ApiCameraData*) this), &stream );
    }


}

void test()
{
    ec2::ApiCameraData data;
    BinaryStream<QByteArray> stream;
    data.serialize(stream);
}

//QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS( QnTransaction, ... )

/*
template<class T>
void serialize( Transaction, BinaryStream<T> );

template<class T>
void deserialize( BinaryStream<T> , Transaction * );

void serialize( Transaction, QJsonValue * );
void deserialize( const QJsonValue &, Transaction * );
*/

#endif  //EC2_TRANSACTION_H
