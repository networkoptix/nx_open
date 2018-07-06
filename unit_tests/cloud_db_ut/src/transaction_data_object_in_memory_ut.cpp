#include <gtest/gtest.h>

#include <nx/utils/test_support/utils.h>

#include <nx_ec/data/api_user_data.h>
#include <nx/sql/detail/request_execution_thread.h>

#include <nx/data_sync_engine/dao/memory/transaction_data_object_in_memory.h>
#include <nx/cloud/cdb/ec2/data_conversion.h>
#include <nx/cloud/cdb/test_support/base_persistent_data_test.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>

#include <transaction/transaction_descriptor.h>

namespace nx {
namespace cdb {
namespace ec2 {
namespace dao {
namespace memory {
namespace test {

class TransactionDataObjectInMemory:
    public ::testing::Test,
    public nx::cdb::test::BasePersistentDataTest
{
public:
    TransactionDataObjectInMemory():
        m_peerGuid(QnUuid::createUuid()),
        m_peerDbId(QnUuid::createUuid()),
        m_systemId(QnUuid::createUuid().toSimpleByteArray()),
        m_peerSequence(0),
        m_lastAddedTransaction(m_peerGuid),
        m_dbConnectionHolder(dbConnectionOptions())
    {
        init();
    }

protected:
    void havingAddedMultipleTransactionsWithSameHash()
    {
        for (int i = 0; i < 2; ++i)
        {
            const auto transaction = generateTransaction();
            saveTransaction(transaction);
            m_lastAddedTransaction = transaction;
        }
    }

    void havingAddedMultipleTransactionsWithSameSequenceFromSamePeer()
    {
        int sequence = -1;

        for (int i = 0; i < 2; ++i)
        {
            auto transaction = generateTransaction();
            if (sequence == -1)
                sequence = transaction.persistentInfo.sequence;
            else
                transaction.persistentInfo.sequence = sequence;
            saveTransaction(transaction);
            m_lastAddedTransaction = transaction;
        }
    }

    void verifyThatOnlyLastOneIsPresent()
    {
        const std::vector<data_sync_engine::dao::TransactionLogRecord> transactions = readAllTransaction();

        ASSERT_EQ(1U, transactions.size());
        ASSERT_EQ(
            QnUbjson::serialized(m_lastAddedTransaction),
            transactions[0].serializer->serialize(Qn::UbjsonFormat, nx_ec::EC2_PROTO_VERSION));
    }

    void verifyThatDataObjectIsEmpty()
    {
        const std::vector<data_sync_engine::dao::TransactionLogRecord> transactions =
            readAllTransaction();
        ASSERT_EQ(0U, transactions.size());
    }

    void beginTran()
    {
        m_currentTran = m_dbConnectionHolder.begin();
    }

    void rollbackTran()
    {
        if (m_currentTran)
        {
            m_currentTran->transaction()->rollback();
            m_currentTran.reset();
        }
    }

private:
    const QnUuid m_peerGuid;
    const QnUuid m_peerDbId;
    const nx::String m_systemId;
    std::int64_t m_peerSequence;
    data_sync_engine::dao::memory::TransactionDataObject m_transactionDataObject;
    ::ec2::ApiUserData m_transactionData;
    data_sync_engine::Command<::ec2::ApiUserData> m_lastAddedTransaction;
    nx::sql::DbConnectionHolder m_dbConnectionHolder;
    std::shared_ptr<nx::sql::QueryContext> m_currentTran;

    void init()
    {
        const auto sharing =
            cdb::test::BusinessDataGenerator::generateRandomSharing(
                cdb::test::BusinessDataGenerator::generateRandomAccount(),
                m_systemId.toStdString());
        ec2::convert(sharing, &m_transactionData);
    }

    template<typename TransactionDataType>
    void saveTransaction(const data_sync_engine::Command<TransactionDataType>& transaction)
    {
        const auto tranHash = ::ec2::transactionHash(transaction.command, transaction.params).toSimpleByteArray();
        const auto ubjsonSerializedTransaction = QnUbjson::serialized(transaction);
        data_sync_engine::dao::TransactionData transactionData{
            m_systemId,
            transaction,
            tranHash,
            ubjsonSerializedTransaction};

        const auto dbResult = m_transactionDataObject.insertOrReplaceTransaction(
            m_currentTran ? m_currentTran.get() : nullptr,
            transactionData);
        ASSERT_EQ(nx::sql::DBResult::ok, dbResult);
    }

    data_sync_engine::Command<::ec2::ApiUserData> generateTransaction()
    {
        data_sync_engine::Command<::ec2::ApiUserData> transaction(m_peerGuid);
        transaction.command = ::ec2::ApiCommand::saveUser;
        transaction.persistentInfo.dbID = m_peerDbId;
        transaction.transactionType = ::ec2::TransactionType::Cloud;
        transaction.persistentInfo.sequence = m_peerSequence++;
        transaction.params = m_transactionData;

        return transaction;
    }

    std::vector<data_sync_engine::dao::TransactionLogRecord> readAllTransaction()
    {
        std::vector<data_sync_engine::dao::TransactionLogRecord> transactions;
        const auto resultCode = m_transactionDataObject.fetchTransactionsOfAPeerQuery(
            m_currentTran ? m_currentTran.get() : nullptr,
            m_systemId,
            m_peerGuid.toSimpleString(),
            m_peerDbId.toSimpleString(),
            0,
            std::numeric_limits<int64_t>::max(),
            &transactions);
        NX_GTEST_ASSERT_EQ(nx::sql::DBResult::ok, resultCode);
        return transactions;
    }
};

TEST_F(TransactionDataObjectInMemory, checking_tran_hash_unique_index)
{
    havingAddedMultipleTransactionsWithSameHash();
    verifyThatOnlyLastOneIsPresent();
}

TEST_F(TransactionDataObjectInMemory, checking_peer_guid_db_guid_sequence_unique_index)
{
    havingAddedMultipleTransactionsWithSameSequenceFromSamePeer();
    verifyThatOnlyLastOneIsPresent();
}

TEST_F(TransactionDataObjectInMemory, DISABLED_tran_rollback)
{
    beginTran();
    havingAddedMultipleTransactionsWithSameHash();
    rollbackTran();
    verifyThatDataObjectIsEmpty();
}

} // namespace test
} // namespace memory
} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
