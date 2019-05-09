#include <tuple>
#include <type_traits>

#include <gtest/gtest.h>

#include <nx/clusterdb/engine/command.h>
#include <nx/clusterdb/engine/command_data.h>
#include <nx/fusion/model_functions.h>
#include <transaction/transaction.h>

namespace nx::test {

    struct Json
    {
        template<typename... Args>
        static auto serialized(Args&& ... args)
        {
            return QJson::serialized(std::forward<Args>(args)...);
        }

        template<typename T>
        static auto deserialized(const QByteArray& buf)
        {
            return QJson::deserialized<T>(buf);
        }
    };

    struct Ubjson
    {
        template<typename... Args>
        static auto serialized(Args&&... args)
        {
            return QnUbjson::serialized(std::forward<Args>(args)...);
        }

        template<typename T>
        static auto deserialized(const QByteArray& buf)
        {
            return QnUbjson::deserialized<T>(buf);
        }
    };

template <typename Types>
class TypesCompatibility:
    public ::testing::Test
{
public:
    using One = typename Types::One;
    using Two = typename Types::Two;

protected:
    std::tuple<One, Two> generate()
    {
        One one;
        Two two;
        Types::copy(&two, one);
        return std::make_tuple(one, two);
    }

    template<typename Format, typename From, typename To>
    void assertOneCanBeRestoredFromAnother()
    {
        auto [one, two] = this->generate();

        if constexpr (std::is_same<From, One>::value)
            assertCanBeRestoredFrom<Format>(one, two);
        else
            assertCanBeRestoredFrom<Format>(two, one);
    }

private:
    template<typename Format, typename From, typename To>
    void assertCanBeRestoredFrom(const From& from, const To& expected)
    {
        const auto actual = Format::template deserialized<To>(
            Format::serialized(from));

        ASSERT_EQ(expected, actual);
    }
};

TYPED_TEST_CASE_P(TypesCompatibility);

TYPED_TEST_P(TypesCompatibility, ubjson_compatible)
{
    this->template assertOneCanBeRestoredFromAnother<
        Ubjson, typename TypeParam::One, typename TypeParam::Two>();
    
    this->template assertOneCanBeRestoredFromAnother<
        Ubjson, typename TypeParam::Two, typename TypeParam::One>();
}

TYPED_TEST_P(TypesCompatibility, DISABLED_json_compatible)
{
    this->template assertOneCanBeRestoredFromAnother<
        Json, typename TypeParam::One, typename TypeParam::Two>();
    
    this->template assertOneCanBeRestoredFromAnother<
        Json, typename TypeParam::Two, typename TypeParam::One>();
}

REGISTER_TYPED_TEST_CASE_P(TypesCompatibility,
    ubjson_compatible,
    DISABLED_json_compatible
);

//-------------------------------------------------------------------------------------------------

struct DataSyncTimestampTypes
{
    class One:
        public nx::vms::api::Timestamp
    {
    public:
        One(): nx::vms::api::Timestamp(rand(), rand()) {}
    };

    class Two:
        public nx::clusterdb::engine::Timestamp
    {
    public:
        Two(): nx::clusterdb::engine::Timestamp(rand(), rand()) {}
    };

    static void copy(
        nx::clusterdb::engine::Timestamp* dest,
        const nx::vms::api::Timestamp& src)
    {
        dest->sequence = src.sequence;
        dest->ticks = src.ticks;
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    Timestamp,
    TypesCompatibility,
    DataSyncTimestampTypes);

//-------------------------------------------------------------------------------------------------

struct DataSyncTransactionTypes
{
    class One:
        public ::ec2::QnAbstractTransaction
    {
    public:
        One():
            ::ec2::QnAbstractTransaction(
                ::ec2::ApiCommand::addLicense,
                QnUuid::createUuid())
        {
        }
    };
    
    class Two:
        public nx::clusterdb::engine::CommandHeader
    {
    public:
        Two():
            nx::clusterdb::engine::CommandHeader(
                ::ec2::ApiCommand::addLicense,
                QnUuid::createUuid())
        {
        }
    };

    static void copy(
        nx::clusterdb::engine::CommandHeader* dest,
        const ::ec2::QnAbstractTransaction& src)
    {
        dest->command = (int) src.command;
        dest->peerID = src.peerID;
        dest->transactionType = src.transactionType;
        dest->persistentInfo.dbID = src.persistentInfo.dbID;
        dest->persistentInfo.sequence = src.persistentInfo.sequence;
        DataSyncTimestampTypes::copy(&dest->persistentInfo.timestamp, src.persistentInfo.timestamp);
        dest->historyAttributes.author = src.historyAttributes.author;
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    Transaction,
    TypesCompatibility,
    DataSyncTransactionTypes);

//-------------------------------------------------------------------------------------------------

struct DataSyncTransactionDataTypes
{
    using One = ::ec2::ApiTransactionData;
    using Two = nx::clusterdb::engine::CommandData;

    static void copy(Two* dest, const One& src)
    {
        dest->dataSize = src.dataSize;
        DataSyncTransactionTypes::copy(&dest->tran, src.tran);
        dest->tranGuid = src.tranGuid;
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    TransactionData,
    TypesCompatibility,
    DataSyncTransactionDataTypes);

} // namespace nx::test
