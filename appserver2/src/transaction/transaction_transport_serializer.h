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
        void serializeTran(QByteArray& buffer, const QnTransaction<T>& tran, const ProcessedPeers& peers)
        {
            OutputBinaryStream<QByteArray> stream(&buffer);
            stream.write("00000000\r\n",10);
            stream.write("0000", 4);
            serialize(peers, &stream);
            serialize( tran, &stream );
            stream.write("\r\n",2); // chunk end
            quint32 payloadSize = buffer.size() - 12;
            quint32* payloadSizePtr = (quint32*) (buffer.data() + 4);
            *payloadSizePtr = htonl(payloadSize);
            toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
        }

        void serializeTran(QByteArray& buffer, const QByteArray& serializedTran, const ProcessedPeers& peers)
        {
            OutputBinaryStream<QByteArray> stream(&buffer);
            stream.write("00000000\r\n",10);
            stream.write("0000", 4);
            serialize(peers, &stream);
            stream.write(serializedTran.data(), serializedTran.size());
            stream.write("\r\n",2); // chunk end
            quint32 payloadSize = buffer.size() - 12;
            quint32* payloadSizePtr = (quint32*) (buffer.data() + 4);
            *payloadSizePtr = htonl(payloadSize);
            toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
        }

        
        static bool deserializeTran(const quint8* chunkPayload, int len,  ProcessedPeers& peers, QByteArray& tranData);

    private:
        static void toFormattedHex(quint8* dst, quint32 payloadSize);
    private:
        QnTransactionMessageBus& m_owner;
    };
}



#endif // __TRANSACTION_TRANSPORT_SERIALIZER_H_
