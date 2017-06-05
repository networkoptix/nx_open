#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>

#include <database/migrations/add_history_attributes_to_transaction.h>
#include <nx_ec/data/api_data.h>
#include <nx_ec/data/api_resource_data.h>
#include <nx_ec/data/api_user_data.h>
#include <transaction/transaction.h>

namespace nx {
namespace cdb {
namespace ec2 {

namespace compatibility {

//-------------------------------------------------------------------------------------------------
// ApiIdData

struct ApiIdData
{
    QnUuid id;
};

#define ApiIdData_compatibility_Fields (id)

//-------------------------------------------------------------------------------------------------
// ApiResourceParamWithRefData

struct ApiResourceParamWithRefData
{
    QString value;
    QString name;
    QnUuid resourceId;
};

#define ApiResourceParamWithRefData_compatibility_Fields (value)(name)(resourceId)

//-------------------------------------------------------------------------------------------------
// ApiResourceData

struct ApiResourceData: ApiIdData
{
    ApiResourceData() {}

    QnUuid parentId;
    QString name;
    QString url;
    QnUuid typeId;
};
#define ApiResourceData_compatibility_Fields ApiIdData_compatibility_Fields (parentId)(name)(url)(typeId)

//-------------------------------------------------------------------------------------------------
// ApiUserData

struct ApiUserData: ApiResourceData
{
    bool isAdmin;
    Qn::GlobalPermissions permissions;
    QnUuid userRoleId;
    QString email;
    QnLatin1Array digest;
    QnLatin1Array hash;
    QnLatin1Array cryptSha512Hash;
    QString realm;
    bool isLdap;
    bool isEnabled;
    bool isCloud;
    QString fullName;
};

#define ApiUserData_compatibility_Fields ApiResourceData_compatibility_Fields \
    (isAdmin) \
    (permissions) \
    (email) \
    (digest) \
    (hash) \
    (cryptSha512Hash) \
    (realm) \
    (isLdap) \
    (isEnabled) \
    (userRoleId) \
    (isCloud) \
    (fullName)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiIdData)(ApiResourceParamWithRefData)(ApiResourceData)(ApiUserData),
    (ubjson),
    _compatibility_Fields)

} // namespace

class TransactionVersioning:
    public ::testing::Test
{
public:
    template<typename One, typename Two>
    void testTwoWayStructureRelevance()
    {
        testOneWayStructureRelevance<One, Two>();
        testOneWayStructureRelevance<Two, One>();
    }

    template<typename TypeToSerialize, typename TypeToDeserialize>
    void testOneWayStructureRelevance()
    {
        TypeToSerialize source;
        const auto sourceSerialized = QnUbjson::serialized(source);

        bool isSucceeded = false;
        QnUbjson::deserialized<TypeToDeserialize>(
            sourceSerialized, TypeToDeserialize(), &isSucceeded);
        ASSERT_TRUE(isSucceeded);
    }
};

struct QnAbstractTransaction:
    public ::ec2::QnAbstractTransaction
{
    QnAbstractTransaction():
        ::ec2::QnAbstractTransaction(QnUuid::createUuid())
    {
    }
};

/**
 * Failure in a test here means migration has to be added.
 */

TEST_F(TransactionVersioning, QnAbstractTransaction_relevance)
{
    testTwoWayStructureRelevance<
        ::ec2::migration::add_history::after::QnAbstractTransaction,
        QnAbstractTransaction>();
}

TEST_F(TransactionVersioning, ApiUserData_relevance)
{
    testTwoWayStructureRelevance<::ec2::ApiUserData, compatibility::ApiUserData>();
}

TEST_F(TransactionVersioning, ApiIdData_relevance)
{
    testTwoWayStructureRelevance<::ec2::ApiIdData, compatibility::ApiIdData>();
}

TEST_F(TransactionVersioning, ApiResourceParamWithRefData_relevance)
{
    testTwoWayStructureRelevance<
        ::ec2::ApiResourceParamWithRefData,
        compatibility::ApiResourceParamWithRefData>();
}

} // namespace ec2
} // namespace cdb
} // namespace nx
