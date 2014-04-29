#ifndef __TRANSACTION_TRANSPORT_SERIALIZER_H_
#define __TRANSACTION_TRANSPORT_SERIALIZER_H_

#include "transaction.h"
#include "common/common_module.h"
#include "utils/serialization/binary_stream.h"
#include "transaction_transport_header.h"

namespace ec2
{
    QN_FUSION_DECLARE_FUNCTIONS(TransactionTransportHeader, (binary)(json))

    class QnTransactionMessageBus;

    /**
     * This class add addition transport header to a transaction.
     */
    class QnTransactionTransportSerializer
    {
    public:
        QnTransactionTransportSerializer(QnTransactionMessageBus& owner);

        template <class T>
        void serializeTran(QByteArray& buffer, const QnTransaction<T>& tran, const TransactionTransportHeader& ttHeader)
        {
            foreach (const QnId& peer, ttHeader.dstPeers) {
                Q_ASSERT(!peer.isNull());
                Q_ASSERT(peer != qnCommon->moduleGUID());
            }
            QnOutputBinaryStream<QByteArray> stream(&buffer);
            stream.write("00000000\r\n",10);
            stream.write("0000", 4);
            QnBinary::serialize(ttHeader, &stream);
            QnBinary::serialize( tran, &stream );
            stream.write("\r\n",2); // chunk end
            quint32 payloadSize = buffer.size() - 12;
            quint32* payloadSizePtr = (quint32*) (buffer.data() + 10);
            *payloadSizePtr = htonl(payloadSize);
            toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
        }

        void serializeTran(QByteArray& buffer, const QByteArray& serializedTran, const TransactionTransportHeader& ttHeader)
        {
            foreach (const QnId& peer, ttHeader.dstPeers) {
                Q_ASSERT(!peer.isNull());
                Q_ASSERT(peer != qnCommon->moduleGUID());
            }
            QnOutputBinaryStream<QByteArray> stream(&buffer);
            stream.write("00000000\r\n",10);
            stream.write("0000", 4);
            QnBinary::serialize(ttHeader, &stream);
            stream.write(serializedTran.data(), serializedTran.size());
            stream.write("\r\n",2); // chunk end
            quint32 payloadSize = buffer.size() - 12;
            quint32* payloadSizePtr = (quint32*) (buffer.data() + 10);
            *payloadSizePtr = htonl(payloadSize);
            toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
        }

        static bool deserializeTran(const quint8* chunkPayload, int len,  TransactionTransportHeader& transportHeader, QByteArray& tranData);

    private:
        static void toFormattedHex(quint8* dst, quint32 payloadSize);

    private:
        QnTransactionMessageBus& m_owner;
    };
}

#endif // __TRANSACTION_TRANSPORT_SERIALIZER_H_
