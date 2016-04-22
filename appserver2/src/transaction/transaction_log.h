#ifndef __TRANSACTION_LOG_H_
#define __TRANSACTION_LOG_H_

#include <QtCore/QSet>
#include <QtCore/QElapsedTimer>
#include <nx/utils/thread/mutex.h>

#include "transaction_descriptor.h"

namespace ec2
{
    static const char ADD_HASH_DATA[] = "$$_HASH_$$";

    class QnDbManager;


    class QnTransactionLog
    {
    public:

        enum ContainsReason {
            Reason_None,
            Reason_Sequence,
            Reason_Timestamp
        };

        QnTransactionLog(QnDbManager* db);
        virtual ~QnTransactionLog();

        static QnTransactionLog* instance();

        ErrorCode getTransactionsAfter(const QnTranState& state, QList<QByteArray>& result);
        QnTranState getTransactionsState();

        bool contains(const QnTranState& state) const;

        template<typename Param>
        struct GenericTransactionDescriptorSaveVisitor
        {
            template<typename Descriptor>
            void operator ()(const Descriptor &d)
            {
                m_errorCode = d.saveFunc(m_tran, &m_tlog);
            }

            GenericTransactionDescriptorSaveVisitor(const QnTransaction<Param> &tran, QnTransactionLog &tlog)
                : m_tran(tran),
                  m_tlog(tlog),
                  m_errorCode(ErrorCode::notImplemented)
            {}

            ErrorCode getError() const { return m_errorCode; }
        private:
            const QnTransaction<Param> &m_tran;
            QnTransactionLog &m_tlog;
            ErrorCode m_errorCode;
        };

        template<typename Param>
        struct GenericTransactionDescriptorSaveSerializedVisitor
        {
            template<typename Descriptor>
            void operator ()(const Descriptor &d)
            {
                m_errorCode = d.saveSerializedFunc(m_tran, m_serializedTran, &m_tlog);
            }

            GenericTransactionDescriptorSaveSerializedVisitor(const QnTransaction<Param> &tran, const QByteArray &serializedTran, QnTransactionLog &tlog)
                : m_tran(tran),
                  m_serializedTran(serializedTran),
                  m_tlog(tlog),
                  m_errorCode(ErrorCode::notImplemented)
            {}

            ErrorCode getError() const { return m_errorCode; }
        private:
            const QnTransaction<Param> &m_tran;
            const QByteArray &m_serializedTran;
            QnTransactionLog &m_tlog;
            ErrorCode m_errorCode;
        };

        template <typename T>
        ErrorCode saveTransaction(const QnTransaction<T>& tran)
        {
            auto filteredDescriptors = ec2::getTransactionDescriptorsFilteredByTransactionParams<T>();
            static_assert(std::tuple_size<decltype(filteredDescriptors)>::value, "Should be at least one Transaction descriptor to proceed");
            GenericTransactionDescriptorSaveVisitor<T> visitor(tran, *this);
            visitTransactionDescriptorIfValue(tran.command, visitor, filteredDescriptors);

            return visitor.getError();
        }

        template <typename T>
        ErrorCode saveTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            auto filteredDescriptors = ec2::getTransactionDescriptorsFilteredByTransactionParams<T>();
            static_assert(std::tuple_size<decltype(filteredDescriptors)>::value, "Should be at least one Transaction descriptor to proceed");
            GenericTransactionDescriptorSaveSerializedVisitor<T> visitor(tran, serializedTran, *this);
            visitTransactionDescriptorIfValue(tran.command, visitor, filteredDescriptors);

            return visitor.getError();
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
            auto descriptor = getTransactionDescriptorByTransactionParams<Param>();
            return descriptor.getHashFunc(param);
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
        friend class QnDbManager;

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
        QnDbManager* m_dbManager;
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
