#ifndef __BINARY_TRANSACTION_SERIALIZER_H_
#define __BINARY_TRANSACTION_SERIALIZER_H_

#include <QtCore/QCache>

#include <transaction/transaction.h>
#include <transaction/transaction_transport_header.h>

#include <utils/common/singleton.h>
#include <utils/common/model_functions.h>

namespace ec2
{
    /**
     * This class serialized a transaction into a byte array.
     */
    class QnBinaryTransactionSerializer: public Singleton<QnBinaryTransactionSerializer>
    {
    public:
        QnBinaryTransactionSerializer() {}

        template<class T>
        QByteArray serializedTransaction(const QnTransaction<T>& tran) {
            QMutexLocker lock(&m_mutex);
            Q_UNUSED(lock);

            // do not cache read-only transactions (they have sequence == 0)
            if (tran.id.sequence > 0 && m_cache.contains(tran.id))
                return *m_cache[tran.id];

            QByteArray* result = new QByteArray();
            QnOutputBinaryStream<QByteArray> stream(result);
            QnBinary::serialize( tran, &stream );

            m_cache.insert(tran.id, result);

            return *result;
        }

        template<class T>
        QByteArray serializedTransactionWithHeader(const QnTransaction<T> &tran, const QnTransactionTransportHeader &header) {
            QByteArray result;
            QnOutputBinaryStream<QByteArray> stream(&result);
            serializeHeader(stream, header);

            QByteArray serializedTran = serializedTransaction(tran);
            stream.write(serializedTran.data(), serializedTran.size());
            stream.write("\r\n",2); // chunk end
            serializePayload(result);
            return result;
        }

        static bool deserializeTran(const quint8* chunkPayload, int len,  QnTransactionTransportHeader& transportHeader, QByteArray& tranData);
    private:

        static void serializeHeader(QnOutputBinaryStream<QByteArray> &stream, const QnTransactionTransportHeader& ttHeader);
        static void serializePayload(QByteArray &buffer);
        static void toFormattedHex(quint8* dst, quint32 payloadSize);

    private:
        mutable QMutex m_mutex;
        QCache<QnAbstractTransaction::ID, QByteArray> m_cache;
    };

}


#endif // __BINARY_TRANSACTION_SERIALIZER_H_
