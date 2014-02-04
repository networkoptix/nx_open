
#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <QString>
#include <vector>
#include "nx_ec/binary_serialization_helper.h"


namespace ec2
{
    namespace ApiCommand
    {
        enum Value
        {
		    NotDefined,

            connect,

            getResourceTypes,

            addCamera,
		    updateCamera,
            removeCamera,
            getCameras,
            getCameraHistoryList,

            getMediaServerList,
            addMediaServer,
            updateMediaServer,

            getUserList,

            getBusinessRuleList
        };

        QString toString( Value val );
    }

    struct QnUuid {
		QnUuid(): data1(0), data2(0), data3(0), data4(0) {}

        quint32 data1;
        quint16 data2;
        quint16 data3;
        qint64  data4;

        QN_DECLARE_STRUCT_SERIALIZATORS();
    };

    class QnAbstractTransaction
    {
    public:
		QnAbstractTransaction(): command(ApiCommand::NotDefined), persistent(true) {}

        void createNewID();
        
        static void setPeerGuid(const QnUuid& value);
        static void setStartNumber(const qint64& value);

        struct ID
        {
			ID(): number(0) {}

            QnUuid peerGUID;
            qint64 number;

            QN_DECLARE_STRUCT_SERIALIZATORS();
        };

        ApiCommand::Value command;
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
        void serialize(OutputBinaryStream<T2>& stream) {
            QnAbstractTransaction::serialize(stream);
            params.serialize(stream);
        }
    };
}

QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnUuid, (data1) (data2) (data3) (data4) )
QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnAbstractTransaction::ID, (peerGUID) (number) )
QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnAbstractTransaction, (command) (id) (persistent))

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
