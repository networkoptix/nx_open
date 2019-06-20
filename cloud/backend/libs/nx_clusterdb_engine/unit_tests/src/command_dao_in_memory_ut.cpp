#include <gtest/gtest.h>

#include <nx/sql/db_connection_holder.h>
#include <nx/sql/test_support/test_with_db_helper.h>
#include <nx/utils/test_support/utils.h>

#include <nx/clusterdb/engine/dao/memory/command_data_object_in_memory.h>

#include "customer_db/data.h"

namespace nx::clusterdb::engine::dao::memory::test {

static constexpr int kProtoVersion = 1;

class TransactionDataObjectInMemory:
    public ::testing::Test,
    public nx::sql::test::TestWithDbHelper
{
public:
    TransactionDataObjectInMemory():
        nx::sql::test::TestWithDbHelper("clusterdb_engine_ut", ""),
        m_peerGuid(QnUuid::createUuid()),
        m_peerDbId(QnUuid::createUuid()),
        m_systemId(QnUuid::createUuid().toSimpleByteArray().toStdString()),
        m_transactionDataObject(kProtoVersion),
        m_lastAddedCommand(engine::test::command::SaveCustomer::code, m_peerGuid),
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
            m_lastAddedCommand = transaction;
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
            m_lastAddedCommand = transaction;
        }
    }

    void verifyThatOnlyLastOneIsPresent()
    {
        const std::vector<clusterdb::engine::dao::TransactionLogRecord>
            transactions = readAllTransaction();

        ASSERT_EQ(1U, transactions.size());
        ASSERT_EQ(
            QnUbjson::serialized(m_lastAddedCommand),
            transactions[0].serializer->serialize(Qn::UbjsonFormat, kProtoVersion));
    }

    void verifyThatDataObjectIsEmpty()
    {
        const std::vector<clusterdb::engine::dao::TransactionLogRecord> transactions =
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
    const std::string m_systemId;
    std::int64_t m_peerSequence = 0;
    clusterdb::engine::dao::memory::TransactionDataObject m_transactionDataObject;
    engine::test::command::SaveCustomer::Data m_commandData;
    clusterdb::engine::Command<engine::test::command::SaveCustomer::Data> m_lastAddedCommand;
    nx::sql::DbConnectionHolder m_dbConnectionHolder;
    std::shared_ptr<nx::sql::QueryContext> m_currentTran;

    void init()
    {
        engine::test::Customer customer;
        customer.id = QnUuid::createUuid().toSimpleString().toStdString();
        customer.fullName = QnUuid::createUuid().toSimpleString().toStdString();
        customer.address = QnUuid::createUuid().toSimpleString().toStdString();
    }

    template<typename CommandDataType>
    void saveTransaction(const clusterdb::engine::Command<CommandDataType>& command)
    {
        const auto tranHash =
            engine::test::command::SaveCustomer::hash(command.params);
        const auto ubjsonSerializedTransaction = QnUbjson::serialized(command);
        clusterdb::engine::dao::TransactionData transactionData{
            m_systemId,
            command,
            tranHash,
            ubjsonSerializedTransaction};

        const auto dbResult = m_transactionDataObject.insertOrReplaceTransaction(
            m_currentTran ? m_currentTran.get() : nullptr,
            transactionData);
        ASSERT_EQ(nx::sql::DBResult::ok, dbResult);
    }

    clusterdb::engine::Command<engine::test::command::SaveCustomer::Data> generateTransaction()
    {
        clusterdb::engine::Command<engine::test::command::SaveCustomer::Data> command(
            engine::test::command::SaveCustomer::code,
            m_peerGuid);
        command.persistentInfo.dbID = m_peerDbId;
        command.persistentInfo.sequence = m_peerSequence++;
        command.params = m_commandData;

        return command;
    }

    std::vector<clusterdb::engine::dao::TransactionLogRecord> readAllTransaction()
    {
        std::vector<clusterdb::engine::dao::TransactionLogRecord> transactions;
        const auto resultCode = m_transactionDataObject.fetchTransactionsOfAPeerQuery(
            m_currentTran ? m_currentTran.get() : nullptr,
            m_systemId,
            m_peerGuid.toSimpleString(),
            m_peerDbId.toSimpleString(),
            -1,
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

} // namespace nx::clusterdb::engine::dao::memory::test
