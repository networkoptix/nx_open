
#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <QString>
#include <vector>
#include "serialization_helper.h"

namespace ec2
{

    enum class ApiCommand
    {
        addCamera,
        removeCamera
    };

    template <class T>
    class QnTransaction
    {
    public:
        struct ID
        {
            GUID peerGUID;
            qint64 number;
        };

        QnTransaction( const QnTransaction& xvalue );

        ID id;
        ApiCommand command;
        bool persistent;
        
        T params;
    };

}

#if 1
#include "nx_ec/data/camera_data.h"
void test()
{
    ec2::ApiCameraData data;
    BinaryStream<QByteArray> stream;
    data.serialize(stream);
}
#endif

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
