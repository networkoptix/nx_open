
#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <QString>
#include <vector>
#include "serialization_helper.h"

namespace ec2
{

    enum ApiCommand
    {
        addCamera,
        removeCamera,

        saveMediaServer
    };

    struct QnUuid {
        quint32 data1;
        quint16 data2;
        quint16 data3;
        qint64  data4;

        QN_DECLARE_STRUCT_SERIALIZATORS();
    };

    class QnAbstractTransaction
    {
    public:
        void createNewID();
        
        static void setPeerGuid(const QnUuid& value);
        static void setStartNumber(const qint64& value);

        struct ID
        {
            QnUuid peerGUID;
            qint64 number;

            QN_DECLARE_STRUCT_SERIALIZATORS();
        };

        ApiCommand command;
        ID id;
        bool persistent;

        QN_DECLARE_STRUCT_SERIALIZATORS();
    private:
        static QnUuid m_staticPeerGUID;
        static qint64 m_staticNumber;
        static QMutex m_mutex;
    };

    template <class T>
    class QnTransaction: public QnAbstractTransaction
    {
    public:
        QnTransaction() {}
        
        T params;

        template <class T2>
        void serialize(BinaryStream<T2> stream) {
            QnAbstractTransaction::serialize(stream);
            params.serialize(stream);
        }
    };
}

QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnUuid, (data1) (data2) (data3) (data4) )
QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnAbstractTransaction::ID, (peerGUID) (number) )
QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnAbstractTransaction, (command) (id) (persistent))

#if 1
#include "nx_ec/data/camera_data.h"
static void test()
{
    ec2::ApiCameraData data;
    BinaryStream<QByteArray> stream;
    data.serialize(stream);
    data.deserialize(stream);

    ec2::QnTransaction<ec2::ApiCameraData> tran;
    tran.serialize(stream);
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
