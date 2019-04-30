#include <tuple>
#include <type_traits>

#include <gtest/gtest.h>

#include <nx/clusterdb/engine/command.h>
#include <nx/fusion/model_functions.h>
#include <transaction/transaction.h>

namespace nx::test {

template <typename Types>
class TypesCompatibility:
    public ::testing::Test
{
public:
    using One = typename Types::One;
    using Two = typename Types::Two;

protected:
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
    void assertCanBeRestoredFrom(const From& from, const To& to)
    {
        auto serialized = Format::serialized(from);
        auto deserialized = Format::deserialized<To>(serialized);

        ASSERT_EQ(to, deserialized);
    }
};

TYPED_TEST_CASE_P(TypesCompatibility);

TYPED_TEST_P(TypesCompatibility, ubjson_compatible)
{
    assertOneCanBeRestoredFromAnother<Ubjson, One, Two>();
    assertOneCanBeRestoredFromAnother<Ubjson, Two, One>();
}

TYPED_TEST_P(TypesCompatibility, DISABLED_json_compatible)
{
    assertOneCanBeRestoredFromAnother<Json, One, Two>();
    assertOneCanBeRestoredFromAnother<Json, Two, One>();
}

REGISTER_TYPED_TEST_CASE_P(TypesCompatibility,
    ubjson_compatible,
    DISABLED_json_compatible
);

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

    static void copy(Two* dest, const One& src)
    {
        dest->command = (int) src.command;
        dest->peerID = src.peerID;
        dest->transactionType = src.transactionType;
        dest->persistentInfo.dbID = src.persistentInfo.dbID;
        dest->persistentInfo.sequence = src.persistentInfo.sequence;
        dest->persistentInfo.timestamp = src.persistentInfo.timestamp;
        dest->historyAttributes.author = src.historyAttributes.author;
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    Transaction,
    TypesCompatibility,
    DataSyncTransactionTypes);

} // namespace nx::test
