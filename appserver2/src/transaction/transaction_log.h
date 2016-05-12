#ifndef __TRANSACTION_LOG_H_
#define __TRANSACTION_LOG_H_

#include <QtCore/QSet>
#include <QtCore/QElapsedTimer>
#include <nx/utils/thread/mutex.h>

#include "transaction_descriptor.h"

namespace ec2
{
    static const char ADD_HASH_DATA[] = "$$_HASH_$$";

    namespace detail { class QnDbManager; }


    class QnTransactionLog
    {
    public:

        enum ContainsReason {
            Reason_None,
            Reason_Sequence,
            Reason_Timestamp
        };

        QnTransactionLog(detail::QnDbManager* db);
        virtual ~QnTransactionLog();

        static QnTransactionLog* instance();

        ErrorCode getTransactionsAfter(const QnTranState& state, QList<QByteArray>& result);
        QnTranState getTransactionsState();

        bool contains(const QnTranState& state) const;

        template <typename T>
        ErrorCode saveTransaction(const QnTransaction<T>& tran)
        {
            auto tdBase = getTransactionDescriptorByValue(tran.command);
            auto td = dynamic_cast<detail::TransactionDescriptor<T>*>(tdBase);
            NX_ASSERT(td, "Downcast to TransactionDescriptor<TransactionParams>* failed");
            if (td == nullptr)
                return ErrorCode::notImplemented;
            return td->saveFunc(tran, this);
        }

        template <typename T>
        ErrorCode saveTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            auto tdBase = getTransactionDescriptorByValue(tran.command);
            auto td = dynamic_cast<detail::TransactionDescriptor<T>*>(tdBase);
            NX_ASSERT(td, "Downcast to TransactionDescriptor<TransactionParams>* failed");
            if (td == nullptr)
                return ErrorCode::notImplemented;
            return td->saveSerializedFunc(tran, serializedTran, this);
            return ErrorCode::ok;
        }

        qint64 getTimeStamp();
        bool init();

        int getLatestSequence(const QnTranStateKey& key) const;
        static QnUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray());
        static QnUuid makeHash(const QByteArray &extraData, const ApiDiscoveryData &data);

         /**
         *  Semantics of the transactionHash() function is following:
         *  if transaction A follows transaction B and overrides it,
         *  their transactionHash() result MUST be the same. Otherwise, transactionHash() result must differ.
         *  Obviously, transactionHash() is not needed for the non-persistent transaction.
         */

        template<typename Param>
        QnUuid transactionHash(const Param &param)
        {
            for (auto it = detail::transactionDescriptors.get<0>().begin();
                 it != detail::transactionDescriptors.get<0>().end(); ++it)
            {
                auto tdBase = (*it).get();
                auto td = dynamic_cast<detail::TransactionDescriptor<Param>*>(tdBase);
                if (td)
                    return td->getHashFunc(param);
            }
            NX_ASSERT(0, "Transaciton descriptor for the given param not found");
            return QnUuid();
        }

        ErrorCode updateSequence(const ApiUpdateSequenceData& data);
        void fillPersistentInfo(QnAbstractTransaction& tran);

        void beginTran();
        void commit();
        void rollback();

        qint64 getTransactionLogTime() const;
        void setTransactionLogTime(qint64 value);
        ErrorCode saveToDB(const QnAbstractTransaction& tranID, const QnUuid& hash, const QByteArray& data);
    private:
        friend class detail::QnDbManager;

        template <class T>
        ContainsReason contains(const QnTransaction<T>& tran) { return contains(tran, transactionHash(tran.params)); }
        ContainsReason contains(const QnAbstractTransaction& tran, const QnUuid& hash) const;

        int currentSequenceNoLock() const;

        ErrorCode updateSequenceNoLock(const QnUuid& peerID, const QnUuid& dbID, int sequence);
    private:
        struct UpdateHistoryData
        {
            UpdateHistoryData(): timestamp(0) {}
            UpdateHistoryData(const QnTranStateKey& updatedBy, const qint64& timestamp): updatedBy(updatedBy), timestamp(timestamp) {}
            QnTranStateKey updatedBy;
            qint64 timestamp;
        };
        struct CommitData
        {
            CommitData() {}
            void clear() { state.values.clear(); updateHistory.clear(); }

            QnTranState state;
            QMap<QnUuid, UpdateHistoryData> updateHistory;
        };
    private:
        detail::QnDbManager* m_dbManager;
        QnTranState m_state;
        QMap<QnUuid, UpdateHistoryData> m_updateHistory;

        mutable QnMutex m_timeMutex;
        QElapsedTimer m_relativeTimer;
        qint64 m_baseTime;
        qint64 m_lastTimestamp;
        CommitData m_commitData;
    };
};

#define transactionLog QnTransactionLog::instance()

#endif // __TRANSACTION_LOG_H_
