#include <gtest/gtest.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <nx/utils/crc32.h>

#include <nx/cloud/cdb/managers/temporary_account_password_manager.h>
#include <nx/cloud/cdb/stree/cdb_ns.h>
#include <nx/cloud/cdb/test_support/base_persistent_data_test.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>

namespace nx {
namespace cdb {
namespace test {

class TemporaryAccountPasswordManager:
    public ::testing::Test,
    public BasePersistentDataTest
{
public:
    TemporaryAccountPasswordManager():
        m_tempPasswordManager(
            m_attrNameset,
            &persistentDbManager()->queryExecutor())
    {
        m_email = BusinessDataGenerator::generateRandomEmailAddress();
    }

protected:
    void generateTemporaryCredentials()
    {
        m_credentials.accountEmail = m_email;
        m_tempPasswordManager.addRandomCredentials(m_email, &m_credentials);
    }

    void assertCredentialsLoginContainsAccountEmailCrc32()
    {
        std::vector<std::string> splitVec;
        boost::split(splitVec, m_credentials.login, boost::algorithm::is_any_of("-"));
        ASSERT_EQ(2U, splitVec.size());
        ASSERT_EQ(std::to_string(nx::utils::crc32(m_email)), splitVec[1]);
    }

private:
    CdbAttrNameSet m_attrNameset;
    cdb::TemporaryAccountPasswordManager m_tempPasswordManager;
    data::TemporaryAccountCredentials m_credentials;
    std::string m_email;
};

TEST_F(TemporaryAccountPasswordManager, temporary_login_is_equal_to_account_email)
{
    generateTemporaryCredentials();
    assertCredentialsLoginContainsAccountEmailCrc32();
}

} // namespace test
} // namespace cdb
} // namespace nx
