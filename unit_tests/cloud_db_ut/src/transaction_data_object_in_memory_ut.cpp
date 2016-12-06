#include <gtest/gtest.h>

#include <nx_ec/data/api_user_data.h>
#include <transaction/transaction_descriptor.h>

#include <ec2/dao/memory/transaction_data_object_in_memory.h>
#include <ec2/data_conversion.h>
#include <test_support/business_data_generator.h>

namespace nx {
namespace cdb {
namespace ec2 {
namespace dao {
namespace memory {
namespace test {

class TransactionDataObjectInMemory:
    public ::testing::Test
{
public:
    TransactionDataObjectInMemory():
        m_peerGuid(QnUuid::createUuid()),
        m_peerDbId(QnUuid::createUuid()),
        m_systemId(QnUuid::createUuid().toSimpleByteArray()),
        m_peerSequence(0),
        m_lastAddedTransaction(m_peerGuid)
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
        boost::optional<int> sequence;

        for (int i = 0; i < 2; ++i)
        {
            auto transaction = generateTransaction();
            if (sequence)
                transaction.persistentInfo.sequence = *sequence;
            else
                sequence = transaction.persistentInfo.sequence;
            saveTransaction(transaction);
            m_lastAddedTransaction = transaction;
        }
    }

    void expectThatOnlyLastOneIsPresent()
    {
        std::vector<dao::TransactionLogRecord> transactions;

        const auto resultCode = m_transactionDataObject.fetchTransactionsOfAPeerQuery(
            nullptr,
            m_systemId,
            m_peerGuid.toSimpleString(),
            m_peerDbId.toSimpleString(),
            0,
            std::numeric_limits<int64_t>::max(),
            &transactions);
        ASSERT_EQ(db::DBResult::ok, resultCode);

        ASSERT_EQ(1U, transactions.size());
        ASSERT_EQ(
            QnUbjson::serialized(m_lastAddedTransaction),
            transactions[0].serializer->serialize(Qn::UbjsonFormat, nx_ec::EC2_PROTO_VERSION));
    }

private:
    const QnUuid m_peerGuid;
    const QnUuid m_peerDbId;
    const nx::String m_systemId;
    std::int64_t m_peerSequence;
    ec2::dao::memory::TransactionDataObject m_transactionDataObject;
    ::ec2::ApiUserData m_transactionData;
    ::ec2::QnTransaction<::ec2::ApiUserData> m_lastAddedTransaction;

    void init()
    {
        const auto sharing =
            cdb::test::BusinessDataGenerator::generateRandomSharing(
                cdb::test::BusinessDataGenerator::generateRandomAccount(),
                m_systemId.toStdString());
        ec2::convert(sharing, &m_transactionData);
    }

    template<typename TransactionDataType>
    void saveTransaction(const ::ec2::QnTransaction<TransactionDataType>& transaction)
    {
        const auto tranHash = ::ec2::transactionHash(transaction.params).toSimpleByteArray();
        const auto ubjsonSerializedTransaction = QnUbjson::serialized(transaction);
        TransactionData transactionData{
            m_systemId,
            transaction,
            tranHash,
            ubjsonSerializedTransaction};

        const auto dbResult = m_transactionDataObject.insertOrReplaceTransaction(
            nullptr,
            transactionData);
        ASSERT_EQ(db::DBResult::ok, dbResult);
    }

    ::ec2::QnTransaction<::ec2::ApiUserData> generateTransaction()
    {
        ::ec2::QnTransaction<::ec2::ApiUserData> transaction(m_peerGuid);
        transaction.command = ::ec2::ApiCommand::saveUser;
        transaction.persistentInfo.dbID = m_peerDbId;
        transaction.transactionType = ::ec2::TransactionType::Cloud;
        transaction.persistentInfo.sequence = m_peerSequence++;
        transaction.params = m_transactionData;

        return transaction;
    }
};

TEST_F(TransactionDataObjectInMemory, checking_tran_hash_unique_index)
{
    havingAddedMultipleTransactionsWithSameHash();
    expectThatOnlyLastOneIsPresent();
}

TEST_F(TransactionDataObjectInMemory, checking_peer_guid_db_guid_sequence_unique_index)
{
    havingAddedMultipleTransactionsWithSameSequenceFromSamePeer();
    expectThatOnlyLastOneIsPresent();
}

} // namespace test
} // namespace memory
} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
