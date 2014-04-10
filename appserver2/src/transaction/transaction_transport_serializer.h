#ifndef __TRANSACTION_TRANSPORT_SERIALIZER_H_
#define __TRANSACTION_TRANSPORT_SERIALIZER_H_

#include "transaction.h"

/*
* This class add addition transport header to a transaction
*/

namespace ec2
{
    class QnTransactionMessageBus;

    typedef QSet<QnId> ProcessedPeers;
    //using namespace ::QnBinary;


    class QnTransactionTransportSerializer
    {
    public:

        QnTransactionTransportSerializer(QnTransactionMessageBus& owner);

        template <class T>
        void serializeTran(QByteArray& buffer, const QnTransaction<T>& tran, const ProcessedPeers& peers = ProcessedPeers())
        {
            OutputBinaryStream<QByteArray> stream(&buffer);
            stream.write("0000",4);
            serialize(updatePeers(peers), &stream);
            serialize( tran, &stream );
            quint32* lenField = (quint32*) buffer.data();
            *lenField = htonl(buffer.size());
        }

        void serializeTran(QByteArray& buffer, const QByteArray& serializedTran, const ProcessedPeers& peers = ProcessedPeers())
        {
            OutputBinaryStream<QByteArray> stream(&buffer);
            stream.write("0000", 4);
            serialize(updatePeers(peers), &stream);
            stream.write(serializedTran.data(), serializedTran.size());
            quint32* lenField = (quint32*) buffer.data();
            *lenField = htonl(buffer.size());
        }

        
        static bool deserializeTran(const quint8* chunkPayload, int len,  ProcessedPeers& peers, QByteArray& tranData);

    private:
        ProcessedPeers updatePeers(const ProcessedPeers& opaque);
        static void toFormattedHex(quint8* dst, quint32 payloadSize);
    private:
        QnTransactionMessageBus& m_owner;
    };
}



#endif // __TRANSACTION_TRANSPORT_SERIALIZER_H_
