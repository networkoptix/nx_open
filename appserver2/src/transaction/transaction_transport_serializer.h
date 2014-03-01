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

    class QnTransactionTransportSerializer
    {
    public:

        QnTransactionTransportSerializer(QnTransactionMessageBus& owner);

        template <class T>
        void serialize(QByteArray& buffer, const QnTransaction<T>& tran, const ProcessedPeers& opaque = ProcessedPeers())
        {
            OutputBinaryStream<QByteArray> stream(&buffer);
            stream.write("00000000\r\n",10);
            QnBinary::serialize(updateOpaque(opaque), &stream);
            tran.serialize(&stream);
            stream.write("\r\n",2); // chunk end
            quint32 payloadSize = buffer.size() - 12;
            toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
        }

        void serialize(QByteArray& buffer, const QByteArray& serializedTran, const ProcessedPeers& opaque = ProcessedPeers())
        {
            OutputBinaryStream<QByteArray> stream(&buffer);
            stream.write("00000000\r\n",10);
            QnBinary::serialize(updateOpaque(opaque), &stream);
            stream.write(serializedTran.data(), serializedTran.size());
            stream.write("\r\n",2); // chunk end
            quint32 payloadSize = buffer.size() - 12;
            toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
        }

        
        static bool deserialize(const quint8* chunkPayload, int len,  ProcessedPeers& peers, QByteArray& tranData);

    private:
        ProcessedPeers updateOpaque(const ProcessedPeers& opaque);
        static void toFormattedHex(quint8* dst, quint32 payloadSize);
    private:
        QnTransactionMessageBus& m_owner;
    };
}



#endif // __TRANSACTION_TRANSPORT_SERIALIZER_H_
