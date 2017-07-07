#ifndef __TRANSACTION_LOG_H_
#define __TRANSACTION_LOG_H_

#include <QtCore/QSet>
#include <QtCore/QElapsedTimer>
#include <nx/utils/thread/mutex.h>

#include "transaction_descriptor.h"

#include <database/api/db_resource_api.h>

namespace ec2
{
    static const char ADD_HASH_DATA[] = "$$_HASH_$$";

    namespace detail { class QnDbManager; }
    enum class TransactionLockType;

    class QnUbjsonTransactionSerializer;

    enum class TransactionLockType
    {
        Regular, //< do commit as soon as commit() function called
        Lazy //< delay commit unless regular commit() called
    };

    class QnTransactionLog
    {
    public:

        enum ContainsReason {
            Reason_None,
            Reason_Sequence,
            Reason_Timestamp
        };

        QnTransactionLog(detail::QnDbManager* db, QnUbjsonTransactionSerializer* tranSerializer);
        virtual ~QnTransactionLog();

        QnUbjsonTransactionSerializer* serializer() const { return m_tranSerializer; }

        /**
         * Return all transactions from the log
         * @param state return transactions with sequence bigger then state,
         * for other peers all transactions are returned.
         * @param output result
         * @param onlyCloudData if false returns all transactions otherwise filter
         *        result and keep only cloud related transactions.
         */
        ErrorCode getTransactionsAfter(
            const QnTranState& state,
            bool onlyCloudData,
            QList<QByteArray>& result);

        /**
         * This function is similar to previous one but returns transactions included in state parameter only
         */
        ErrorCode getExactTransactionsAfter(
            QnTranState* inOutState,
            bool onlyCloudData,
            QList<QByteArray>& result,
            int maxDataSize,
            bool* outIsFinished);

        QnTranState getTransactionsState();

        // filter should contains sorted data
        QVector<qint32> getTransactionsState(const QVector<ApiPersistentIdData>& filter);

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

        Timestamp getTimeStamp();
        bool init();
        bool clear();

        int getLatestSequence(const ApiPersistentIdData& key) const;
        static QnUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray());
        static QnUuid makeHash(const QByteArray &extraData, const ApiDiscoveryData &data);

        ErrorCode updateSequence(const ApiUpdateSequenceData& data);
        ErrorCode updateSequence(const QnAbstractTransaction& tran, TransactionLockType lockType);
        void fillPersistentInfo(QnAbstractTransaction& tran);

        void beginTran();
        void commit();
        void rollback();

        Timestamp getTransactionLogTime() const;
        void setTransactionLogTime(Timestamp value);
        ErrorCode saveToDB(
            const QnAbstractTransaction &tranID,
            const QnUuid &hash,
            const QByteArray &data);
        void resetPreparedStatements();
    private:
        friend class detail::QnDbManager;
        ErrorCode updateSequenceNoLock(const QnUuid& peerID, const QnUuid& dbID, int sequence);

        template <class T>
        ContainsReason contains(const QnTransaction<T>& tran) { return contains(tran, transactionHash(tran.command, tran.params)); }
        ContainsReason contains(const QnAbstractTransaction& tran, const QnUuid& hash) const;

        int currentSequenceNoLock() const;

    private:
        struct UpdateHistoryData
        {
            UpdateHistoryData(): timestamp(Timestamp::fromInteger(0)) {}
            UpdateHistoryData(const ApiPersistentIdData& updatedBy, const Timestamp& timestamp): updatedBy(updatedBy), timestamp(timestamp) {}
            ApiPersistentIdData updatedBy;
            Timestamp timestamp;
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
        quint64 m_baseTime;
        Timestamp m_lastTimestamp;
        CommitData m_commitData;
        QnUbjsonTransactionSerializer* m_tranSerializer;
        ec2::database::api::QueryCache m_insertTransactionQuery;
        ec2::database::api::QueryCache m_updateSequenceQuery;
    };
};


#endif // __TRANSACTION_LOG_H_
