#pragma once

#include <QtCore/QSet>
#include <QtCore/QElapsedTimer>
#include <nx/utils/thread/mutex.h>

#include <transaction/transaction_descriptor.h>
#include <transaction/ubjson_transaction_serializer.h>

#include <database/api/db_resource_api.h>

namespace ec2
{
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
            const nx::vms::api::TranState& state,
            bool onlyCloudData,
            QList<QByteArray>& result);

        /**
         * This function is similar to previous one but returns transactions included in state parameter only
         */
        ErrorCode getExactTransactionsAfter(
            nx::vms::api::TranState* inOutState,
            bool onlyCloudData,
            QList<QByteArray>& result,
            int maxDataSize,
            bool* outIsFinished);

        nx::vms::api::TranState getTransactionsState();

        // filter should contains sorted data
        QVector<qint32> getTransactionsState(const QVector<nx::vms::api::PersistentIdData>& filter);

        bool contains(const nx::vms::api::TranState& state) const;

        template <typename T>
        ErrorCode saveTransaction(const QnTransaction<T>& tran)
        {
            auto tdBase = getTransactionDescriptorByValue(tran.command);
            auto td = dynamic_cast<detail::TransactionDescriptor<T>*>(tdBase);
            NX_ASSERT(td, "Downcast to TransactionDescriptor<TransactionParams>* failed");
            if (td == nullptr)
                return ErrorCode::notImplemented;
            QByteArray serializedTran = serializer()->serializedTransaction(tran);
            return saveToDB(tran, td->getHashFunc(tran.params), serializedTran);
        }

        template <typename T>
        ErrorCode saveTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            auto tdBase = getTransactionDescriptorByValue(tran.command);
            auto td = dynamic_cast<detail::TransactionDescriptor<T>*>(tdBase);
            NX_ASSERT(td, "Downcast to TransactionDescriptor<TransactionParams>* failed");
            if (td == nullptr)
                return ErrorCode::notImplemented;
            return saveToDB(tran, td->getHashFunc(tran.params), serializedTran);
            return ErrorCode::ok;
        }

        nx::vms::api::Timestamp getTimeStamp();
        bool init();
        bool clear();

        int getLatestSequence(const nx::vms::api::PersistentIdData& key) const;

        ErrorCode updateSequence(const nx::vms::api::UpdateSequenceData& data);
        ErrorCode updateSequence(const QnAbstractTransaction& tran, TransactionLockType lockType);
        void fillPersistentInfo(QnAbstractTransaction& tran);

        void beginTran();
        void commit();
        void rollback();

        nx::vms::api::Timestamp getTransactionLogTime() const;
        void setTransactionLogTime(nx::vms::api::Timestamp value);
        ErrorCode saveToDB(
            const QnAbstractTransaction &tranID,
            const QnUuid &hash,
            const QByteArray &data);

    private:
        friend class detail::QnDbManager;
        ErrorCode updateSequenceNoLock(const QnUuid& peerID, const QnUuid& dbID, int sequence);

        template <class T>
        ContainsReason contains(const QnTransaction<T>& tran)
        {
            return contains(tran, transactionHash(tran.command, tran.params));
        }

        ContainsReason contains(const QnAbstractTransaction& tran, const QnUuid& hash) const;

        int currentSequenceNoLock() const;

    private:
        struct UpdateHistoryData
        {
            UpdateHistoryData() = default;
            UpdateHistoryData(const nx::vms::api::PersistentIdData& updatedBy,
                const nx::vms::api::Timestamp& timestamp);
            nx::vms::api::PersistentIdData updatedBy;
            nx::vms::api::Timestamp timestamp;
        };

        struct CommitData
        {
            CommitData() = default;
            void clear() { state.values.clear(); updateHistory.clear(); }
            nx::vms::api::TranState state;
            QMap<QnUuid, UpdateHistoryData> updateHistory;
        };

    private:
        detail::QnDbManager* m_dbManager;
        nx::vms::api::TranState m_state;
        QMap<QnUuid, UpdateHistoryData> m_updateHistory;

        mutable QnMutex m_timeMutex;
        QElapsedTimer m_relativeTimer;
        quint64 m_baseTime;
        nx::vms::api::Timestamp m_lastTimestamp;
        CommitData m_commitData;
        QnUbjsonTransactionSerializer* m_tranSerializer;
        ec2::database::api::QueryCache m_insertTransactionQuery;
        ec2::database::api::QueryCache m_updateSequenceQuery;
    };
};
